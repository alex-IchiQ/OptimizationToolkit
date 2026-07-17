// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SOptimizePanel.h"
#include "Toolset/Panels/SFindingTree.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "DetailsViewArgs.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "ISettingsModule.h"
#include "Layout/ChildrenBase.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Containers/Ticker.h"
#include "UObject/UnrealType.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "SOptimizePanel"

using namespace ToolsetUI;

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

void SOptimizePanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowPropertyMatrixButton = false;
	DetailsArgs.bShowScrollBar = false;
	DetailsArgs.bHideSelectionTip = true;

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	SettingsView = PropertyEditor.CreateDetailView(DetailsArgs);
	SettingsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SOptimizePanel::IsSettingVisible));
	SettingsView->OnFinishedChangingProperties().AddSP(this, &SOptimizePanel::OnSettingChanged);
	SettingsView->SetObject(GetMutableDefault<UOptimizationToolsetSettings>());

	ChildSlot
	[
		SNew(SVerticalBox)

		// Search and severity filters apply to the unified findings tree.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(22, 16, 22, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Filter findings…"))
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					if (Model.IsValid())
					{
						Model->SetSearchFilter(NewText.ToString());
					}
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10, 0, 0, 0)).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))[ MakeSeverityFilterButton(ESeverity::Critical) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))[ MakeSeverityFilterButton(ESeverity::Major) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))[ MakeSeverityFilterButton(ESeverity::Minor) ]
			]
		]

		// Category-specific thresholds come before the category's object tree.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(22, 6, 22, 8))
		[
			BuildSettingsPanel()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(16, 0, 16, 16))
		[
			SAssignNew(Tree, SFindingTree)
			.OnMakeCard(FOnMakeFindingCard::CreateSP(this, &SOptimizePanel::MakeFindingCard))
		]
	];

	if (Model.IsValid())
	{
		ChangedHandle = Model->OnChanged().AddSP(this, &SOptimizePanel::Refresh);
		Refresh();
	}
}

SOptimizePanel::~SOptimizePanel()
{
	if (Model.IsValid())
	{
		Model->OnChanged().Remove(ChangedHandle);
	}
}

void SOptimizePanel::Refresh()
{
	if (Model.IsValid() && Tree.IsValid())
	{
		Tree->SetFindings(Model->GetVisibleFindings());
	}
	const TOptional<ECategory> CurrentCategory = Model.IsValid()
		? Model->GetCategoryFilter() : TOptional<ECategory>();
	const bool bCategoryChanged = CurrentCategory.IsSet() != DisplayedSettingsCategory.IsSet()
		|| (CurrentCategory.IsSet() && CurrentCategory.GetValue() != DisplayedSettingsCategory.GetValue());
	if (SettingsView.IsValid() && bCategoryChanged)
	{
		DisplayedSettingsCategory = CurrentCategory;
		SettingsView->ForceRefresh();
	}
}

TSharedRef<SWidget> SOptimizePanel::BuildSettingsPanel()
{
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(14, 10))
		.Visibility_Lambda([this]()
		{
			return Model.IsValid() && Model->GetCategoryFilter().IsSet()
				? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Heading")
				.Text(this, &SOptimizePanel::GetSettingsTitle)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				SNew(SBox)
				.MaxDesiredHeight(210.0f)
				.Visibility_Lambda([this]()
				{
					return HasSettingsForSelectedCategory() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SettingsView.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]()
				{
					return HasSettingsForSelectedCategory() ? EVisibility::Collapsed : EVisibility::Visible;
				})

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
					.AutoWrapText(true)
					.Text(this, &SOptimizePanel::GetNoSettingsText)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12, 0, 0, 0))
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Ghost")
					.Visibility_Lambda([this]()
					{
						return Model.IsValid() && Model->GetCategoryFilter().IsSet()
							&& Model->GetCategoryFilter().GetValue() == ECategory::Project
							? EVisibility::Visible : EVisibility::Collapsed;
					})
					.OnClicked_Lambda([]()
					{
						OpenProjectRenderingSettings();
						return FReply::Handled();
					})
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
						.Text(LOCTEXT("ProjectSettingsButton", "Open Rendering Settings"))
					]
				]
			]
		];
}

TSharedRef<SWidget> SOptimizePanel::MakeSeverityFilterButton(ESeverity Severity)
{
	const FLinearColor Color = FToolsetStyle::ColorForSeverity(Severity);
	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Button.Ghost")
		.OnClicked_Lambda([this, Severity]()
		{
			if (Model.IsValid())
			{
				Model->ToggleSeverity(Severity);
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.TextStyle(&S(), "Toolset.Text.Body")
			.Text(FToolsetStyle::LabelForSeverity(Severity))
			.ColorAndOpacity_Lambda([this, Severity, Color]()
			{
				return Model.IsValid() && Model->IsSeverityEnabled(Severity)
					? FSlateColor(Color) : FSlateColor(FToolsetStyle::TextSecondary);
			})
		];
}

TSharedRef<SWidget> SOptimizePanel::MakeFindingCard(TSharedPtr<FFinding> Item)
{
	const FLinearColor SevColor = FToolsetStyle::ColorForSeverity(Item->Severity);
	const bool bCanFix = FToolsetModel::HasSupportedFix(*Item);
	IOptimizationFix* Fix = bCanFix ? FToolsetRegistry::Get().FindFix(Item->FixId) : nullptr;
	const FText FixLabel = Fix ? Fix->GetLabel() : LOCTEXT("FixGeneric", "Apply");

	TSharedRef<SHorizontalBox> Pills = SNew(SHorizontalBox);
	Pills->AddSlot().AutoWidth().Padding(FMargin(0, 0, 6, 0))
	[
		SNew(SBorder)
		.BorderImage(Brush("Toolset.Fill.Pill"))
		.BorderBackgroundColor(FSlateColor(FLinearColor(SevColor.R, SevColor.G, SevColor.B, 0.18f)))
		.Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(FToolsetStyle::LabelForSeverity(Item->Severity)).ColorAndOpacity(FSlateColor(SevColor))
		]
	];
	Pills->AddSlot().AutoWidth()
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(FToolsetStyle::LabelForCategory(Item->Category))
		]
	];
	Pills->AddSlot().AutoWidth().Padding(FMargin(6, 0, 0, 0))
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(GetScopeLabel(Item->Scope))
		]
	];
	if (!Item->LevelName.IsEmpty())
	{
		Pills->AddSlot().AutoWidth().Padding(FMargin(6, 0, 0, 0))
		[
			SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(Item->LevelName).ColorAndOpacity(FSlateColor(FToolsetStyle::Accent))
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(0)
		.ToolTipText(FText::Format(LOCTEXT("WhyTooltip", "Why: {0}"), Item->WhyItMatters))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(4)
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Fill"))
					.BorderBackgroundColor(FSlateColor(SevColor))[ SNullWidget::NullWidget ]
				]
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(14, 12))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Pills ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 8, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(Item->Title)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(Item->Subject)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 7, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(FToolsetStyle::SeverityGood))
					.Text(FText::Format(LOCTEXT("FixFmt", "Fix: {0}"), Item->HowToFix))
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, bCanFix ? 8 : 12, 0))
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Ghost")
				.IsEnabled(CanNavigateTo(*Item))
				.OnClicked_Lambda([this, Item]()
				{
					if (Item->Scope == EFindingScope::System && Model.IsValid())
					{
						Model->RunScan();
					}
					else
					{
						NavigateTo(*Item);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(GetNavigationLabel(*Item))
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 12, 0))
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Primary")
				.Visibility(bCanFix ? EVisibility::Visible : EVisibility::Collapsed)
				.OnClicked_Lambda([this, Item]()
				{
					if (Model.IsValid())
					{
						Model->ApplyFix(Item);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
					.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent)).Text(FixLabel)
				]
			]
		];
}

bool SOptimizePanel::IsSettingVisible(const FPropertyAndParent& PropertyAndParent) const
{
	if (!Model.IsValid() || !Model->GetCategoryFilter().IsSet())
	{
		return false;
	}

	const FString PropertyCategory = PropertyAndParent.Property.GetMetaData(TEXT("Category"));
	switch (Model->GetCategoryFilter().GetValue())
	{
	case ECategory::Meshes:
		return PropertyCategory == TEXT("Meshes") || PropertyCategory == TEXT("Instancing");
	case ECategory::Materials:  return PropertyCategory == TEXT("Materials");
	case ECategory::Textures:   return PropertyCategory == TEXT("Textures");
	case ECategory::Lighting:   return PropertyCategory == TEXT("Lighting");
	case ECategory::Blueprints: return PropertyCategory == TEXT("Blueprints");
	default:                    return false;
	}
}

bool SOptimizePanel::HasSettingsForSelectedCategory() const
{
	if (!Model.IsValid() || !Model->GetCategoryFilter().IsSet())
	{
		return false;
	}
	const ECategory Category = Model->GetCategoryFilter().GetValue();
	return Category == ECategory::Meshes || Category == ECategory::Materials
		|| Category == ECategory::Textures || Category == ECategory::Lighting
		|| Category == ECategory::Blueprints;
}

FText SOptimizePanel::GetSettingsTitle() const
{
	if (!Model.IsValid() || !Model->GetCategoryFilter().IsSet())
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("SettingsTitle", "{0} settings"),
		FToolsetStyle::LabelForCategory(Model->GetCategoryFilter().GetValue()));
}

FText SOptimizePanel::GetNoSettingsText() const
{
	if (Model.IsValid() && Model->GetCategoryFilter().IsSet()
		&& Model->GetCategoryFilter().GetValue() == ECategory::Project)
	{
		return LOCTEXT("ProjectSettingsHelp", "These checks use Unreal project rendering settings rather than plugin thresholds.");
	}
	return LOCTEXT("NoCategorySettings", "This category has no adjustable analysis thresholds yet.");
}

void SOptimizePanel::OnSettingChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (UOptimizationToolsetSettings* Settings = GetMutableDefault<UOptimizationToolsetSettings>())
	{
		Settings->SaveConfig();
	}
	if (Model.IsValid())
	{
		Model->InvalidateScan();
	}
}

bool SOptimizePanel::CanNavigateTo(const FFinding& Finding)
{
	if (Finding.TargetAsset.IsValid() || Finding.TargetActor.IsValid()
		|| Finding.Scope == EFindingScope::Project || Finding.Scope == EFindingScope::System)
	{
		return true;
	}
	for (const TWeakObjectPtr<AActor>& Actor : Finding.RelatedActors)
	{
		if (Actor.IsValid())
		{
			return true;
		}
	}
	return false;
}

FText SOptimizePanel::GetScopeLabel(EFindingScope Scope)
{
	switch (Scope)
	{
	case EFindingScope::Asset:   return LOCTEXT("AssetScope", "Asset");
	case EFindingScope::Actor:   return LOCTEXT("ActorScope", "Actor");
	case EFindingScope::Level:   return LOCTEXT("LevelScope", "Level");
	case EFindingScope::Project: return LOCTEXT("ProjectScope", "Project");
	default:                     return LOCTEXT("SystemScope", "System");
	}
}

FText SOptimizePanel::GetNavigationLabel(const FFinding& Finding)
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

void SOptimizePanel::NavigateTo(const FFinding& Finding)
{
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

void SOptimizePanel::OpenProjectRenderingSettings(FName FindingTypeId)
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

	// Settings Editor clears its details filter while switching sections. Wait
	// until navigation settles, then find the search box *inside* SSettingsEditor
	// rather than relying on whichever nested widget happened to receive focus.
	// Retrying also covers the first time the Project Settings tab is spawned.
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
