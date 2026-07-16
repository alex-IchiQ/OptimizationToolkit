// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/MaterialPass.h"

#include "MaterialShared.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "MaterialPass"

namespace
{
	/** Static-mesh asset plus its effective component material assignments. */
	struct FMaterialSlotKey
	{
		const UStaticMesh* Mesh = nullptr;
		TArray<const UMaterialInterface*> Materials;

		bool operator==(const FMaterialSlotKey& Other) const
		{
			return Mesh == Other.Mesh && Materials == Other.Materials;
		}

		friend uint32 GetTypeHash(const FMaterialSlotKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.Mesh);
			for (const UMaterialInterface* Material : Key.Materials)
			{
				Hash = HashCombine(Hash, GetTypeHash(Material));
			}
			return Hash;
		}
	};
}

void FMaterialPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<FMaterialSlotKey, TWeakObjectPtr<AActor>> SlotLayouts;
	TMap<UMaterialInterface*, TWeakObjectPtr<AActor>> MaterialOwners;

	for (AActor* Actor : Context.Actors)
	{
		TInlineComponentArray<UMeshComponent*> MeshComponents(Actor);
		for (UMeshComponent* Component : MeshComponents)
		{
			if (!Component)
			{
				continue;
			}

			const int32 MaterialCount = Component->GetNumMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
				if (Material && !Material->HasAnyFlags(RF_Transient)
					&& !Material->GetPathName().StartsWith(TEXT("/Engine/"))
					&& !MaterialOwners.Contains(Material))
				{
					MaterialOwners.Add(Material, Actor);
				}
			}

			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
			UStaticMesh* Mesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
			if (!Mesh)
			{
				continue;
			}

			FMaterialSlotKey Key;
			Key.Mesh = Mesh;
			Key.Materials.Reserve(MaterialCount);
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				Key.Materials.Add(Component->GetMaterial(MaterialIndex));
			}
			SlotLayouts.FindOrAdd(MoveTemp(Key), Actor);
		}
	}

	for (const TPair<FMaterialSlotKey, TWeakObjectPtr<AActor>>& Pair : SlotLayouts)
	{
		const int32 SlotCount = Pair.Key.Materials.Num();
		const FText Subject = FText::Format(
			LOCTEXT("MaterialSlotsSubject", "{0} ({1} slots)"),
			FText::FromString(Pair.Key.Mesh->GetName()), FText::AsNumber(SlotCount));

		if (SlotCount > T.MaterialSlotBudget)
		{
			const ESeverity Severity = SlotCount > T.MaterialSlotBudget * 2
				? ESeverity::Major : ESeverity::Minor;
			FFinding F(TEXT("Material.SlotBudget"), Severity, ECategory::Materials,
				LOCTEXT("MaterialSlotBudgetTitle", "Mesh has many material slots"), Subject);
			F.WhyItMatters = LOCTEXT("MaterialSlotBudgetWhy", "Each visible mesh section typically requires another draw submission and material state change.");
			F.HowToFix = LOCTEXT("MaterialSlotBudgetFix", "Merge compatible materials or consolidate sections in the source mesh where visual requirements allow.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}

		int32 EmptySlots = 0;
		int32 DuplicateSlots = 0;
		TSet<const UMaterialInterface*> SeenMaterials;
		for (const UMaterialInterface* Material : Pair.Key.Materials)
		{
			if (!Material)
			{
				++EmptySlots;
			}
			else if (SeenMaterials.Contains(Material))
			{
				++DuplicateSlots;
			}
			else
			{
				SeenMaterials.Add(Material);
			}
		}

		if (EmptySlots > 0)
		{
			FFinding F(TEXT("Material.EmptySlots"), ESeverity::Minor, ECategory::Materials,
				LOCTEXT("EmptyMaterialSlotsTitle", "Mesh contains empty material slots"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("EmptyMaterialSlotsWhy", "{0} slots have no material assigned, which often indicates obsolete or broken section setup."),
				FText::AsNumber(EmptySlots));
			F.HowToFix = LOCTEXT("EmptyMaterialSlotsFix", "Inspect the affected sections and remove unused slots in the source mesh or assign the intended material.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}

		if (DuplicateSlots > 0)
		{
			FFinding F(TEXT("Material.DuplicateSlots"), ESeverity::Minor, ECategory::Materials,
				LOCTEXT("DuplicateMaterialSlotsTitle", "Multiple mesh slots use the same material"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("DuplicateMaterialSlotsWhy", "{0} repeated assignments may represent sections that can be merged to reduce draw submissions."),
				FText::AsNumber(DuplicateSlots));
			F.HowToFix = LOCTEXT("DuplicateMaterialSlotsFix", "Review section boundaries and merge slots in the source mesh when they do not need separate IDs.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}
	}

	for (const TPair<UMaterialInterface*, TWeakObjectPtr<AActor>>& Pair : MaterialOwners)
	{
		UMaterialInterface* Material = Pair.Key;
		const FText Subject = FText::FromString(Material->GetName());

		if (IsTranslucentBlendMode(*Material))
		{
			FFinding F(TEXT("Material.Translucent"), ESeverity::Minor, ECategory::Materials,
				LOCTEXT("TranslucentMaterialTitle", "Translucent material requires review"), Subject);
			F.WhyItMatters = LOCTEXT("TranslucentMaterialWhy", "Translucency can create heavy overdraw and sorting cost, especially on large screen-space surfaces.");
			F.HowToFix = LOCTEXT("TranslucentMaterialFix", "Keep translucency only where required; prefer Masked or Opaque when the visual result permits.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}

		if (Material->IsTwoSided())
		{
			FFinding F(TEXT("Material.TwoSided"), ESeverity::Minor, ECategory::Materials,
				LOCTEXT("TwoSidedMaterialTitle", "Two-sided material requires review"), Subject);
			F.WhyItMatters = LOCTEXT("TwoSidedMaterialWhy", "Rendering both face orientations increases rasterized geometry and can amplify overdraw.");
			F.HowToFix = LOCTEXT("TwoSidedMaterialFix", "Disable Two Sided unless the asset genuinely needs visible backfaces, such as foliage or thin cloth.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

#undef LOCTEXT_NAMESPACE
