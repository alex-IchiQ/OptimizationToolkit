// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Cleanup/Actions/FixUpRedirectorsAction.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"

#define LOCTEXT_NAMESPACE "FixUpRedirectorsAction"

FText FFixUpRedirectorsAction::GetTitle() const
{
	return LOCTEXT("Title", "Fix up redirectors");
}

FText FFixUpRedirectorsAction::GetDescription() const
{
	return LOCTEXT("Description", "Re-points every reference at the asset it was moved or renamed to, then deletes the leftover redirectors. This resaves referencing assets and cannot be undone.");
}

FText FFixUpRedirectorsAction::GetButtonLabel() const
{
	return LOCTEXT("Button", "Fix up");
}

FText FFixUpRedirectorsAction::Execute() const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// A partial registry would silently miss redirectors, which looks like the
	// action "worked" while leaving the project half-fixed.
	if (AssetRegistry.IsLoadingAssets())
	{
		return LOCTEXT("StillScanning", "The asset registry is still scanning the project — try again once it finishes.");
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

	TArray<FAssetData> RedirectorAssets;
	AssetRegistry.GetAssets(Filter, RedirectorAssets);
	if (RedirectorAssets.IsEmpty())
	{
		return LOCTEXT("NoRedirectors", "No redirectors found under /Game — nothing to fix up.");
	}

	TArray<UObjectRedirector*> Redirectors;
	Redirectors.Reserve(RedirectorAssets.Num());
	for (const FAssetData& RedirectorAsset : RedirectorAssets)
	{
		if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(RedirectorAsset.GetAsset()))
		{
			Redirectors.Add(Redirector);
		}
	}

	if (Redirectors.IsEmpty())
	{
		return LOCTEXT("NoRedirectorsLoaded", "Found redirector entries, but none could be loaded.");
	}

	const int32 Count = Redirectors.Num();
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().FixupReferencers(Redirectors);

	return FText::Format(
		LOCTEXT("FixedUp", "Fixed up {0} redirectors under /Game."), FText::AsNumber(Count));
}

#undef LOCTEXT_NAMESPACE
