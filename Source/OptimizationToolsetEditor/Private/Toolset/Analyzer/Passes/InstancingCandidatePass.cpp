// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/InstancingCandidatePass.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "InstancingCandidatePass"

namespace
{
	/** Properties that must match before actors are suggested as one instance group. */
	struct FInstancingKey
	{
		const UStaticMesh* Mesh = nullptr;
		TArray<const UMaterialInterface*> Materials;
		FName CollisionProfile;
		ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
		ECollisionChannel ObjectType = ECC_WorldStatic;
		FCollisionResponseContainer CollisionResponses;
		bool bCastShadow = true;

		bool operator==(const FInstancingKey& Other) const
		{
			return Mesh == Other.Mesh
				&& Materials == Other.Materials
				&& CollisionProfile == Other.CollisionProfile
				&& CollisionEnabled == Other.CollisionEnabled
				&& ObjectType == Other.ObjectType
				&& CollisionResponses == Other.CollisionResponses
				&& bCastShadow == Other.bCastShadow;
		}

		// CollisionResponses is deliberately left out of the hash: it is compared in
		// operator== but hashing it adds cost for a field that rarely differs, and a
		// hash collision only costs an extra equality check.
		friend uint32 GetTypeHash(const FInstancingKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.Mesh);
			for (const UMaterialInterface* Material : Key.Materials)
			{
				Hash = HashCombine(Hash, GetTypeHash(Material));
			}
			Hash = HashCombine(Hash, GetTypeHash(Key.CollisionProfile));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Key.CollisionEnabled)));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Key.ObjectType)));
			Hash = HashCombine(Hash, GetTypeHash(Key.bCastShadow));
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
		Key.Mesh = Mesh;
		Key.CollisionProfile = Component->GetCollisionProfileName();
		Key.CollisionEnabled = Component->GetCollisionEnabled();
		Key.ObjectType = Component->GetCollisionObjectType();
		Key.CollisionResponses = Component->GetCollisionResponseToChannels();
		Key.bCastShadow = Component->CastShadow;

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
		const ESeverity Severity = Actors.Num() >= T.InstancingCandidateCount * 3
			? ESeverity::Major : ESeverity::Minor;

		FFinding F(TEXT("Mesh.InstancingCandidate"), Severity, ECategory::Meshes,
			LOCTEXT("InstancingCandidateTitle", "Repeated static meshes could be instanced"),
			FText::Format(LOCTEXT("InstancingCandidateSubject", "{0} x {1}"),
				FText::FromString(Pair.Key.Mesh->GetName()), FText::AsNumber(Actors.Num())));
		F.WhyItMatters = FText::Format(
			LOCTEXT("InstancingCandidateWhy", "These compatible actors may save roughly {0} repeated draw submissions when grouped."),
			FText::AsNumber(EstimatedSavedDrawCalls));
		F.HowToFix = LOCTEXT("InstancingCandidateFix", "Review the group, then replace it with an ISM or HISM component if no actor needs unique behavior.");
		F.TargetActor = Actors[0];
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
