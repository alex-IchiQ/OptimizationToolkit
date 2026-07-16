// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Cleanup/Actions/DeleteUnusedAssetsAction.h"

#include "ObjectTools.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "DeleteUnusedAssetsAction"

FText FDeleteUnusedAssetsAction::GetTitle() const
{
	return LOCTEXT("Title", "Delete unused assets");
}

FText FDeleteUnusedAssetsAction::GetDescription() const
{
	return LOCTEXT("Description",
		"Finds assets under /Game that nothing references, then opens Unreal's delete dialog so you can review them. "
		"Unreferenced is not the same as unused: anything loaded by soft path, by a config entry, or by name from a Blueprint "
		"has no reference on disk and will be listed here. Read the list before confirming.");
}

FText FDeleteUnusedAssetsAction::GetButtonLabel() const
{
	return LOCTEXT("Button", "Find unused");
}

FText FDeleteUnusedAssetsAction::Execute() const
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// A partial registry means partial reference data, and here a missing
	// reference reads as "safe to delete". Refuse rather than risk that.
	if (AssetRegistry.IsLoadingAssets())
	{
		return LOCTEXT("StillScanning", "The asset registry is still scanning the project — try again once it finishes.");
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	TArray<FAssetData> Unused;
	for (const FAssetData& Asset : Assets)
	{
		const FString PackagePath = Asset.PackageName.ToString();

		// World Partition stores one package per actor under these folders. The
		// map doesn't reference them the way a normal asset reference works, so
		// they look unreferenced — and deleting them deletes level actors.
		if (PackagePath.Contains(TEXT("/__ExternalActors__/"))
			|| PackagePath.Contains(TEXT("/__ExternalObjects__/")))
		{
			continue;
		}

		const FName ClassName = Asset.AssetClassPath.GetAssetName();

		// Maps are the roots everything else hangs off: nothing references them,
		// so every map in the project would qualify as "unused".
		if (ClassName == TEXT("World"))
		{
			continue;
		}

		// Redirectors are unreferenced by design once fixed up, and they have
		// their own action.
		if (ClassName == TEXT("ObjectRedirector"))
		{
			continue;
		}

		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(Asset.PackageName, Referencers);
		Referencers.Remove(Asset.PackageName);	// a package referencing itself proves nothing
		if (Referencers.Num() > 0)
		{
			continue;
		}

		Unused.Add(Asset);
	}

	if (Unused.IsEmpty())
	{
		return LOCTEXT("NoUnused", "No unreferenced assets found under /Game.");
	}

	// Unreal's own dialog lists every asset, re-checks references at delete time
	// and offers force delete. Hand-rolling that would be worse in every way.
	const int32 DeletedCount = ObjectTools::DeleteAssets(Unused, /*bShowConfirmation*/ true);

	if (DeletedCount <= 0)
	{
		return FText::Format(
			LOCTEXT("FoundNoneDeleted", "Found {0} unreferenced assets; nothing was deleted."),
			FText::AsNumber(Unused.Num()));
	}

	return FText::Format(
		LOCTEXT("Deleted", "Deleted {0} of {1} unreferenced assets."),
		FText::AsNumber(DeletedCount), FText::AsNumber(Unused.Num()));
}

#undef LOCTEXT_NAMESPACE
