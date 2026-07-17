// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"

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

	LastScan = FLevelAnalyzer::AnalyzeCurrentLevel();
	bHasScanned = true;

	RebuildDerivedLists();
}

void FToolsetModel::RebuildDerivedLists()
{
	AllFindings.Reset();
	FixableFindings.Reset();

	// One shared FFinding per row: Optimize shows the same object Analyze does, so
	// a row can never disagree with itself about what it is describing.
	for (const FFinding& Finding : LastScan.Findings)
	{
		TSharedPtr<FFinding> Shared = MakeShared<FFinding>(Finding);
		AllFindings.Add(Shared);
		if (HasSupportedFix(Finding))
		{
			FixableFindings.Add(Shared);
		}
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

TArray<TSharedPtr<FFinding>> FToolsetModel::GetVisibleFixable() const
{
	TArray<TSharedPtr<FFinding>> Visible;
	for (const TSharedPtr<FFinding>& Finding : FixableFindings)
	{
		if (Finding.IsValid() && PassesCategory(*Finding))
		{
			Visible.Add(Finding);
		}
	}
	return Visible;
}

int32 FToolsetModel::CountForCategory(ECategory Category, bool bFixableOnly) const
{
	const TArray<TSharedPtr<FFinding>>& Source = bFixableOnly ? FixableFindings : AllFindings;

	int32 Count = 0;
	for (const TSharedPtr<FFinding>& Finding : Source)
	{
		if (!Finding.IsValid() || Finding->Category != Category)
		{
			continue;
		}

		// Analyze has severity/search filters above its list; a badge that ignored
		// them would promise rows the panel won't show. Optimize has no such
		// toolbar, so its badge counts everything fixable in the category.
		if (!bFixableOnly && !PassesSeverityAndSearch(*Finding))
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

void FToolsetModel::ApplyFix(TSharedPtr<FFinding> Finding)
{
	if (!Finding.IsValid())
	{
		return;
	}

	if (IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Finding->FixId))
	{
		if (Fix->IsSupported())
		{
			Fix->Apply(*Finding);
		}
	}

	RunScan();	// the level changed; every view's numbers are now stale
}

void FToolsetModel::ApplyAllFixes()
{
	// Snapshot first: each Apply mutates the level, and the rescan at the end
	// rebuilds the very array we are iterating.
	TArray<TSharedPtr<FFinding>> Snapshot = FixableFindings;
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
				Fix->Apply(*Finding);
			}
		}
	}

	RunScan();
}
