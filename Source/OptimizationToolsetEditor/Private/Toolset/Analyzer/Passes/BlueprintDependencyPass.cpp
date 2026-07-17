// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/BlueprintDependencyPass.h"

#include "AssetManagerEditorModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "BlueprintDependencyPass"

namespace
{
	/** One package in a chain, and what it costs on disk. */
	struct FChainEntry
	{
		FName PackageName;
		int64 Bytes = 0;
	};

	/** Everything a root package drags in with it. */
	struct FChainResult
	{
		int64 TotalBytes = 0;
		int32 PackageCount = 0;
		TArray<FChainEntry> Entries;	// sorted heaviest first once gathered
	};

	bool IsWalkableDependency(FName PackageName)
	{
		const FString Path = PackageName.ToString();

		// /Script packages are code, not content: they carry no disk size and
		// walking them just drags the whole module graph in for nothing.
		return !Path.StartsWith(TEXT("/Script/"));
	}

	int64 GetDiskSize(IAssetManagerEditorModule& EditorModule, IAssetRegistry& AssetRegistry, FName PackageName)
	{
		TArray<FAssetData> AssetsInPackage;
		AssetRegistry.GetAssetsByPackageName(PackageName, AssetsInPackage);
		if (AssetsInPackage.IsEmpty())
		{
			return 0;
		}

		int64 Size = 0;
		if (EditorModule.GetIntegerValueForCustomColumn(
			AssetsInPackage[0], IAssetManagerEditorModule::DiskSizeName, Size) && Size > 0)
		{
			return Size;
		}
		return 0;
	}

	/** Walks hard package references from Root, summing what each one costs. */
	FChainResult GatherChain(IAssetManagerEditorModule& EditorModule, IAssetRegistry& AssetRegistry, FName Root)
	{
		FChainResult Result;

		TSet<FName> Visited;
		TArray<FName> Pending;
		Visited.Add(Root);
		Pending.Add(Root);

		while (!Pending.IsEmpty())
		{
			const FName PackageName = Pending.Pop(EAllowShrinking::No);

			// The root is what we are measuring the cost *of*, so its own size
			// is not part of what it drags in.
			if (PackageName != Root)
			{
				const int64 Bytes = GetDiskSize(EditorModule, AssetRegistry, PackageName);
				if (Bytes > 0)
				{
					Result.TotalBytes += Bytes;
					++Result.PackageCount;
					Result.Entries.Add({ PackageName, Bytes });
				}
			}

			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(PackageName, Dependencies,
				UE::AssetRegistry::EDependencyCategory::Package);

			for (const FName Dependency : Dependencies)
			{
				if (IsWalkableDependency(Dependency) && !Visited.Contains(Dependency))
				{
					Visited.Add(Dependency);
					Pending.Add(Dependency);
				}
			}
		}

		Result.Entries.Sort([](const FChainEntry& A, const FChainEntry& B)
		{
			return A.Bytes > B.Bytes;
		});
		return Result;
	}

	/** "T_Sky_8K (210 MB), SM_Rock (40 MB)" — the assets worth looking at first. */
	FText DescribeHeaviest(const FChainResult& Chain)
	{
		TArray<FText> Parts;
		const int32 Count = FMath::Min(3, Chain.Entries.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FChainEntry& Entry = Chain.Entries[Index];
			Parts.Add(FText::Format(
				LOCTEXT("HeaviestEntry", "{0} ({1})"),
				FText::FromString(FPackageName::GetShortName(Entry.PackageName)),
				FText::AsMemory(Entry.Bytes)));
		}
		return FText::Join(LOCTEXT("HeaviestSeparator", ", "), Parts);
	}
}

void FBlueprintDependencyPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	if (!IAssetManagerEditorModule::IsAvailable())
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Partial dependency data would under-report the chain, which reads as "this
	// Blueprint is cheap" — the opposite of what the pass exists to say.
	if (AssetRegistry.IsLoadingAssets())
	{
		return;
	}

	IAssetManagerEditorModule& EditorModule = IAssetManagerEditorModule::Get();
	const int64 ThresholdBytes = static_cast<int64>(T.DependencyChainSizeMB) * 1024 * 1024;

	// One entry per Blueprint class: the chain is a property of the asset, so a
	// hundred placed copies are still one thing to fix.
	TMap<FName, TWeakObjectPtr<AActor>> BlueprintPackages;
	for (AActor* Actor : Context.Actors)
	{
		if (!Actor)
		{
			continue;
		}

		const UClass* Class = Actor->GetClass();
		if (!Class || Class->ClassGeneratedBy == nullptr)
		{
			continue;
		}

		const FName PackageName = Class->ClassGeneratedBy->GetPackage()->GetFName();
		if (!BlueprintPackages.Contains(PackageName))
		{
			BlueprintPackages.Add(PackageName, Actor);
		}
	}

	for (const TPair<FName, TWeakObjectPtr<AActor>>& Pair : BlueprintPackages)
	{
		const FChainResult Chain = GatherChain(EditorModule, AssetRegistry, Pair.Key);
		if (Chain.TotalBytes < ThresholdBytes)
		{
			continue;
		}

		const ESeverity Severity = Chain.TotalBytes >= ThresholdBytes * 4
			? ESeverity::Major : ESeverity::Minor;

		FFinding F(TEXT("Blueprint.HeavyDependencyChain"), Severity, ECategory::Blueprints,
			LOCTEXT("Title", "Blueprint pulls in a large dependency chain"),
			FText::Format(LOCTEXT("Subject", "{0} — {1} across {2} assets"),
				FText::FromString(FPackageName::GetShortName(Pair.Key)),
				FText::AsMemory(Chain.TotalBytes),
				FText::AsNumber(Chain.PackageCount)));
		F.WhyItMatters = FText::Format(
			LOCTEXT("Why", "Everything hard-referenced by this Blueprint loads with it, so touching it costs {0} of memory and load time even if most of it is never used."),
			FText::AsMemory(Chain.TotalBytes));
		F.HowToFix = FText::Format(
			LOCTEXT("Fix", "Heaviest: {0}. Make the ones that aren't needed immediately soft references (TSoftObjectPtr) and load them on demand."),
			DescribeHeaviest(Chain));
		F.TargetActor = Pair.Value;
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
