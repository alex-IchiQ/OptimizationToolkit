// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Analyzer/Passes/InstancingCandidatePass.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Level.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "InstancingCandidatePass"

namespace
{
	/** Properties that must match before actors are suggested as one instance group. */
	struct FInstancingKey
	{
		const ULevel* Level = nullptr;
		const UStaticMesh* Mesh = nullptr;
		FName FolderPath;
		TArray<const UMaterialInterface*> Materials;
		FName CollisionProfile;
		ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
		ECollisionChannel ObjectType = ECC_WorldStatic;
		FCollisionResponseContainer CollisionResponses;
		bool bCastShadow = true;
		bool bActorHidden = false;
		bool bActorHiddenEd = false;
		bool bVisible = true;
		bool bHiddenInGame = false;
		bool bReceivesDecals = true;
		bool bUseAsOccluder = true;
		bool bRenderCustomDepth = false;
		int32 CustomDepthStencilValue = 0;
		float MinDrawDistance = 0.0f;
		float MaxDrawDistance = 0.0f;
		bool bLightingChannel0 = true;
		bool bLightingChannel1 = false;
		bool bLightingChannel2 = false;

		bool operator==(const FInstancingKey& Other) const
		{
			return Level == Other.Level
				&& Mesh == Other.Mesh
				&& FolderPath == Other.FolderPath
				&& Materials == Other.Materials
				&& CollisionProfile == Other.CollisionProfile
				&& CollisionEnabled == Other.CollisionEnabled
				&& ObjectType == Other.ObjectType
				&& CollisionResponses == Other.CollisionResponses
				&& bCastShadow == Other.bCastShadow
				&& bActorHidden == Other.bActorHidden
				&& bActorHiddenEd == Other.bActorHiddenEd
				&& bVisible == Other.bVisible
				&& bHiddenInGame == Other.bHiddenInGame
				&& bReceivesDecals == Other.bReceivesDecals
				&& bUseAsOccluder == Other.bUseAsOccluder
				&& bRenderCustomDepth == Other.bRenderCustomDepth
				&& CustomDepthStencilValue == Other.CustomDepthStencilValue
				&& MinDrawDistance == Other.MinDrawDistance
				&& MaxDrawDistance == Other.MaxDrawDistance
				&& bLightingChannel0 == Other.bLightingChannel0
				&& bLightingChannel1 == Other.bLightingChannel1
				&& bLightingChannel2 == Other.bLightingChannel2;
		}

		// CollisionResponses is deliberately left out of the hash: it is compared in
		// operator== but hashing it adds cost for a field that rarely differs, and a
		// hash collision only costs an extra equality check.
		friend uint32 GetTypeHash(const FInstancingKey& Key)
		{
			uint32 Hash = HashCombine(GetTypeHash(Key.Level), GetTypeHash(Key.Mesh));
			Hash = HashCombine(Hash, GetTypeHash(Key.FolderPath));
			for (const UMaterialInterface* Material : Key.Materials)
			{
				Hash = HashCombine(Hash, GetTypeHash(Material));
			}
			Hash = HashCombine(Hash, GetTypeHash(Key.CollisionProfile));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Key.CollisionEnabled)));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Key.ObjectType)));
			Hash = HashCombine(Hash, GetTypeHash(Key.bCastShadow));
			Hash = HashCombine(Hash, GetTypeHash(Key.bActorHidden));
			Hash = HashCombine(Hash, GetTypeHash(Key.bActorHiddenEd));
			Hash = HashCombine(Hash, GetTypeHash(Key.bVisible));
			Hash = HashCombine(Hash, GetTypeHash(Key.bHiddenInGame));
			Hash = HashCombine(Hash, GetTypeHash(Key.bReceivesDecals));
			Hash = HashCombine(Hash, GetTypeHash(Key.bUseAsOccluder));
			Hash = HashCombine(Hash, GetTypeHash(Key.bRenderCustomDepth));
			Hash = HashCombine(Hash, GetTypeHash(Key.CustomDepthStencilValue));
			Hash = HashCombine(Hash, GetTypeHash(Key.MinDrawDistance));
			Hash = HashCombine(Hash, GetTypeHash(Key.MaxDrawDistance));
			Hash = HashCombine(Hash, GetTypeHash(Key.bLightingChannel0));
			Hash = HashCombine(Hash, GetTypeHash(Key.bLightingChannel1));
			Hash = HashCombine(Hash, GetTypeHash(Key.bLightingChannel2));
			return Hash;
		}
	};
}

void FInstancingCandidatePass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<FInstancingKey, TArray<AStaticMeshActor*>> Groups;

	for (AStaticMeshActor* Actor : Context.StaticMeshActors)
	{
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;

		// Converting subclasses, attached actors, tagged actors, or movable actors
		// can silently discard gameplay behavior. Keep this detector conservative.
		if (!Mesh || Actor->GetClass() != AStaticMeshActor::StaticClass()
			|| Component->Mobility != EComponentMobility::Static
			|| Actor->GetAttachParentActor() != nullptr
			|| !Component->GetAttachChildren().IsEmpty()
			|| !Actor->Tags.IsEmpty() || !Component->ComponentTags.IsEmpty())
		{
			continue;
		}

		FInstancingKey Key;
		Key.Level = Actor->GetLevel();
		Key.Mesh = Mesh;
		Key.FolderPath = Actor->GetFolderPath();
		Key.CollisionProfile = Component->GetCollisionProfileName();
		Key.CollisionEnabled = Component->GetCollisionEnabled();
		Key.ObjectType = Component->GetCollisionObjectType();
		Key.CollisionResponses = Component->GetCollisionResponseToChannels();
		Key.bCastShadow = Component->CastShadow;
		Key.bActorHidden = Actor->IsHidden();
		Key.bActorHiddenEd = Actor->IsHiddenEd();
		Key.bVisible = Component->IsVisible();
		Key.bHiddenInGame = Component->bHiddenInGame;
		Key.bReceivesDecals = Component->bReceivesDecals;
		Key.bUseAsOccluder = Component->bUseAsOccluder;
		Key.bRenderCustomDepth = Component->bRenderCustomDepth;
		Key.CustomDepthStencilValue = Component->CustomDepthStencilValue;
		Key.MinDrawDistance = Component->MinDrawDistance;
		Key.MaxDrawDistance = Component->LDMaxDrawDistance;
		Key.bLightingChannel0 = Component->LightingChannels.bChannel0;
		Key.bLightingChannel1 = Component->LightingChannels.bChannel1;
		Key.bLightingChannel2 = Component->LightingChannels.bChannel2;

		const int32 MaterialCount = Component->GetNumMaterials();
		Key.Materials.Reserve(MaterialCount);
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			Key.Materials.Add(Component->GetMaterial(MaterialIndex));
		}

		Groups.FindOrAdd(MoveTemp(Key)).Add(Actor);
	}

	for (const TPair<FInstancingKey, TArray<AStaticMeshActor*>>& Pair : Groups)
	{
		const TArray<AStaticMeshActor*>& Actors = Pair.Value;
		if (Actors.Num() < T.InstancingCandidateCount)
		{
			continue;
		}

		const int32 MaterialPasses = FMath::Max(1, Pair.Key.Materials.Num());
		const int32 EstimatedSavedDrawCalls = (Actors.Num() - 1) * MaterialPasses;
		const ESeverity Severity = Actors.Num() >= T.InstancingCandidateCount * 3 ? ESeverity::Major : ESeverity::Minor;

		FFinding F(TEXT("Mesh.InstancingCandidate"), Severity, ECategory::Meshes, EFindingScope::Level,
			LOCTEXT("InstancingCandidateTitle", "Repeated static meshes could be instanced"),
			FText::Format(LOCTEXT("InstancingCandidateSubject", "{0} x {1}"),
				FText::FromString(Pair.Key.Mesh->GetName()), FText::AsNumber(Actors.Num())));
		F.WhyItMatters = FText::Format(
			LOCTEXT("InstancingCandidateWhy", "These compatible actors may save roughly {0} repeated draw submissions when grouped."),
			FText::AsNumber(EstimatedSavedDrawCalls));
		F.HowToFix = LOCTEXT("InstancingCandidateFix", "Review the group, then replace it with an ISM or HISM component if no actor needs unique behavior.");
		F.TargetActor = Actors[0];

		// Hand the whole group to the fix: it rewrites all of them, and it must
		// not have to re-derive the compatibility rules enforced above.
		F.RelatedActors.Reserve(Actors.Num());
		for (AStaticMeshActor* GroupActor : Actors)
		{
			F.RelatedActors.Add(GroupActor);
		}
		F.FixId = TEXT("Fix_ConvertToInstances");

		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
