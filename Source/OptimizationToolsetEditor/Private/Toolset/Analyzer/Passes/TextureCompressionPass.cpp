// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/TextureCompressionPass.h"

#include "SceneTypes.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Components/MeshComponent.h"

#define LOCTEXT_NAMESPACE "TextureCompressionPass"

namespace
{
	/** Which material input a texture was found feeding, and who to blame for it. */
	struct FTextureRoles
	{
		TSet<UTexture2D*> Normals;
		TSet<UTexture2D*> Data;		// roughness / metallic / AO / specular
		TSet<UTexture2D*> Colour;	// base colour / emissive
		TMap<UTexture2D*, TWeakObjectPtr<AActor>> Owners;
	};

	void GatherRole(UMaterialInterface& Material, EMaterialProperty Property,
		TSet<UTexture2D*>& OutSet, FTextureRoles& Roles, AActor* Owner)
	{
		TArray<UTexture*> Textures;
		if (!Material.GetTexturesInPropertyChain(Property, Textures, nullptr, nullptr))
		{
			return;
		}

		for (UTexture* Texture : Textures)
		{
			UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
			if (!Texture2D || Texture2D->GetPathName().StartsWith(TEXT("/Engine/")))
			{
				continue;
			}

			OutSet.Add(Texture2D);
			if (!Roles.Owners.Contains(Texture2D))
			{
				Roles.Owners.Add(Texture2D, Owner);
			}
		}
	}
}

void FTextureCompressionPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	FTextureRoles Roles;
	TSet<UMaterialInterface*> Seen;

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
				if (!Material || Seen.Contains(Material))
				{
					continue;
				}
				Seen.Add(Material);

				GatherRole(*Material, MP_Normal, Roles.Normals, Roles, Actor);

				GatherRole(*Material, MP_Roughness, Roles.Data, Roles, Actor);
				GatherRole(*Material, MP_Metallic, Roles.Data, Roles, Actor);
				GatherRole(*Material, MP_AmbientOcclusion, Roles.Data, Roles, Actor);
				GatherRole(*Material, MP_Specular, Roles.Data, Roles, Actor);

				GatherRole(*Material, MP_BaseColor, Roles.Colour, Roles, Actor);
				GatherRole(*Material, MP_EmissiveColor, Roles.Colour, Roles, Actor);
			}
		}
	}

	// --- Normal maps not compressed as normal maps.
	for (UTexture2D* Texture : Roles.Normals)
	{
		if (Texture->CompressionSettings == TC_Normalmap)
		{
			continue;
		}

		FFinding F(TEXT("Texture.NormalMapWrongCompression"), ESeverity::Major, ECategory::Textures,
			LOCTEXT("NormalTitle", "Normal map is not using Normalmap compression"),
			FText::FromString(Texture->GetName()));
		F.WhyItMatters = LOCTEXT("NormalWhy", "This texture feeds a material's Normal input but is compressed as colour, which both wastes memory and mangles the normals it stores.");
		F.HowToFix = LOCTEXT("NormalFix", "Set Compression Settings to Normalmap on the texture (one-click fix available).");
		F.TargetActor = Roles.Owners.FindRef(Texture);
		F.TargetAsset = Texture;
		F.FixId = TEXT("Fix_NormalmapCompression");
		Out.Findings.Add(MoveTemp(F));
	}

	// --- Data textures interpreted as colour.
	for (UTexture2D* Texture : Roles.Data)
	{
		if (!Texture->SRGB)
		{
			continue;
		}

		// A texture feeding both colour and data is packed on purpose (colour in
		// RGB, roughness in alpha). sRGB is a real trade-off there, not a mistake
		// to correct behind the user's back.
		if (Roles.Colour.Contains(Texture))
		{
			continue;
		}

		FFinding F(TEXT("Texture.DataTextureSRGB"), ESeverity::Major, ECategory::Textures,
			LOCTEXT("DataTitle", "Data texture is treated as sRGB"), FText::FromString(Texture->GetName()));
		F.WhyItMatters = LOCTEXT("DataWhy", "This texture feeds roughness, metallic, AO or specular, which are numbers rather than colour. Decoding it through sRGB bends those numbers.");
		F.HowToFix = LOCTEXT("DataFix", "Turn sRGB off on the texture (one-click fix available).");
		F.TargetActor = Roles.Owners.FindRef(Texture);
		F.TargetAsset = Texture;
		F.FixId = TEXT("Fix_DisableTextureSRGB");
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
