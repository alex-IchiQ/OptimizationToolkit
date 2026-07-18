// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SCategorySettingsPanel.h"

#include "Toolset/Navigation/FindingNavigator.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCategorySettingsPanel"

using namespace ToolsetUI;

void SCategorySettingsPanel::Construct(const FArguments& InArgs)
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
	SettingsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SCategorySettingsPanel::IsSettingVisible));
	SettingsView->OnFinishedChangingProperties().AddSP(this, &SCategorySettingsPanel::OnSettingChanged);
	SettingsView->SetObject(GetMutableDefault<UOptimizationToolsetSettings>());

	ChildSlot
	[
		SNew(SBorder)
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
				.Text(this, &SCategorySettingsPanel::GetSettingsTitle)
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
					.Text(this, &SCategorySettingsPanel::GetNoSettingsText)
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
						FFindingNavigator::OpenProjectRenderingSettings();
						return FReply::Handled();
					})
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
						.Text(LOCTEXT("ProjectSettingsButton", "Open Rendering Settings"))
					]
				]
			]
		]
	];

	if (Model.IsValid())
	{
		ChangedHandle = Model->OnChanged().AddSP(this, &SCategorySettingsPanel::Refresh);
		Refresh();
	}
}

SCategorySettingsPanel::~SCategorySettingsPanel()
{
	if (Model.IsValid())
	{
		Model->OnChanged().Remove(ChangedHandle);
	}
}

void SCategorySettingsPanel::Refresh()
{
	const TOptional<ECategory> CurrentCategory = Model.IsValid()
		? Model->GetCategoryFilter() : TOptional<ECategory>();
	const bool bCategoryChanged = CurrentCategory.IsSet() != DisplayedCategory.IsSet()
		|| (CurrentCategory.IsSet() && CurrentCategory.GetValue() != DisplayedCategory.GetValue());
	if (SettingsView.IsValid() && bCategoryChanged)
	{
		DisplayedCategory = CurrentCategory;
		SettingsView->ForceRefresh();
	}
}

bool SCategorySettingsPanel::IsSettingVisible(const FPropertyAndParent& PropertyAndParent) const
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

bool SCategorySettingsPanel::HasSettingsForSelectedCategory() const
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

FText SCategorySettingsPanel::GetSettingsTitle() const
{
	if (!Model.IsValid() || !Model->GetCategoryFilter().IsSet())
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("SettingsTitle", "{0} settings"),
		FToolsetStyle::LabelForCategory(Model->GetCategoryFilter().GetValue()));
}

FText SCategorySettingsPanel::GetNoSettingsText() const
{
	if (Model.IsValid() && Model->GetCategoryFilter().IsSet()
		&& Model->GetCategoryFilter().GetValue() == ECategory::Project)
	{
		return LOCTEXT("ProjectSettingsHelp", "These checks use Unreal project rendering settings rather than plugin thresholds.");
	}
	return LOCTEXT("NoCategorySettings", "This category has no adjustable analysis thresholds yet.");
}

void SCategorySettingsPanel::OnSettingChanged(const FPropertyChangedEvent& PropertyChangedEvent)
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

#undef LOCTEXT_NAMESPACE
