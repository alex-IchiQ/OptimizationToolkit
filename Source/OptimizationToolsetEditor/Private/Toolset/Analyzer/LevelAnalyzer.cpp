// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/ToolsetRegistry.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/Light.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/**
	 * Level scale, from the actors the walk already gathered.
	 *
	 * This reads components rather than the pre-bucketed arrays on purpose: a
	 * Blueprint actor's meshes and lights are just as real as an AStaticMeshActor's,
	 * and a count that ignored them would contradict what the user sees in the
	 * viewport. It costs a component walk, not a second world walk.
	 */
	void GatherLevelStats(const FLevelScanContext& Context, FLevelStats& Out)
	{
		Out.Actors = Context.Actors.Num();

		TSet<const UStaticMesh*> UniqueMeshes;
		TSet<const UMaterialInterface*> UniqueMaterials;

		for (AActor* Actor : Context.Actors)
		{
			TInlineComponentArray<UActorComponent*> Components(Actor);
			for (UActorComponent* Component : Components)
			{
				if (Component->IsA<ULightComponentBase>())
				{
					++Out.Lights;
					continue;
				}

				UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component);
				if (!MeshComponent)
				{
					continue;
				}

				// Same exclusions as FMaterialPass: engine defaults and transient
				// instances are not something a user placed or can act on.
				const int32 MaterialCount = MeshComponent->GetNumMaterials();
				for (int32 Index = 0; Index < MaterialCount; ++Index)
				{
					const UMaterialInterface* Material = MeshComponent->GetMaterial(Index);
					if (Material && !Material->HasAnyFlags(RF_Transient)
						&& !Material->GetPathName().StartsWith(TEXT("/Engine/")))
					{
						UniqueMaterials.Add(Material);
					}
				}

				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);
				UStaticMesh* Mesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
				if (!Mesh)
				{
					continue;
				}
				UniqueMeshes.Add(Mesh);

				// Triangles are per *instance*: an ISM/HISM draws its mesh once per
				// instance, so counting the asset once would hide the whole reason
				// instancing candidates matter.
				const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent);
				const int64 InstanceCount = Instanced ? static_cast<int64>(Instanced->GetInstanceCount()) : 1;
				Out.Triangles += static_cast<int64>(Mesh->GetNumTriangles(0)) * InstanceCount;
			}
		}

		Out.Meshes = UniqueMeshes.Num();
		Out.Materials = UniqueMaterials.Num();
	}

	bool HasValidScopeTarget(const FFinding& Finding)
	{
		switch (Finding.Scope)
		{
		case EFindingScope::Asset:
			return Finding.TargetAsset.IsValid();
		case EFindingScope::Actor:
			return Finding.TargetActor.IsValid();
		case EFindingScope::Level:
			return Finding.TargetActor.IsValid() || Finding.RelatedActors.ContainsByPredicate(
				[](const TWeakObjectPtr<AActor>& Actor) { return Actor.IsValid(); });
		case EFindingScope::Project:
			return Finding.Category == ECategory::Project;
		case EFindingScope::System:
			return true;
		default:
			return false;
		}
	}
}

FScanResult FLevelAnalyzer::AnalyzeCurrentLevel(const TSet<FName>& ExcludedLevelPackages)
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
		Thresholds.NaniteMinimumTriangles = FMath::Clamp(
			Settings->NaniteMinimumTriangles, 0, FMath::Max(0, Thresholds.NaniteCandidateTriangles - 1));
		Thresholds.ExcessiveTriangles = FMath::Max(Thresholds.NaniteCandidateTriangles, Settings->ExcessiveTriangles);
		Thresholds.OversizedTextureSize = FMath::Max(1, Settings->OversizedTextureSize);
		Thresholds.TextureDensityBudget = FMath::Max(128, Settings->TextureDensityBudget);
		Thresholds.MaterialSlotBudget = FMath::Max(1, Settings->MaterialSlotBudget);
		Thresholds.MaterialSamplerBudget = FMath::Max(1, Settings->MaterialSamplerBudget);
		Thresholds.MaterialInstructionBudget = FMath::Max(50, Settings->MaterialInstructionBudget);
		Thresholds.MovableLightBudget = FMath::Max(0, Settings->MovableLightBudget);
		Thresholds.LightmapResolutionBudget = FMath::Max(32, Settings->LightmapResolutionBudget);
		Thresholds.InstancingCandidateCount = FMath::Max(2, Settings->InstancingCandidateCount);
		Thresholds.DependencyChainSizeMB = FMath::Max(1, Settings->DependencyChainSizeMB);
	}
	// Walk the world once and bucket the types passes ask for, instead of every
	// pass running its own TActorIterator over the whole level.
	//
	// TActorIterator covers UWorld::GetLevels() — the persistent level *and* every
	// loaded sub-level — so sub-levels are in scope unless the user opts out here.
	// Unloaded ones are invisible to any iterator; nothing can read a level that
	// isn't in memory.
	FLevelScanContext Context;
	Context.World = World;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		const ULevel* ActorLevel = Actor->GetLevel();
		if (ActorLevel && ExcludedLevelPackages.Contains(ActorLevel->GetOutermost()->GetFName()))
		{
			continue;
		}

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
	GatherLevelStats(Context, Result.Stats);

	for (const TUniquePtr<IAnalyzePass>& Pass : FToolsetRegistry::Get().GetPasses())
	{
		if (Pass)
		{
			Pass->Run(Context, Thresholds, Result);
		}
	}

	// A scope is a contract with navigation and fixes, not display metadata.
	// Catch a new pass that declares an asset/actor/level finding but forgets the
	// corresponding target while the mistake is still local to that pass.
	for (const FFinding& Finding : Result.Findings)
	{
		ensureMsgf(HasValidScopeTarget(Finding),
			TEXT("Finding '%s' has scope %d but no compatible target."),
			*Finding.TypeId.ToString(), static_cast<int32>(Finding.Scope));
	}

	// Stamp the sub-level onto every finding that points at an actor. Done here,
	// once, rather than in each pass: it is derivable from TargetActor, so asking
	// nine passes to remember it would only mean eight places to forget it.
	for (FFinding& Finding : Result.Findings)
	{
		const AActor* Actor = Finding.TargetActor.Get();
		if (!Actor)
		{
			continue;
		}

		const ULevel* Level = Actor->GetLevel();
		if (!Level || Level == World->PersistentLevel)
		{
			continue;	// naming the level you already have open is noise
		}

		Finding.LevelName = FText::FromString(FPackageName::GetShortName(Level->GetOutermost()->GetName()));
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
