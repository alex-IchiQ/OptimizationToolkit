// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SDashboardView.h"
#include "Toolset/ToolsetStyle.h"	// ColorForAssetCategory (a colour helper, not a style)

#include "Misc/ScopedSlowTask.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDashboardView"

void SDashboardView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(12, 10))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(14, 12))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "Project size"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 4, 0, 0))
				[
					SNew(STextBlock)
					.Text(this, &SDashboardView::GetSummaryText)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]

				// Stacked bar + legend, rebuilt on each scan.
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 14, 0, 0))
				[
					SAssignNew(BreakdownBox, SVerticalBox)
				]
			]
		]
	];
}

void SDashboardView::Scan()
{
	FScopedSlowTask SlowTask(0.0f, LOCTEXT("Measuring", "Measuring project size..."));
	SlowTask.MakeDialog();

	SizeReport = FProjectSizeReport::Compute();
	bHasReport = true;
	RebuildBreakdown();
}

void SDashboardView::RebuildBreakdown()
{
	if (!BreakdownBox.IsValid())
	{
		return;
	}

	BreakdownBox->ClearChildren();
	if (SizeReport.TotalBytes <= 0)
	{
		return;
	}

	// One stacked bar plus a legend, so the whole footprint is on screen with
	// nothing truncated. SColorBlock fills each slot with a solid colour — no brush
	// needed, which keeps this on the default editor styling.
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	TSharedRef<SWrapBox> Legend = SNew(SWrapBox).UseAllottedSize(true);

	for (const FProjectSizeEntry& Entry : SizeReport.Entries)
	{
		const FLinearColor Color = FToolsetStyle::ColorForAssetCategory(Entry.Category);
		const float Fraction = static_cast<float>(
			static_cast<double>(Entry.TotalBytes) / static_cast<double>(SizeReport.TotalBytes));

		Bar->AddSlot().FillWidth(FMath::Max(Fraction, KINDA_SMALL_NUMBER))
		[
			SNew(SColorBlock).Color(Color)
		];

		Legend->AddSlot().Padding(FMargin(0, 0, 18, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 7, 0))
			[
				SNew(SBox).WidthOverride(9.0f).HeightOverride(9.0f)
				[
					SNew(SColorBlock).Color(Color)
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FProjectSizeReport::LabelForCategory(Entry.Category))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("LegendValue", "{0} · {1}%"),
					FText::AsMemory(Entry.TotalBytes),
					FText::AsNumber(FMath::RoundToInt(Fraction * 100.0f))))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
	}

	BreakdownBox->AddSlot().AutoHeight()
	[
		SNew(SBox).HeightOverride(16.0f)[ Bar ]
	];
	BreakdownBox->AddSlot().AutoHeight().Padding(FMargin(0, 14, 0, 0))
	[
		Legend
	];
}

FText SDashboardView::GetSummaryText() const
{
	if (!bHasReport)
	{
		return LOCTEXT("NoScan", "Press Scan to measure every package under /Game.");
	}
	if (SizeReport.bRegistryIncomplete)
	{
		return LOCTEXT("Incomplete", "The asset registry is still scanning — the numbers would be short. Scan again once it finishes.");
	}
	return FText::Format(LOCTEXT("SummaryFmt", "{0} across {1} packages"),
		FText::AsMemory(SizeReport.TotalBytes), FText::AsNumber(SizeReport.PackageCount));
}

#undef LOCTEXT_NAMESPACE
