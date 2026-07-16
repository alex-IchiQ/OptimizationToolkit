// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/ToolsetRegistry.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"

FScanResult FLevelAnalyzer::AnalyzeCurrentLevel()
{
	FScanResult Result;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return Result;
	}

	const double StartTime = FPlatformTime::Seconds();
	FAnalyzeThresholds Thresholds;
	if (const UOptimizationToolsetSettings* Settings = GetDefault<UOptimizationToolsetSettings>())
	{
		Thresholds.NaniteCandidateTriangles = FMath::Max(1, Settings->NaniteCandidateTriangles);
		Thresholds.ExcessiveTriangles = FMath::Max(
			Thresholds.NaniteCandidateTriangles, Settings->ExcessiveTriangles);
		Thresholds.OversizedTextureSize = FMath::Max(1, Settings->OversizedTextureSize);
		Thresholds.MaterialSlotBudget = FMath::Max(1, Settings->MaterialSlotBudget);
		Thresholds.MovableLightBudget = FMath::Max(0, Settings->MovableLightBudget);
		Thresholds.InstancingCandidateCount = FMath::Max(2, Settings->InstancingCandidateCount);
	}
	// Walk the world once and bucket the types passes ask for, instead of every
	// pass running its own TActorIterator over the whole level.
	FLevelScanContext Context;
	Context.World = World;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		Context.Actors.Add(Actor);

		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
		{
			Context.StaticMeshActors.Add(StaticMeshActor);
		}
		else if (ALight* Light = Cast<ALight>(Actor))
		{
			Context.Lights.Add(Light);
		}
	}
	Result.ActorsScanned = Context.Actors.Num();

	for (const TUniquePtr<IAnalyzePass>& Pass : FToolsetRegistry::Get().GetPasses())
	{
		if (Pass)
		{
			Pass->Run(Context, Thresholds, Result);
		}
	}

	// Stable ordering: most severe first, then by title so the list is deterministic.
	Result.Findings.Sort([](const FFinding& A, const FFinding& B)
	{
		if (A.Severity != B.Severity)
		{
			return static_cast<uint8>(A.Severity) < static_cast<uint8>(B.Severity);
		}
		return A.Title.CompareTo(B.Title) < 0;
	});

	Result.ScanSeconds = FPlatformTime::Seconds() - StartTime;
	return Result;
}

void FLevelAnalyzer::FocusActor(TWeakObjectPtr<AActor> Actor)
{
	if (!GEditor || !Actor.IsValid())
	{
		return;
	}

	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(Actor.Get(), /*bInSelected*/ true, /*bNotify*/ true);
	GEditor->MoveViewportCamerasToActor(*Actor.Get(), /*bActiveViewportOnly*/ false);
}
