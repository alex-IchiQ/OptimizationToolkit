// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SPlaceholderPanel.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPlaceholderPanel"

using namespace ToolsetUI;

void SPlaceholderPanel::Construct(const FArguments& InArgs)
{
	TSharedRef<SVerticalBox> ActionList = SNew(SVerticalBox);
	for (const FText& Action : InArgs._PlannedActions)
	{
		ActionList->AddSlot().AutoHeight().Padding(FMargin(0, 4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 10, 0))
			[
				SNew(SBox).WidthOverride(6).HeightOverride(6)
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Fill.Rounded")).BorderBackgroundColor(FSlateColor(FToolsetStyle::Accent))
					[
						SNullWidget::NullWidget
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true).Text(Action)
			]
		];
	}

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(22, 20))
		[
			SNew(SBorder)
			.BorderImage(Brush("Toolset.Card"))
			.Padding(FMargin(22, 20))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Title").Text(InArgs._Title)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 16))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true).Text(InArgs._Body)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 3))
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(LOCTEXT("PlannedTag", "PLANNED ACTIONS"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))
				[
					ActionList
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
