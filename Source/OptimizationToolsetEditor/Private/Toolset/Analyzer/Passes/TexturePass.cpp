// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/TexturePass.h"

#include "SceneTypes.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

#define LOCTEXT_NAMESPACE "TexturePass"

namespace
{
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

	for (AActor* Actor : Context.Actors)
	{
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
			FFinding F(TEXT("Texture.Oversized"), Severity, ECategory::Textures,
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
			FFinding F(TEXT("Texture.NonPowerOfTwo"), ESeverity::Minor, ECategory::Textures,
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
			FFinding F(TEXT("Texture.MipsDisabled"), Severity, ECategory::Textures,
				LOCTEXT("MissingTextureMipsTitle", "Large texture has mipmaps disabled"), Subject);
			F.WhyItMatters = LOCTEXT("MissingTextureMipsWhy", "Rendering the full-resolution texture at every distance wastes bandwidth and can shimmer.");
			F.HowToFix = LOCTEXT("MissingTextureMipsFix", "Use FromTextureGroup mip generation unless this asset intentionally requires exact texels.");
			F.TargetActor = Pair.Value;
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

#undef LOCTEXT_NAMESPACE
