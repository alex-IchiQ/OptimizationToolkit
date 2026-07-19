// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SOptimizeShell.h"
#include "Toolset/Slate/SAnalyzerView.h"
#include "Toolset/Slate/SCleanupView.h"
#include "Toolset/Slate/SDashboardView.h"
#include "Toolset/Slate/SOptimizeView.h"
#include "Toolset/Slate/SProfileView.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SOptimizeShell"

void SOptimizeShell::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)

		// Left navigation rail.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox).WidthOverride(160.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.Padding(FMargin(6, 8))
				[
					BuildNav()
				]
			]
		]

		// Switched content.
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(Switcher, SWidgetSwitcher)

			+ SWidgetSwitcher::Slot()[ SAssignNew(DashboardView, SDashboardView) ] // Dashboard
			+ SWidgetSwitcher::Slot()[ SAssignNew(OptimizeView, SOptimizeView) ]  // Optimize
			+ SWidgetSwitcher::Slot()[ SAssignNew(AnalyzerView, SAnalyzerView) ]  // Analyzer
			+ SWidgetSwitcher::Slot()[ SNew(SProfileView) ]                      // Profile
			+ SWidgetSwitcher::Slot()[ SAssignNew(CleanupView, SCleanupView) ]    // Cleanup
		]
	];

	SelectSection(Current);
}

TSharedRef<SWidget> SOptimizeShell::BuildNav()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4, 4, 4, 10))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Brand", "Optimization"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		// One Scan runs both the level analysis and the project sweep, so the pages
		// never carry their own scan buttons.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 10))
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(8, 6))
			.ToolTipText(LOCTEXT("ScanTip", "Analyze the level and sweep the project."))
			.OnClicked(this, &SOptimizeShell::OnScanClicked)
			[
				SNew(STextBlock).Text(LOCTEXT("Scan", "Scan"))
			]
		]

		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Dashboard, LOCTEXT("NavDashboard", "Dashboard")) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Optimize, LOCTEXT("NavOptimize", "Optimize")) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Analyzer, LOCTEXT("NavAnalyzer", "Analyzer")) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Profile, LOCTEXT("NavProfile", "Profile")) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Cleanup, LOCTEXT("NavCleanup", "Clean Up")) ];
}

TSharedRef<SWidget> SOptimizeShell::MakeNavButton(EOptimizeSection Section, const FText& Label)
{
	return SNew(SButton)
		.ButtonStyle(&FAppStyle::Get(), "SimpleButton")
		.HAlign(HAlign_Left)
		.ContentPadding(FMargin(8, 6))
		.OnClicked_Lambda([this, Section]()
		{
			SelectSection(Section);
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			// The active section reads at full strength; the rest are dimmed.
			.ColorAndOpacity_Lambda([this, Section]()
			{
				return IsSelected(Section) ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground();
			})
		];
}

void SOptimizeShell::SelectSection(EOptimizeSection Section)
{
	Current = Section;
	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(static_cast<int32>(Section));
	}
}

FReply SOptimizeShell::OnScanClicked()
{
	// Level analysis first (cheap), then the project-wide walks.
	if (OptimizeView.IsValid())
	{
		OptimizeView->Scan();
	}
	if (DashboardView.IsValid())
	{
		DashboardView->Scan();
	}
	if (AnalyzerView.IsValid())
	{
		AnalyzerView->Scan();
	}
	if (CleanupView.IsValid())
	{
		CleanupView->Scan();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
