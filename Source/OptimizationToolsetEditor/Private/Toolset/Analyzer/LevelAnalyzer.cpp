// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/ToolsetRegistry.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"

FScanResult FLevelAnalyzer::AnalyzeCurrentLevel()
{
	FScanResult Result;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return Result;
	}

	const double StartTime = FPlatformTime::Seconds();
	const FAnalyzeThresholds Thresholds;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		++Result.ActorsScanned;
	}

	for (const TUniquePtr<IAnalyzePass>& Pass : FToolsetRegistry::Get().GetPasses())
	{
		if (Pass)
		{
			Pass->Run(World, Thresholds, Result);
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
