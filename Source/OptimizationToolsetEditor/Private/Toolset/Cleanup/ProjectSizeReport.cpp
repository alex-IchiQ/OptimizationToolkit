// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Cleanup/ProjectSizeReport.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "ProjectSizeReport"

namespace
{
	/**
	 * Class-to-category table. Listed explicitly rather than matched by name
	 * prefix: "Material" would also swallow MaterialParameterCollection, and
	 * anything unlisted should land in Other rather than be guessed at.
	 */
	EAssetCategory CategoryForClassImpl(FName ClassName)
	{
		static const TMap<FName, EAssetCategory> Table = {
			// Textures
			{ TEXT("Texture2D"),                  EAssetCategory::Textures },
			{ TEXT("TextureCube"),                EAssetCategory::Textures },
			{ TEXT("Texture2DArray"),             EAssetCategory::Textures },
			{ TEXT("TextureCubeArray"),           EAssetCategory::Textures },
			{ TEXT("VolumeTexture"),              EAssetCategory::Textures },
			{ TEXT("SparseVolumeTexture"),        EAssetCategory::Textures },
			{ TEXT("TextureRenderTarget2D"),      EAssetCategory::Textures },
			{ TEXT("TextureRenderTargetCube"),    EAssetCategory::Textures },
			{ TEXT("MediaTexture"),               EAssetCategory::Textures },

			// Static meshes
			{ TEXT("StaticMesh"),                 EAssetCategory::StaticMeshes },

			// Skeletal meshes and their rigging
			{ TEXT("SkeletalMesh"),               EAssetCategory::SkeletalMeshes },
			{ TEXT("Skeleton"),                   EAssetCategory::SkeletalMeshes },
			{ TEXT("PhysicsAsset"),               EAssetCategory::SkeletalMeshes },

			// Materials
			{ TEXT("Material"),                   EAssetCategory::Materials },
			{ TEXT("MaterialInstanceConstant"),   EAssetCategory::Materials },
			{ TEXT("MaterialFunction"),           EAssetCategory::Materials },
			{ TEXT("MaterialFunctionInstance"),   EAssetCategory::Materials },
			{ TEXT("MaterialParameterCollection"),EAssetCategory::Materials },
			{ TEXT("SubsurfaceProfile"),          EAssetCategory::Materials },

			// Animation (AnimBlueprint sits here: it is animation content first)
			{ TEXT("AnimSequence"),               EAssetCategory::Animations },
			{ TEXT("AnimMontage"),                EAssetCategory::Animations },
			{ TEXT("AnimComposite"),              EAssetCategory::Animations },
			{ TEXT("AnimBlueprint"),              EAssetCategory::Animations },
			{ TEXT("BlendSpace"),                 EAssetCategory::Animations },
			{ TEXT("BlendSpace1D"),               EAssetCategory::Animations },
			{ TEXT("PoseAsset"),                  EAssetCategory::Animations },

			// Audio
			{ TEXT("SoundWave"),                  EAssetCategory::Audio },
			{ TEXT("SoundCue"),                   EAssetCategory::Audio },
			{ TEXT("SoundClass"),                 EAssetCategory::Audio },
			{ TEXT("SoundMix"),                   EAssetCategory::Audio },
			{ TEXT("SoundAttenuation"),           EAssetCategory::Audio },
			{ TEXT("SoundConcurrency"),           EAssetCategory::Audio },
			{ TEXT("MetaSoundSource"),            EAssetCategory::Audio },
			{ TEXT("MetaSoundPatch"),             EAssetCategory::Audio },

			// Blueprints and script-side data
			{ TEXT("Blueprint"),                  EAssetCategory::Blueprints },
			{ TEXT("WidgetBlueprint"),            EAssetCategory::Blueprints },
			{ TEXT("BlueprintGeneratedClass"),    EAssetCategory::Blueprints },
			{ TEXT("UserDefinedStruct"),          EAssetCategory::Blueprints },
			{ TEXT("UserDefinedEnum"),            EAssetCategory::Blueprints },

			// Levels
			{ TEXT("World"),                      EAssetCategory::Levels },
			{ TEXT("LevelInstance"),              EAssetCategory::Levels },
		};

		const EAssetCategory* Found = Table.Find(ClassName);
		return Found ? *Found : EAssetCategory::Other;
	}
}

EAssetCategory FProjectSizeReport::CategoryForClass(FName ClassName)
{
	return CategoryForClassImpl(ClassName);
}

FText FProjectSizeReport::LabelForCategory(EAssetCategory Category)
{
	switch (Category)
	{
	case EAssetCategory::Textures:       return LOCTEXT("CatTextures", "Textures");
	case EAssetCategory::StaticMeshes:   return LOCTEXT("CatStaticMeshes", "Static meshes");
	case EAssetCategory::SkeletalMeshes: return LOCTEXT("CatSkeletalMeshes", "Skeletal meshes");
	case EAssetCategory::Materials:      return LOCTEXT("CatMaterials", "Materials");
	case EAssetCategory::Animations:     return LOCTEXT("CatAnimations", "Animations");
	case EAssetCategory::Audio:          return LOCTEXT("CatAudio", "Audio");
	case EAssetCategory::Blueprints:     return LOCTEXT("CatBlueprints", "Blueprints");
	case EAssetCategory::Levels:         return LOCTEXT("CatLevels", "Levels");
	default:                             return LOCTEXT("CatOther", "Other");
	}
}

FProjectSizeReport FProjectSizeReport::Compute()
{
	FProjectSizeReport Report;

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Half a registry means half a project measured, which reads as "the project
	// shrank" rather than "we looked too early". Say so instead of guessing.
	if (AssetRegistry.IsLoadingAssets())
	{
		Report.bRegistryIncomplete = true;
		return Report;
	}

	const double StartTime = FPlatformTime::Seconds();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	// Size is a property of the package file, and one package can hold several
	// assets, so measure each package once and attribute it to the first asset
	// that names it.
	TMap<FName, FName> PackageToClass;
	PackageToClass.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		FName& ClassName = PackageToClass.FindOrAdd(Asset.PackageName);
		if (ClassName.IsNone())
		{
			ClassName = Asset.AssetClassPath.GetAssetName();
		}
	}

	TMap<EAssetCategory, FProjectSizeEntry> ByCategory;
	for (const TPair<FName, FName>& Pair : PackageToClass)
	{
		FString Filename;
		if (!FPackageName::DoesPackageExist(Pair.Key.ToString(), &Filename))
		{
			continue;	// in the registry but not on disk yet (newly created, unsaved)
		}

		const int64 Size = IFileManager::Get().FileSize(*Filename);
		if (Size <= 0)
		{
			continue;
		}

		const EAssetCategory Category = CategoryForClassImpl(Pair.Value);
		FProjectSizeEntry& Entry = ByCategory.FindOrAdd(Category);
		Entry.Category = Category;
		Entry.TotalBytes += Size;
		++Entry.PackageCount;

		Report.TotalBytes += Size;
		++Report.PackageCount;
	}

	ByCategory.GenerateValueArray(Report.Entries);
	Report.Entries.Sort([](const FProjectSizeEntry& A, const FProjectSizeEntry& B)
	{
		return A.TotalBytes > B.TotalBytes;
	});

	Report.ComputeSeconds = FPlatformTime::Seconds() - StartTime;
	return Report;
}

#undef LOCTEXT_NAMESPACE
