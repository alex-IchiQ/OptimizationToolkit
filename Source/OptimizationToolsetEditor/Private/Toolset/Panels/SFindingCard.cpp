// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SFindingCard.h"

#include "Toolset/Navigation/FindingNavigator.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SFindingCard"

using namespace ToolsetUI;

void SFindingCard::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	Finding = InArgs._Finding;

	if (!Finding.IsValid())
	{
		ChildSlot[SNullWidget::NullWidget];
		return;
	}

	const FLinearColor SeverityColor = FToolsetStyle::ColorForSeverity(Finding->Severity);
	const bool bCanFix = FToolsetModel::HasSupportedFix(*Finding);
	IOptimizationFix* Fix = bCanFix ? FToolsetRegistry::Get().FindFix(Finding->FixId) : nullptr;
	const FText FixLabel = Fix ? Fix->GetLabel() : LOCTEXT("FixGeneric", "Apply");

	TSharedRef<SHorizontalBox> Pills = SNew(SHorizontalBox);
	Pills->AddSlot().AutoWidth().Padding(FMargin(0, 0, 6, 0))
	[
		SNew(SBorder)
		.BorderImage(Brush("Toolset.Fill.Pill"))
		.BorderBackgroundColor(FSlateColor(FLinearColor(SeverityColor.R, SeverityColor.G, SeverityColor.B, 0.18f)))
		.Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(FToolsetStyle::LabelForSeverity(Finding->Severity)).ColorAndOpacity(FSlateColor(SeverityColor))
		]
	];
	Pills->AddSlot().AutoWidth()
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(FToolsetStyle::LabelForCategory(Finding->Category))
		]
	];
	Pills->AddSlot().AutoWidth().Padding(FMargin(6, 0, 0, 0))
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(GetScopeLabel(Finding->Scope))
		]
	];
	if (!Finding->LevelName.IsEmpty())
	{
		Pills->AddSlot().AutoWidth().Padding(FMargin(6, 0, 0, 0))
		[
			SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(Finding->LevelName).ColorAndOpacity(FSlateColor(FToolsetStyle::Accent))
			]
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(0)
		.ToolTipText(FText::Format(LOCTEXT("WhyTooltip", "Why: {0}"), Finding->WhyItMatters))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(4)
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Fill"))
					.BorderBackgroundColor(FSlateColor(SeverityColor))[SNullWidget::NullWidget]
				]
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(14, 12))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Pills]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 8, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(Finding->Title)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(Finding->Subject)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 7, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(FToolsetStyle::SeverityGood))
					.Text(FText::Format(LOCTEXT("FixFmt", "Fix: {0}"), Finding->HowToFix))
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, bCanFix ? 8 : 12, 0))
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Ghost")
				.IsEnabled(FFindingNavigator::CanNavigate(*Finding))
				.OnClicked_Lambda([this]()
				{
					FFindingNavigator::Navigate(*Finding, Model);
					return FReply::Handled();
				})
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
					.Text(FFindingNavigator::GetActionLabel(*Finding))
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 12, 0))
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Primary")
				.Visibility(bCanFix ? EVisibility::Visible : EVisibility::Collapsed)
				.OnClicked_Lambda([this]()
				{
					if (Model.IsValid())
					{
						Model->ApplyFix(Finding);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
					.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent)).Text(FixLabel)
				]
			]
		]
	];
}

FText SFindingCard::GetScopeLabel(EFindingScope Scope)
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

#undef LOCTEXT_NAMESPACE
