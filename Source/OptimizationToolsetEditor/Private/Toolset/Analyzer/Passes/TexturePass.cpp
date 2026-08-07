// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Analyzer/Passes/TexturePass.h"

#include "SceneTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureStreamingTypes.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

#define LOCTEXT_NAMESPACE "TexturePass"

namespace
{
	/** Above this, a never-stream texture is a memory cost worth flagging. */
	constexpr int64 NonStreamingBudgetBytes = 256 * 1024;

	/**
	 * Texels this texture delivers per metre of surface, for one usage of it.
	 *
	 * `TexelFactor` is world units per UV unit — how much of the world one tile of
	 * the texture is stretched over — which is why this beats judging the texture's
	 * own dimensions: it accounts for UV tiling. A 1m tile repeated across a 50m
	 * wall reads as 1m of coverage, not 50m, so a tiling material is not accused of
	 * being oversized just for being big on screen.
	 */
	float TexelsPerMetre(int32 TextureSize, float TexelFactor)
	{
		// Unreal's world unit is a centimetre.
		return (TextureSize * 100.0f) / TexelFactor;
	}
	/** Groups whose size/mip rules are deliberately different from normal art content. */
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

void FTexturePass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<UTexture2D*, TWeakObjectPtr<AActor>> TextureOwners;

	// Largest TexelFactor seen for each texture, i.e. its *least* dense usage.
	//
	// A texture stretched over a wall somewhere justifies its resolution even if
	// it is also wasted on a doorknob: the doorknob's UVs are the mistake, not the
	// asset. So a texture is only oversized when even the usage that spreads it
	// furthest is still too dense — which makes this the conservative direction.
	TMap<UTexture2D*, float> BestTexelFactors;

	for (AActor* Actor : Context.Actors)
	{
		for (TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor); UPrimitiveComponent* Component : PrimitiveComponents)
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

			// The streamer's own view of this component: which textures it uses and
			// how far each is stretched. Empty unless the level's texture streaming
			// data has been built, which is why every use of it has a fallback.
			FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::High, Component);
			TArray<FStreamingRenderAssetPrimitiveInfo> StreamingInfos;
			Component->GetStreamingRenderAssetInfoWithNULLRemoval(LevelContext, StreamingInfos);

			for (const FStreamingRenderAssetPrimitiveInfo& Info : StreamingInfos)
			{
				UTexture2D* Texture = Cast<UTexture2D>(Info.RenderAsset);
				if (!Texture || Info.TexelFactor <= 0.0f)
				{
					continue;
				}

				float& Best = BestTexelFactors.FindOrAdd(Texture, 0.0f);
				Best = FMath::Max(Best, Info.TexelFactor);
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
		const int32 EffectiveLongestSide = Texture->MaxTextureSize > 0 ? FMath::Min(LongestSide, Texture->MaxTextureSize) : LongestSide;
		const bool bSpecialPurpose = IsSpecialPurposeTextureGroup(Texture->LODGroup);
		const bool bPowerOfTwo = FMath::IsPowerOfTwo(Width) && FMath::IsPowerOfTwo(Height);
		const FText Subject = FText::Format(LOCTEXT("TextureSubject", "{0} ({1} x {2})"),
			FText::FromString(Texture->GetName()), FText::AsNumber(Width), FText::AsNumber(Height));

		// --- Oversized ---------------------------------------------------------
		//
		// Preferred rule: how many texels the texture actually delivers per metre of
		// surface. Falls back to judging the texture's own dimensions only when the
		// streaming data needed for the real answer isn't there — and says so, since
		// the fallback genuinely cannot tell an 8k skybox from an 8k bolt.
		const float* BestTexelFactor = BestTexelFactors.Find(Texture);
		if (const bool bHaveDensity = BestTexelFactor != nullptr && *BestTexelFactor > 0.0f)
		{
			const float Density = TexelsPerMetre(EffectiveLongestSide, *BestTexelFactor);
			if (Density > T.TextureDensityBudget)
			{
				const ESeverity Severity = (Texture->VirtualTextureStreaming || bSpecialPurpose) ? ESeverity::Minor : (Density > T.TextureDensityBudget * 2 ? ESeverity::Major : ESeverity::Minor);

				FFinding F(TEXT("Texture.Oversized"), Severity, ECategory::Textures, EFindingScope::Asset,
					LOCTEXT("OverDenseTextureTitle", "Texture resolution exceeds what the surface shows"), Subject);
				F.WhyItMatters = FText::Format(
					LOCTEXT("OverDenseTextureWhy", "Even where it is stretched furthest, this texture delivers about {0} texels per metre of surface against a budget of {1}. Texels nobody can see still cost memory, streaming and build time."),
					FText::AsNumber(FMath::RoundToInt(Density)), FText::AsNumber(T.TextureDensityBudget));
				F.HowToFix = LOCTEXT("OverDenseTextureFix", "Halve the source resolution (or set Max Texture Size) — at this density it would look identical.");
				F.TargetActor = Pair.Value;
				F.TargetAsset = Texture;
				Out.Findings.Add(MoveTemp(F));
			}
		}
		else if (EffectiveLongestSide > T.OversizedTextureSize)
		{
			const ESeverity Severity = Texture->VirtualTextureStreaming || bSpecialPurpose ? ESeverity::Minor : ESeverity::Major;
			FFinding F(TEXT("Texture.Oversized"), Severity, ECategory::Textures, EFindingScope::Asset,
				LOCTEXT("OversizedTextureTitle", "Texture exceeds the configured size limit"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("OversizedTextureWhy", "Its effective longest side is {0}px; large textures increase memory, streaming, and build cost. This level has no texture streaming data, so this judges the texture's own size — it cannot tell a huge surface from a tiny one."),
				FText::AsNumber(EffectiveLongestSide));
			F.HowToFix = LOCTEXT("OversizedTextureFix", "Run Build > Build Texture Streaming for a verdict based on how large this texture actually appears; otherwise check texel density by hand before reducing the source resolution.");
			F.TargetActor = Pair.Value;
			F.TargetAsset = Texture;
			Out.Findings.Add(MoveTemp(F));
		}

		if (!bSpecialPurpose && !bPowerOfTwo
			&& Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::None)
		{
			FFinding F(TEXT("Texture.NonPowerOfTwo"), ESeverity::Minor, ECategory::Textures, EFindingScope::Asset,
				LOCTEXT("NonPowerOfTwoTitle", "Texture dimensions are not power-of-two"), Subject);
			F.WhyItMatters = LOCTEXT("NonPowerOfTwoWhy", "Unpadded dimensions can prevent a complete mip chain and make streaming less efficient.");
			F.HowToFix = LOCTEXT("NonPowerOfTwoFix", "Resize the source or choose an appropriate Padding and Resizing mode in the texture asset.");
			F.TargetActor = Pair.Value;
			F.TargetAsset = Texture;
			Out.Findings.Add(MoveTemp(F));
		}

		// --- Non-streaming and large -------------------------------------------
		//
		// A NeverStream texture is fully resident in VRAM for the level's whole
		// lifetime — streaming can never trim it — so a large one is a fixed memory
		// cost, unlike a streamable texture that only pays for the mips in view.
		// Special-purpose groups (UI, lightmaps, render targets) are never-stream by
		// design and are left alone.
		if (!bSpecialPurpose && Texture->NeverStream && !Texture->VirtualTextureStreaming)
		{
			if (const int64 FullBytes = Texture->CalcTextureMemorySizeEnum(TMC_AllMips); FullBytes > NonStreamingBudgetBytes)
			{
				const ESeverity Severity = FullBytes > NonStreamingBudgetBytes * 4 ? ESeverity::Major : ESeverity::Minor;
				FFinding F(TEXT("Texture.NonStreaming"), Severity, ECategory::Textures, EFindingScope::Asset,
					LOCTEXT("NonStreamingTitle", "Large texture is set to never stream"), Subject);
				F.WhyItMatters = FText::Format(
					LOCTEXT("NonStreamingWhy", "Never Stream keeps all {0} of this texture resident in memory for the whole level; streaming can't reclaim any of it."),
					FText::AsMemory(FullBytes));
				F.HowToFix = LOCTEXT("NonStreamingFix", "Turn Never Stream off so its mips stream with distance, unless it must be pin-sharp at all times (a UI or decal atlas).");
				F.TargetActor = Pair.Value;
				F.TargetAsset = Texture;
				F.FixId = TEXT("Fix_EnableStreaming");
				Out.Findings.Add(MoveTemp(F));
			}
		}

		if (!bSpecialPurpose && bPowerOfTwo && LongestSide >= 1024 && Texture->MipGenSettings == TMGS_NoMipmaps && !Texture->VirtualTextureStreaming)
		{
			const ESeverity Severity = LongestSide >= 2048 ? ESeverity::Major : ESeverity::Minor;
			FFinding F(TEXT("Texture.MipsDisabled"), Severity, ECategory::Textures, EFindingScope::Asset, LOCTEXT("MissingTextureMipsTitle", "Large texture has mipmaps disabled"), Subject);
			F.WhyItMatters = LOCTEXT("MissingTextureMipsWhy", "Rendering the full-resolution texture at every distance wastes bandwidth and can shimmer.");
			F.HowToFix = LOCTEXT("MissingTextureMipsFix", "Use FromTextureGroup mip generation unless this asset intentionally requires exact texels.");
			F.TargetActor = Pair.Value;
			F.TargetAsset = Texture;
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

#undef LOCTEXT_NAMESPACE
