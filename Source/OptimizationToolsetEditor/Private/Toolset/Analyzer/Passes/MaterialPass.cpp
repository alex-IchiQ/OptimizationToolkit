// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/MaterialPass.h"
#include "Toolset/ToolsetCompat.h"

#include "MaterialShared.h"
#include "MaterialStatsCommon.h"
#include "Shader.h"
#include "RHIShaderPlatform.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "MaterialPass"

namespace
{
	/**
	 * The compiled shader map for a material, or null when there isn't one.
	 *
	 * Null is normal, not exceptional: a material still compiling, or one that
	 * failed to compile, has no shader map, and every stat below is unknowable
	 * until it does. Reporting a "0 instruction" material would be a lie.
	 */
	const FMaterialResource* ResourceForMaterial(const UMaterialInterface& Material)
	{
#if OPTIMIZATION_MATERIAL_RESOURCE_BY_SHADER_PLATFORM
		return Material.GetMaterialResource(GMaxRHIShaderPlatform);
#else
		return Material.GetMaterialResource(GMaxRHIFeatureLevel);
#endif
	}

	/**
	 * Instruction count of the heaviest shader the engine considers representative
	 * of this material, plus which one that was.
	 *
	 * A material compiles to dozens of permutations, so "the" instruction count
	 * needs a choice of shader. We ask the engine which ones represent the material
	 * — the same answer its own stats panel uses — rather than inventing a rule.
	 * `FMaterialStatsUtils::GetRepresentativeInstructionCounts()` does exactly this
	 * job but is not exported, so we walk its exported half instead.
	 */
	int32 WorstInstructionCount(const FMaterialResource& Resource, FString& OutShaderDescription)
	{
		const FMaterialShaderMap* ShaderMap = Resource.GetGameThreadShaderMap();
		if (!ShaderMap)
		{
			return INDEX_NONE;
		}

		TMap<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>> ShaderTypes;
		FMaterialStatsUtils::GetRepresentativeShaderTypesAndDescriptions(ShaderTypes, &Resource);

		int32 Worst = INDEX_NONE;
		for (const TPair<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>>& Pair : ShaderTypes)
		{
			for (const FMaterialStatsUtils::FRepresentativeShaderInfo& Info : Pair.Value)
			{
				FShaderType* ShaderType = FindShaderTypeByName(Info.ShaderName);
				if (!ShaderType)
				{
					continue;
				}

				const int32 Count = static_cast<int32>(ShaderMap->GetMaxNumInstructionsForShader(ShaderType));
				if (Count > Worst)
				{
					Worst = Count;
					OutShaderDescription = Info.ShaderDescription;
				}
			}
		}
		return Worst;
	}
	/** Static-mesh asset plus its effective component material assignments. */
	struct FMaterialSlotKey
	{
		UStaticMesh* Mesh = nullptr;
		TArray<const UMaterialInterface*> Materials;
		bool bUsesComponentOverrides = false;

		bool operator==(const FMaterialSlotKey& Other) const
		{
			return Mesh == Other.Mesh && Materials == Other.Materials
				&& bUsesComponentOverrides == Other.bUsesComponentOverrides;
		}

		friend uint32 GetTypeHash(const FMaterialSlotKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.Mesh);
			for (const UMaterialInterface* Material : Key.Materials)
			{
				Hash = HashCombine(Hash, GetTypeHash(Material));
			}
			Hash = HashCombine(Hash, GetTypeHash(Key.bUsesComponentOverrides));
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
				UMaterialInterface* EffectiveMaterial = Component->GetMaterial(MaterialIndex);
				Key.Materials.Add(EffectiveMaterial);
				Key.bUsesComponentOverrides |= EffectiveMaterial != Mesh->GetMaterial(MaterialIndex);
			}
			SlotLayouts.FindOrAdd(MoveTemp(Key), Actor);
		}
	}

	TSet<const UStaticMesh*> ReportedSlotBudgets;
	for (const TPair<FMaterialSlotKey, TWeakObjectPtr<AActor>>& Pair : SlotLayouts)
	{
		const int32 SlotCount = Pair.Key.Materials.Num();
		const FText Subject = FText::Format(
			LOCTEXT("MaterialSlotsSubject", "{0} ({1} slots)"),
			FText::FromString(Pair.Key.Mesh->GetName()), FText::AsNumber(SlotCount));

		if (SlotCount > T.MaterialSlotBudget && !ReportedSlotBudgets.Contains(Pair.Key.Mesh))
		{
			ReportedSlotBudgets.Add(Pair.Key.Mesh);
			const ESeverity Severity = SlotCount > T.MaterialSlotBudget * 2
				? ESeverity::Major : ESeverity::Minor;
			FFinding F(TEXT("Material.SlotBudget"), Severity, ECategory::Materials, EFindingScope::Asset,
				LOCTEXT("MaterialSlotBudgetTitle", "Mesh has many material slots"), Subject);
			F.WhyItMatters = LOCTEXT("MaterialSlotBudgetWhy", "Each visible mesh section typically requires another draw submission and material state change.");
			F.HowToFix = LOCTEXT("MaterialSlotBudgetFix", "Merge compatible materials or consolidate sections in the source mesh where visual requirements allow.");
			F.TargetActor = Pair.Value;
			F.TargetAsset = Pair.Key.Mesh;
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
				Pair.Key.bUsesComponentOverrides ? EFindingScope::Actor : EFindingScope::Asset,
				LOCTEXT("EmptyMaterialSlotsTitle", "Mesh contains empty material slots"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("EmptyMaterialSlotsWhy", "{0} slots have no material assigned, which often indicates obsolete or broken section setup."),
				FText::AsNumber(EmptySlots));
			F.HowToFix = LOCTEXT("EmptyMaterialSlotsFix", "Inspect the affected sections and remove unused slots in the source mesh or assign the intended material.");
			F.TargetActor = Pair.Value;
			if (!Pair.Key.bUsesComponentOverrides)
			{
				F.TargetAsset = Pair.Key.Mesh;
			}
			Out.Findings.Add(MoveTemp(F));
		}

		if (DuplicateSlots > 0)
		{
			FFinding F(TEXT("Material.DuplicateSlots"), ESeverity::Minor, ECategory::Materials,
				Pair.Key.bUsesComponentOverrides ? EFindingScope::Actor : EFindingScope::Asset,
				LOCTEXT("DuplicateMaterialSlotsTitle", "Multiple mesh slots use the same material"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("DuplicateMaterialSlotsWhy", "{0} repeated assignments may represent sections that can be merged to reduce draw submissions."),
				FText::AsNumber(DuplicateSlots));
			F.HowToFix = LOCTEXT("DuplicateMaterialSlotsFix", "Review section boundaries and merge slots in the source mesh when they do not need separate IDs.");
			F.TargetActor = Pair.Value;
			if (!Pair.Key.bUsesComponentOverrides)
			{
				F.TargetAsset = Pair.Key.Mesh;
			}
			Out.Findings.Add(MoveTemp(F));
		}
	}

	for (const TPair<UMaterialInterface*, TWeakObjectPtr<AActor>>& Pair : MaterialOwners)
	{
		UMaterialInterface* Material = Pair.Key;
		const FText Subject = FText::FromString(Material->GetName());

		// --- Shader cost: samplers and instructions ---------------------------
		//
		// Both need the compiled shader map, so both are skipped in the same
		// breath when there isn't one.
		if (const FMaterialResource* Resource = ResourceForMaterial(*Material))
		{
			// -1 means no valid shader map (still compiling, or a compile error).
			const int32 Samplers = Resource->GetSamplerUsage();
			if (Samplers > T.MaterialSamplerBudget)
			{
				// 16 is the hard limit on most platforms, so past the budget this is
				// heading for a compile failure, not just a slow material.
				const ESeverity Severity = Samplers >= 16 ? ESeverity::Major : ESeverity::Minor;

				FFinding F(TEXT("Material.SamplerCount"), Severity, ECategory::Materials, EFindingScope::Asset,
					LOCTEXT("SamplerCountTitle", "Material uses many texture samplers"),
					FText::Format(LOCTEXT("SamplerCountSubject", "{0} ({1} samplers)"), Subject, FText::AsNumber(Samplers)));
				F.WhyItMatters = FText::Format(
					LOCTEXT("SamplerCountWhy", "Each sampler is a texture fetch per pixel, and most platforms cap a material at 16. The budget is {0}."),
					FText::AsNumber(T.MaterialSamplerBudget));
				F.HowToFix = LOCTEXT("SamplerCountFix", "Pack greyscale maps into channels of one texture, or switch shared textures to Shared:Wrap sampler source.");
				F.TargetActor = Pair.Value;
				F.TargetAsset = Material;
				Out.Findings.Add(MoveTemp(F));
			}

			FString WorstShader;
			const int32 Instructions = WorstInstructionCount(*Resource, WorstShader);
			if (Instructions > T.MaterialInstructionBudget)
			{
				const ESeverity Severity = Instructions > T.MaterialInstructionBudget * 2
					? ESeverity::Major : ESeverity::Minor;

				FFinding F(TEXT("Material.InstructionCount"), Severity, ECategory::Materials, EFindingScope::Asset,
					LOCTEXT("InstructionCountTitle", "Material shader is instruction-heavy"),
					FText::Format(LOCTEXT("InstructionCountSubject", "{0} ({1} instructions)"), Subject, FText::AsNumber(Instructions)));
				F.WhyItMatters = FText::Format(
					LOCTEXT("InstructionCountWhy", "{0} costs {1} instructions per pixel it covers, against a budget of {2}. The cost scales with how much of the screen it fills."),
					FText::FromString(WorstShader), FText::AsNumber(Instructions), FText::AsNumber(T.MaterialInstructionBudget));
				F.HowToFix = LOCTEXT("InstructionCountFix", "Bake constant maths into textures, move per-pixel work to the vertex shader, or trim unused nodes feeding the output.");
				F.TargetActor = Pair.Value;
				F.TargetAsset = Material;
				Out.Findings.Add(MoveTemp(F));
			}
		}

		if (IsTranslucentBlendMode(*Material))
		{
			FFinding F(TEXT("Material.Translucent"), ESeverity::Minor, ECategory::Materials, EFindingScope::Asset,
				LOCTEXT("TranslucentMaterialTitle", "Translucent material requires review"), Subject);
			F.WhyItMatters = LOCTEXT("TranslucentMaterialWhy", "Translucency can create heavy overdraw and sorting cost, especially on large screen-space surfaces.");
			F.HowToFix = LOCTEXT("TranslucentMaterialFix", "Keep translucency only where required; prefer Masked or Opaque when the visual result permits.");
			F.TargetActor = Pair.Value;
			F.TargetAsset = Material;
			Out.Findings.Add(MoveTemp(F));
		}

		if (Material->IsTwoSided())
		{
			FFinding F(TEXT("Material.TwoSided"), ESeverity::Minor, ECategory::Materials, EFindingScope::Asset,
				LOCTEXT("TwoSidedMaterialTitle", "Two-sided material requires review"), Subject);
			F.WhyItMatters = LOCTEXT("TwoSidedMaterialWhy", "Rendering both face orientations increases rasterized geometry and can amplify overdraw.");
			F.HowToFix = LOCTEXT("TwoSidedMaterialFix", "Disable Two Sided unless the asset genuinely needs visible backfaces, such as foliage or thin cloth.");
			F.TargetActor = Pair.Value;
			F.TargetAsset = Material;
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

#undef LOCTEXT_NAMESPACE
