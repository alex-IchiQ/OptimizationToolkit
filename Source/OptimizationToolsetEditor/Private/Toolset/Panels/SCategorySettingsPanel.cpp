// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SCategorySettingsPanel.h"

#include "Toolset/Navigation/FindingNavigator.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/Panels/SToolsetToggle.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCategorySettingsPanel"

using namespace ToolsetUI;

namespace
{
	UOptimizationToolsetSettings* Settings()
	{
		return GetMutableDefault<UOptimizationToolsetSettings>();
	}

	/** An int clamp/UI bound from property metadata, if it declared one. */
	TOptional<int32> MetaInt(const FProperty& Property, const TCHAR* Key)
	{
		if (Property.HasMetaData(Key))
		{
			return FCString::Atoi(*Property.GetMetaData(Key));
		}
		return TOptional<int32>();
	}

	/** The label the property asked for, falling back to its prettified name. */
	FText SettingLabel(const FProperty& Property)
	{
		if (Property.HasMetaData(TEXT("DisplayName")))
		{
			return FText::FromString(Property.GetMetaData(TEXT("DisplayName")));
		}
		return Property.GetDisplayNameText();
	}
}

void SCategorySettingsPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

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

			// The reflected threshold rows, rebuilt when the category changes.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				SAssignNew(SettingsBox, SVerticalBox)
			]

			// Shown instead when a category has no plugin thresholds — Project points
			// at the engine's own settings, the rest simply have nothing to tune yet.
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
	if (bCategoryChanged)
	{
		DisplayedCategory = CurrentCategory;
		RebuildRows();
	}
}

void SCategorySettingsPanel::RebuildRows()
{
	if (!SettingsBox.IsValid())
	{
		return;
	}

	SettingsBox->ClearChildren();
	if (!DisplayedCategory.IsSet())
	{
		return;
	}

	// Reflected order matches declaration order in the settings header, so the rows
	// read the way the class is written rather than alphabetically.
	for (TFieldIterator<FProperty> It(UOptimizationToolsetSettings::StaticClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (Property && PropertyMatchesCategory(*Property, DisplayedCategory.GetValue()))
		{
			SettingsBox->AddSlot().AutoHeight().Padding(FMargin(0, 3))
			[
				MakeSettingRow(*Property)
			];
		}
	}
}

bool SCategorySettingsPanel::PropertyMatchesCategory(const FProperty& Property, ECategory Category)
{
	const FString PropertyCategory = Property.GetMetaData(TEXT("Category"));
	switch (Category)
	{
	// Meshes owns instancing too: an ISM recommendation is a mesh decision.
	case ECategory::Meshes:     return PropertyCategory == TEXT("Meshes") || PropertyCategory == TEXT("Instancing");
	case ECategory::Materials:  return PropertyCategory == TEXT("Materials");
	case ECategory::Textures:   return PropertyCategory == TEXT("Textures");
	case ECategory::Lighting:   return PropertyCategory == TEXT("Lighting");
	case ECategory::Blueprints: return PropertyCategory == TEXT("Blueprints");
	default:                    return false;	// Collision, Project: no plugin thresholds
	}
}

TSharedRef<SWidget> SCategorySettingsPanel::MakeSettingRow(FProperty& Property)
{
	TSharedRef<SWidget> Editor = SNullWidget::NullWidget;
	if (FIntProperty* IntProperty = CastField<FIntProperty>(&Property))
	{
		Editor = MakeIntEditor(*IntProperty);
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
	{
		Editor = MakeBoolEditor(*BoolProperty);
	}

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card.Inner"))
		.Padding(FMargin(12, 8))
		.ToolTipText(Property.GetToolTipText())
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(0, 0, 12, 0))
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Body")
				.AutoWrapText(true)
				.Text(SettingLabel(Property))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Editor
			]
		];
}

TSharedRef<SWidget> SCategorySettingsPanel::MakeIntEditor(FIntProperty& Property)
{
	// Hard bounds clamp typed input; UI bounds set the drag range. Falling the UI
	// range back to the hard range keeps the slider sane when only one is declared.
	const TOptional<int32> ClampMin = MetaInt(Property, TEXT("ClampMin"));
	const TOptional<int32> ClampMax = MetaInt(Property, TEXT("ClampMax"));
	const TOptional<int32> UIMin = MetaInt(Property, TEXT("UIMin"));
	const TOptional<int32> UIMax = MetaInt(Property, TEXT("UIMax"));

	FIntProperty* PropertyPtr = &Property;

	return SNew(SBox).WidthOverride(132.0f)
	[
		SNew(SSpinBox<int32>)
		.Style(&S(), "Toolset.SpinBox")
		.MinValue(ClampMin)
		.MaxValue(ClampMax)
		.MinSliderValue(UIMin.IsSet() ? UIMin : ClampMin)
		.MaxSliderValue(UIMax.IsSet() ? UIMax : ClampMax)
		.Value_Lambda([PropertyPtr]()
		{
			return PropertyPtr->GetPropertyValue_InContainer(Settings());
		})
		// Live-update the value on drag so the number tracks the handle, but only
		// persist and re-scan once the edit settles — a SaveConfig per drag frame
		// would thrash the config file and invalidate the scan dozens of times.
		.OnValueChanged_Lambda([PropertyPtr](int32 NewValue)
		{
			PropertyPtr->SetPropertyValue_InContainer(Settings(), NewValue);
		})
		.OnValueCommitted_Lambda([this, PropertyPtr](int32 NewValue, ETextCommit::Type)
		{
			PropertyPtr->SetPropertyValue_InContainer(Settings(), NewValue);
			CommitChange();
		})
	];
}

TSharedRef<SWidget> SCategorySettingsPanel::MakeBoolEditor(FBoolProperty& Property)
{
	FBoolProperty* PropertyPtr = &Property;

	return SNew(SToolsetToggle)
		.IsChecked_Lambda([PropertyPtr]()
		{
			return PropertyPtr->GetPropertyValue_InContainer(Settings());
		})
		.OnToggled_Lambda([this, PropertyPtr](bool bNewValue)
		{
			PropertyPtr->SetPropertyValue_InContainer(Settings(), bNewValue);
			CommitChange();
		});
}

void SCategorySettingsPanel::CommitChange()
{
	if (UOptimizationToolsetSettings* SettingsObject = Settings())
	{
		SettingsObject->SaveConfig();
	}
	if (Model.IsValid())
	{
		Model->InvalidateScan();
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

#undef LOCTEXT_NAMESPACE
