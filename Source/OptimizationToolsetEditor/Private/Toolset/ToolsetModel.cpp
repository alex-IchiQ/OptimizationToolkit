// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/ToolsetModel.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"

void FToolsetModel::RunScan()
{
	// Hand the outgoing numbers to the Dashboard before they are overwritten, so
	// it can show what this scan moved. Every rescan shifts the baseline forward,
	// including the automatic one after a fix — which is the case that matters.
	if (bHasScanned)
	{
		PreviousStats = LastScan.Stats;
		bHasPreviousStats = true;
	}

	LastScan = FLevelAnalyzer::AnalyzeCurrentLevel(BuildExcludedLevelPackages());
	bHasScanned = true;

	RebuildDerivedLists();
}

void FToolsetModel::InvalidateScan()
{
	LastScan = FScanResult();
	bHasScanned = false;
	bHasPreviousStats = false;
	AllFindings.Reset();
	ChangedEvent.Broadcast();
}

bool FToolsetModel::IsLevelIncluded(FName PackageName, bool bPersistentLevel) const
{
	if (const bool* Override = LevelInclusionOverrides.Find(PackageName))
	{
		return *Override;
	}

	// The persistent level is always included by default. The old project setting
	// now controls only the initial state of sub-level toggles; the Dashboard can
	// override either choice without mutating project configuration.
	if (bPersistentLevel)
	{
		return true;
	}

	const UOptimizationToolsetSettings* Settings = GetDefault<UOptimizationToolsetSettings>();
	return !Settings || Settings->bIncludeSubLevels;
}

void FToolsetModel::SetLevelIncluded(FName PackageName, bool bIncluded)
{
	if (IsLevelIncluded(PackageName, /*bPersistentLevel*/ false) == bIncluded
		&& LevelInclusionOverrides.Contains(PackageName))
	{
		return;
	}

	LevelInclusionOverrides.Add(PackageName, bIncluded);

	// Findings from the previous scope are now stale. In particular, leaving
	// them fixable would let Optimize mutate an actor in a level the user just
	// excluded. Clear the result and require one explicit scan of the new scope.
	InvalidateScan();
}

TSet<FName> FToolsetModel::BuildExcludedLevelPackages() const
{
	TSet<FName> Excluded;
	const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return Excluded;
	}

	for (const ULevel* Level : World->GetLevels())
	{
		if (!Level)
		{
			continue;
		}

		const FName PackageName = Level->GetOutermost()->GetFName();
		if (!IsLevelIncluded(PackageName, Level == World->PersistentLevel))
		{
			Excluded.Add(PackageName);
		}
	}
	return Excluded;
}

void FToolsetModel::RebuildDerivedLists()
{
	AllFindings.Reset();

	// One shared FFinding per row keeps every consumer on the same object.
	for (const FFinding& Finding : LastScan.Findings)
	{
		TSharedPtr<FFinding> Shared = MakeShared<FFinding>(Finding);
		AllFindings.Add(Shared);
	}

	ChangedEvent.Broadcast();
}

TArray<TSharedPtr<FFinding>> FToolsetModel::GetVisibleFindings() const
{
	TArray<TSharedPtr<FFinding>> Visible;
	for (const TSharedPtr<FFinding>& Finding : AllFindings)
	{
		if (Finding.IsValid() && PassesSeverityAndSearch(*Finding) && PassesCategory(*Finding))
		{
			Visible.Add(Finding);
		}
	}
	return Visible;
}

int32 FToolsetModel::CountForCategory(ECategory Category) const
{
	int32 Count = 0;
	for (const TSharedPtr<FFinding>& Finding : AllFindings)
	{
		if (!Finding.IsValid() || Finding->Category != Category)
		{
			continue;
		}
		// The unified Optimize workspace owns severity/search filtering, so badges
		// must count exactly what its category would currently show.
		if (!PassesSeverityAndSearch(*Finding))
		{
			continue;
		}
		++Count;
	}
	return Count;
}

void FToolsetModel::SetSearchFilter(const FString& InSearch)
{
	if (SearchFilter != InSearch)
	{
		SearchFilter = InSearch;
		ChangedEvent.Broadcast();
	}
}

void FToolsetModel::ToggleSeverity(ESeverity Severity)
{
	if (EnabledSeverities.Contains(Severity))
	{
		EnabledSeverities.Remove(Severity);
	}
	else
	{
		EnabledSeverities.Add(Severity);
	}
	ChangedEvent.Broadcast();
}

void FToolsetModel::SetCategoryFilter(TOptional<ECategory> Category)
{
	CategoryFilter = Category;
	ChangedEvent.Broadcast();
}

bool FToolsetModel::PassesSeverityAndSearch(const FFinding& Finding) const
{
	if (!EnabledSeverities.Contains(Finding.Severity))
	{
		return false;
	}
	if (!SearchFilter.IsEmpty())
	{
		const FString Haystack = Finding.Title.ToString() + TEXT(" ") + Finding.Subject.ToString();
		if (!Haystack.Contains(SearchFilter))
		{
			return false;
		}
	}
	return true;
}

bool FToolsetModel::PassesCategory(const FFinding& Finding) const
{
	return !CategoryFilter.IsSet() || Finding.Category == CategoryFilter.GetValue();
}

bool FToolsetModel::HasSupportedFix(const FFinding& Finding)
{
	IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Finding.FixId);
	return Fix && Fix->IsSupported();
}

bool FToolsetModel::ApplyFix(TSharedPtr<FFinding> Finding)
{
	if (!Finding.IsValid())
	{
		return false;
	}

	bool bApplied = false;
	if (IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Finding->FixId))
	{
		if (Fix->IsSupported())
		{
			bApplied = Fix->Apply(*Finding);
		}
	}

	RunScan();	// the level changed; every view's numbers are now stale
	return bApplied;
}

int32 FToolsetModel::ApplyFixes(const TArray<TSharedPtr<FFinding>>& Findings)
{
	// Snapshot first: the RunScan() at the end rebuilds AllFindings, which is the
	// array these shared pointers were pulled from.
	TArray<TSharedPtr<FFinding>> Snapshot = Findings;
	int32 AppliedCount = 0;
	for (const TSharedPtr<FFinding>& Finding : Snapshot)
	{
		if (!Finding.IsValid())
		{
			continue;
		}
		if (IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Finding->FixId))
		{
			if (Fix->IsSupported())
			{
				AppliedCount += Fix->Apply(*Finding) ? 1 : 0;
			}
		}
	}

	RunScan();	// one refresh for the whole batch, not one per fix
	return AppliedCount;
}
