// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Navigation/FindingNavigator.h"

#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/ToolsetModel.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "ISettingsModule.h"
#include "Layout/ChildrenBase.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "FindingNavigator"

namespace
{
	TSharedPtr<SSearchBox> FindSettingsSearchBox(const TSharedRef<SWidget>& Widget, bool bInsideSettingsEditor = false)
	{
		const FString WidgetType = Widget->GetTypeAsString();
		const bool bInsideSettings = bInsideSettingsEditor || WidgetType == TEXT("SSettingsEditor");
		if (bInsideSettings && WidgetType == TEXT("SSearchBox"))
		{
			return StaticCastSharedRef<SSearchBox>(Widget);
		}

		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SSearchBox> SearchBox = FindSettingsSearchBox(Children->GetChildAt(Index), bInsideSettings))
			{
				return SearchBox;
			}
		}
		return nullptr;
	}
}

bool FFindingNavigator::CanNavigate(const FFinding& Finding)
{
	if (Finding.TargetAsset.IsValid() || Finding.TargetActor.IsValid()
		|| Finding.Scope == EFindingScope::Project || Finding.Scope == EFindingScope::System)
	{
		return true;
	}

	return Finding.RelatedActors.ContainsByPredicate(
		[](const TWeakObjectPtr<AActor>& Actor) { return Actor.IsValid(); });
}

FText FFindingNavigator::GetActionLabel(const FFinding& Finding)
{
	if (Finding.TargetAsset.IsValid())
	{
		return LOCTEXT("ShowInContentButton", "Show in Content");
	}
	if (Finding.Scope == EFindingScope::Project)
	{
		return LOCTEXT("OpenSettingsButton", "Open Settings");
	}
	if (Finding.Scope == EFindingScope::System)
	{
		return LOCTEXT("ScanAgainButton", "Scan Again");
	}
	if (Finding.TargetActor.IsValid() || !Finding.RelatedActors.IsEmpty())
	{
		return LOCTEXT("FocusActorButton", "Focus Actor");
	}
	return LOCTEXT("UnavailableButton", "Unavailable");
}

void FFindingNavigator::Navigate(const FFinding& Finding, const TSharedPtr<FToolsetModel>& Model)
{
	if (Finding.Scope == EFindingScope::System)
	{
		if (Model.IsValid())
		{
			Model->RunScan();
		}
		return;
	}

	if (UObject* Asset = Finding.TargetAsset.Get())
	{
		if (GEditor)
		{
			GEditor->SyncBrowserToObject(Asset);
		}
		return;
	}
	if (Finding.TargetActor.IsValid())
	{
		FLevelAnalyzer::FocusActor(Finding.TargetActor);
		return;
	}
	for (const TWeakObjectPtr<AActor>& Actor : Finding.RelatedActors)
	{
		if (Actor.IsValid())
		{
			FLevelAnalyzer::FocusActor(Actor);
			return;
		}
	}
	if (Finding.Scope == EFindingScope::Project)
	{
		OpenProjectRenderingSettings(Finding.TypeId);
	}
}

void FFindingNavigator::OpenProjectRenderingSettings(FName FindingTypeId)
{
	FName SectionName = TEXT("Rendering");
	FText SearchText;

	if (FindingTypeId == TEXT("Project.OcclusionCullingDisabled"))
	{
		SearchText = LOCTEXT("SearchOcclusionCulling", "Occlusion Culling");
	}
	else if (FindingTypeId == TEXT("Project.AllShaderPermutations"))
	{
		SectionName = TEXT("Rendering Overrides");
		SearchText = LOCTEXT("SearchAllShaderPermutations", "Force all shader permutation support");
	}
	else if (FindingTypeId == TEXT("Project.StaticLightingWithLumen"))
	{
		SearchText = LOCTEXT("SearchAllowStaticLighting", "Allow Static Lighting");
	}
	else if (FindingTypeId == TEXT("Project.RayTracingEnabled"))
	{
		SearchText = LOCTEXT("SearchHardwareRayTracing", "Support Hardware Ray Tracing");
	}
	else if (FindingTypeId == TEXT("Project.RayTracingWithoutSkinCache"))
	{
		SearchText = LOCTEXT("SearchComputeSkinCache", "Support Compute Skin Cache");
	}

	FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"))
		.ShowViewer(TEXT("Project"), TEXT("Engine"), SectionName);

	// The Settings Editor resets its filter while changing sections. Retry until
	// that navigation has settled, including when the tab is spawned for the first time.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[SearchText, Attempts = 0](float) mutable
		{
			TArray<TSharedRef<SWindow>> VisibleWindows;
			FSlateApplication::Get().GetAllVisibleWindowsOrdered(VisibleWindows);
			for (const TSharedRef<SWindow>& Window : VisibleWindows)
			{
				if (TSharedPtr<SSearchBox> SearchBox = FindSettingsSearchBox(Window))
				{
					SearchBox->SetText(SearchText);
					FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
					return false;
				}
			}

			return ++Attempts < 10;
		}), 0.0f);
}

#undef LOCTEXT_NAMESPACE
