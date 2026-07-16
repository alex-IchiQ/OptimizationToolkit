// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/AnalyzePasses.h"
#include "Toolset/ToolsetCompat.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "AnalyzePasses"

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

	bool IsSpecialPurposeTextureGroup(TextureGroup Group)
	{
		switch (Group)
		{
		case TEXTUREGROUP_UI:
		case TEXTUREGROUP_RenderTarget:
		case TEXTUREGROUP_Lightmap:
		case TEXTUREGROUP_Shadowmap:
		case TEXTUREGROUP_ColorLookupTable:
		case TEXTUREGROUP_Terrain_Heightmap:
		case TEXTUREGROUP_Terrain_Weightmap:
		case TEXTUREGROUP_Bokeh:
		case TEXTUREGROUP_IESLightProfile:
		case TEXTUREGROUP_Pixels2D:
			return true;
		default:
			return false;
		}
	}
}

void FStaticMeshPass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	// De-dupe by mesh asset: many actors can share one mesh; we only want to
	// report an asset-level problem (LODs, Nanite, tris) once.
	TSet<const UStaticMesh*> Reported;

	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;

		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}

		const FText Subject = FText::FromString(Mesh->GetName());
		const int32 NumTris = Mesh->GetNumTriangles(0);

#if OPTIMIZATION_HAS_NANITE
		const bool bNanite = Mesh->IsNaniteEnabled();
#else
		const bool bNanite = false;
#endif

		// --- Collision: per-poly on an otherwise cheap prop is a silent CPU cost.
		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			if (Body->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple)
			{
				FFinding F(ESeverity::Minor, ECategory::Collision,
					LOCTEXT("ComplexCollisionTitle", "Complex (per-poly) collision used as simple"), Subject);
				F.WhyItMatters = LOCTEXT("ComplexCollisionWhy", "Per-poly collision is far more expensive to query than a primitive hull.");
				F.HowToFix = LOCTEXT("ComplexCollisionFix", "Add a simple collision primitive and switch the trace flag to Default.");
				F.TargetActor = Actor;
				F.FixId = TEXT("Fix_SimpleCollision");
				Out.Findings.Add(MoveTemp(F));
			}
		}

		if (Reported.Contains(Mesh))
		{
			continue;
		}
		Reported.Add(Mesh);

		// --- Excessive triangles without Nanite: a hard performance cliff.
		if (!bNanite && NumTris >= T.ExcessiveTriangles)
		{
			FFinding F(ESeverity::Critical, ECategory::Meshes,
				LOCTEXT("ExcessiveTrisTitle", "Excessive triangles on a non-Nanite mesh"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("ExcessiveTrisWhy", "{0} triangles rendered without Nanite hammers the GPU and draw-call budget."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("ExcessiveTrisFix", "Enable Nanite, or add aggressive LODs / decimate the source mesh.");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_EnableNanite");
			Out.Findings.Add(MoveTemp(F));
		}
#if OPTIMIZATION_HAS_NANITE
		// --- Nanite candidate: heavy enough to benefit, but not turned on.
		else if (!bNanite && NumTris >= T.NaniteCandidateTriangles)
		{
			FFinding F(ESeverity::Major, ECategory::Meshes,
				LOCTEXT("NaniteCandidateTitle", "Nanite candidate not enabled"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("NaniteCandidateWhy", "At {0} triangles this mesh would render cheaper and self-LOD with Nanite."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("NaniteCandidateFix", "Enable Nanite on the static mesh (one-click fix available).");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_EnableNanite");
			Out.Findings.Add(MoveTemp(F));
		}
#endif

		// --- Missing LODs on a non-Nanite mesh: no distance falloff at all.
		if (!bNanite && Mesh->GetNumLODs() <= 1 && NumTris >= T.NaniteCandidateTriangles)
		{
			FFinding F(ESeverity::Major, ECategory::Meshes,
				LOCTEXT("MissingLODTitle", "No LODs on a heavy non-Nanite mesh"), Subject);
			F.WhyItMatters = LOCTEXT("MissingLODWhy", "Without LODs the full triangle count is drawn at every distance.");
			F.HowToFix = LOCTEXT("MissingLODFix", "Enable Nanite, or generate an LOD chain (auto-LOD fix available).");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_GenerateLODs");
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

void FLightingPass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TArray<ALight*> MovableLights;

	for (TActorIterator<ALight> It(World); It; ++It)
	{
		ALight* Light = *It;

		ULightComponent* LC = Light->GetLightComponent();
		if (LC && LC->Mobility == EComponentMobility::Movable)
		{
			MovableLights.Add(Light);
		}
	}

	if (MovableLights.Num() <= T.MovableLightBudget)
	{
		return;
	}

	const FText BudgetContext = FText::Format(
		LOCTEXT("MovableLightBudgetContext", "The level contains {0} movable lights; the configured budget is {1}."),
		FText::AsNumber(MovableLights.Num()), FText::AsNumber(T.MovableLightBudget));

	// Emit addressable findings so Focus and Apply operate on the light the user
	// is reviewing instead of an arbitrary actor from an aggregate warning.
	for (ALight* Light : MovableLights)
	{
		FFinding F(ESeverity::Major, ECategory::Lighting,
			LOCTEXT("MovableLightTitle", "Movable light contributes to an exceeded budget"),
			FText::FromString(Light->GetActorLabel()));
		F.WhyItMatters = FText::Format(
			LOCTEXT("MovableLightWhy", "{0} Every movable light adds dynamic shadow and lighting cost each frame."),
			BudgetContext);
		F.HowToFix = LOCTEXT("MovableLightFix", "If this light does not move at runtime, change it to Stationary.");
		F.TargetActor = Light;
		F.FixId = TEXT("Fix_ReviewLightMobility");
		Out.Findings.Add(MoveTemp(F));
	}
}

void FTexturePass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<UTexture2D*, TWeakObjectPtr<AActor>> TextureOwners;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (!Component)
			{
				continue;
			}

			TArray<UTexture*> UsedTextures;
			Component->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High);
			for (UTexture* UsedTexture : UsedTextures)
			{
				UTexture2D* Texture = Cast<UTexture2D>(UsedTexture);
				if (!Texture || Texture->HasAnyFlags(RF_Transient)
					|| Texture->GetPathName().StartsWith(TEXT("/Engine/")))
				{
					continue;
				}

				if (!TextureOwners.Contains(Texture))
				{
					TextureOwners.Add(Texture, Actor);
				}
			}
		}
	}

	for (const TPair<UTexture2D*, TWeakObjectPtr<AActor>>& Pair : TextureOwners)
	{
		UTexture2D* Texture = Pair.Key;
		const int32 Width = Texture->GetSizeX();
		const int32 Height = Texture->GetSizeY();
		if (Width <= 0 || Height <= 0)
		{
			continue;
		}

		const int32 LongestSide = FMath::Max(Width, Height);
		const int32 EffectiveLongestSide = Texture->MaxTextureSize > 0
			? FMath::Min(LongestSide, Texture->MaxTextureSize) : LongestSide;
		const bool bSpecialPurpose = IsSpecialPurposeTextureGroup(Texture->LODGroup);
		const bool bPowerOfTwo = FMath::IsPowerOfTwo(Width) && FMath::IsPowerOfTwo(Height);
		const FText Subject = FText::Format(
			LOCTEXT("TextureSubject", "{0} ({1} x {2})"),
			FText::FromString(Texture->GetName()), FText::AsNumber(Width), FText::AsNumber(Height));

		if (EffectiveLongestSide > T.OversizedTextureSize)
		{
			const ESeverity Severity = Texture->VirtualTextureStreaming || bSpecialPurpose
				? ESeverity::Minor : ESeverity::Major;
			FFinding F(Severity, ECategory::Textures,
				LOCTEXT("OversizedTextureTitle", "Texture exceeds the configured size limit"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("OversizedTextureWhy", "Its effective longest side is {0}px; large textures increase memory, streaming, and build cost."),
				FText::AsNumber(EffectiveLongestSide));
			F.HowToFix = LOCTEXT("OversizedTextureFix", "Verify texel density, then reduce the source resolution or set Max Texture Size.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}

		if (!bSpecialPurpose && !bPowerOfTwo
			&& Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::None)
		{
			FFinding F(ESeverity::Minor, ECategory::Textures,
				LOCTEXT("NonPowerOfTwoTitle", "Texture dimensions are not power-of-two"), Subject);
			F.WhyItMatters = LOCTEXT("NonPowerOfTwoWhy", "Unpadded dimensions can prevent a complete mip chain and make streaming less efficient.");
			F.HowToFix = LOCTEXT("NonPowerOfTwoFix", "Resize the source or choose an appropriate Padding and Resizing mode in the texture asset.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}

		if (!bSpecialPurpose && bPowerOfTwo && LongestSide >= 1024
			&& Texture->MipGenSettings == TMGS_NoMipmaps && !Texture->VirtualTextureStreaming)
		{
			const ESeverity Severity = LongestSide >= 2048 ? ESeverity::Major : ESeverity::Minor;
			FFinding F(Severity, ECategory::Textures,
				LOCTEXT("MissingTextureMipsTitle", "Large texture has mipmaps disabled"), Subject);
			F.WhyItMatters = LOCTEXT("MissingTextureMipsWhy", "Rendering the full-resolution texture at every distance wastes bandwidth and can shimmer.");
			F.HowToFix = LOCTEXT("MissingTextureMipsFix", "Use FromTextureGroup mip generation unless this asset intentionally requires exact texels.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

void FMaterialPass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<FMaterialSlotKey, TWeakObjectPtr<AActor>> SlotLayouts;
	TMap<UMaterialInterface*, TWeakObjectPtr<AActor>> MaterialOwners;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
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
			FFinding F(Severity, ECategory::Materials,
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
			FFinding F(ESeverity::Minor, ECategory::Materials,
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
			FFinding F(ESeverity::Minor, ECategory::Materials,
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
			FFinding F(ESeverity::Minor, ECategory::Materials,
				LOCTEXT("TranslucentMaterialTitle", "Translucent material requires review"), Subject);
			F.WhyItMatters = LOCTEXT("TranslucentMaterialWhy", "Translucency can create heavy overdraw and sorting cost, especially on large screen-space surfaces.");
			F.HowToFix = LOCTEXT("TranslucentMaterialFix", "Keep translucency only where required; prefer Masked or Opaque when the visual result permits.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}

		if (Material->IsTwoSided())
		{
			FFinding F(ESeverity::Minor, ECategory::Materials,
				LOCTEXT("TwoSidedMaterialTitle", "Two-sided material requires review"), Subject);
			F.WhyItMatters = LOCTEXT("TwoSidedMaterialWhy", "Rendering both face orientations increases rasterized geometry and can amplify overdraw.");
			F.HowToFix = LOCTEXT("TwoSidedMaterialFix", "Disable Two Sided unless the asset genuinely needs visible backfaces, such as foliage or thin cloth.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

void FInstancingCandidatePass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<FInstancingKey, TArray<AStaticMeshActor*>> Groups;

	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
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

		FFinding F(Severity, ECategory::Meshes,
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
