// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Analyzer/Passes/BlueprintDependencyPass.h"

#include "AssetManagerEditorModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

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

	bool IsWalkableDependency(IAssetRegistry& AssetRegistry, FName PackageName)
	{
		const FString Path = PackageName.ToString();

		// /Script packages are code, not content: they carry no disk size and
		// walking them just drags the whole module graph in for nothing.
		if (Path.StartsWith(TEXT("/Script/")))
		{
			return false;
		}

		// A hard World reference should not turn a Blueprint check into a level
		// size report. Stop at map packages entirely: omitting only the .umap's own
		// bytes while still walking its dependencies would still charge the
		// Blueprint for nearly every asset placed in that level.
		TArray<FAssetData> AssetsInPackage;
		AssetRegistry.GetAssetsByPackageName(PackageName, AssetsInPackage);
		for (const FAssetData& Asset : AssetsInPackage)
		{
			if ((Asset.PackageFlags & PKG_ContainsMap) != 0)
			{
				return false;
			}
		}

		return true;
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

		// Hard references only, and it has to be asked for: EDependencyCategory::Package
		// with a default FDependencyQuery returns hard *and* soft dependencies. That
		// was this pass's original bug — a soft reference is precisely the thing that
		// does *not* load with the Blueprint, so counting one inflated the chain and
		// then advised making soft something that already was.
		//
		// (Note this is the opposite lever from the ::All gotcha in HANDOFF: that one
		// is about the dependency *category*, this is about the query *flags*.)
		const UE::AssetRegistry::FDependencyQuery HardOnly(UE::AssetRegistry::EDependencyQuery::Hard);

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
				UE::AssetRegistry::EDependencyCategory::Package, HardOnly);

			for (const FName Dependency : Dependencies)
			{
				if (Visited.Contains(Dependency))
				{
					continue;
				}

				// Mark rejected packages as visited too, so a level referenced through
				// several branches is classified only once.
				Visited.Add(Dependency);
				if (IsWalkableDependency(AssetRegistry, Dependency))
				{
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

	/**
	 * Says the check couldn't run, instead of returning quietly.
	 *
	 * Every way this pass can fail looks exactly like a clean level from the
	 * outside, which is the most expensive kind of wrong a tool can be: it costs
	 * trust in every *other* finding too. This turns each of those exits into a
	 * visible row that names the reason and what to do about it.
	 */
	FFinding MakeUnavailableFinding(const FText& Reason)
	{
		FFinding F(TEXT("Blueprint.DependencyCheckUnavailable"), ESeverity::Minor, ECategory::Blueprints, EFindingScope::System,
			LOCTEXT("UnavailableTitle", "Blueprint dependency check did not run"),
			LOCTEXT("UnavailableSubject", "Dependency chain sizes"));
		F.WhyItMatters = FText::Format(
			LOCTEXT("UnavailableWhy", "This scan could not measure what Blueprints drag in, because {0}. No news here is not good news — heavy Blueprints would simply be missing from the results."),
			Reason);
		F.HowToFix = LOCTEXT("UnavailableFix", "Wait for the editor to finish scanning assets, then scan again.");
		return F;
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
		Out.Findings.Add(MakeUnavailableFinding(
			LOCTEXT("NoAssetManagerEditor", "the Asset Manager Editor plugin is not enabled")));
		return;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Partial dependency data would under-report the chain, which reads as "this
	// Blueprint is cheap" — the opposite of what the pass exists to say.
	if (AssetRegistry.IsLoadingAssets())
	{
		Out.Findings.Add(MakeUnavailableFinding(
			LOCTEXT("RegistryLoading", "the editor is still scanning the project's assets")));
		return;
	}

	IAssetManagerEditorModule& EditorModule = IAssetManagerEditorModule::Get();

	// Force the registry source to initialize before asking it for any size.
	//
	// FAssetManagerEditorModule::StartupModule() leaves CurrentRegistrySource null
	// and only fills it lazily, from GetCurrentRegistrySource() or
	// SetCurrentRegistrySource(). GetIntegerValueForCustomColumn() does *not* — it
	// reads CurrentRegistrySource directly and returns false while it is still
	// null. So without this line every size came back 0, every chain measured 0
	// bytes, and the pass reported nothing at any threshold — until something else
	// in the editor (opening a Size Map, say) happened to initialize it first, at
	// which point the same scan suddenly worked. This one call is the whole fix.
	const FAssetManagerEditorRegistrySource* RegistrySource = EditorModule.GetCurrentRegistrySource();
	if (!RegistrySource || !RegistrySource->HasRegistry())
	{
		Out.Findings.Add(MakeUnavailableFinding(
			LOCTEXT("NoRegistrySource", "no asset registry source is available yet")));
		return;
	}

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

		FFinding F(TEXT("Blueprint.HeavyDependencyChain"), Severity, ECategory::Blueprints, EFindingScope::Asset,
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
		if (const AActor* Owner = Pair.Value.Get())
		{
			F.TargetAsset = Owner->GetClass()->ClassGeneratedBy;
		}
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
