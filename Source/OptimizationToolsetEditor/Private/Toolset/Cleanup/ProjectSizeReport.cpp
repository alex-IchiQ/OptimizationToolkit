// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Cleanup/ProjectSizeReport.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

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

	TMap<FName, FProjectSizeEntry> ByClass;
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

		FProjectSizeEntry& Entry = ByClass.FindOrAdd(Pair.Value);
		Entry.ClassName = Pair.Value;
		Entry.TotalBytes += Size;
		++Entry.PackageCount;

		Report.TotalBytes += Size;
		++Report.PackageCount;
	}

	ByClass.GenerateValueArray(Report.Entries);
	Report.Entries.Sort([](const FProjectSizeEntry& A, const FProjectSizeEntry& B)
	{
		return A.TotalBytes > B.TotalBytes;
	});

	Report.ComputeSeconds = FPlatformTime::Seconds() - StartTime;
	return Report;
}
