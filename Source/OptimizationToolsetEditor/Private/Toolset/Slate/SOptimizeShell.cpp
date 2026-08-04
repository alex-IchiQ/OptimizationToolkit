// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Slate/SOptimizeShell.h"
#include "Toolset/Slate/SAnalyzerView.h"
#include "Toolset/Slate/SCleanupView.h"
#include "Toolset/Slate/SDashboardView.h"
#include "Toolset/Slate/SOptimizeView.h"
#include "Toolset/Slate/SProfileView.h"
#include "Toolset/Slate/OptimizeStyle.h"
#include "Toolset/ToolsetModel.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SOptimizeShell"

void SOptimizeShell::Construct(const FArguments& InArgs)
{
	Model = MakeShared<FToolsetModel>();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FOptimizeStyle::Brush("Opt.Window"))
		.Padding(0)
		[
			SNew(SHorizontalBox)

			// Left navigation rail.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(168.0f)
				[
					SNew(SBorder)
					.BorderImage(FOptimizeStyle::Brush("Opt.Panel"))
					.Padding(FMargin(8, 10))
					[
						BuildNav()
					]
				]
			]

			// Switched content on the dark backdrop.
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(Switcher, SWidgetSwitcher)

				+ SWidgetSwitcher::Slot()[ SAssignNew(DashboardView, SDashboardView).Model(Model) ] // Dashboard
				+ SWidgetSwitcher::Slot()[ SAssignNew(OptimizeView, SOptimizeView).Model(Model) ]  // Optimize
				+ SWidgetSwitcher::Slot()[ SAssignNew(AnalyzerView, SAnalyzerView) ]  // Analyzer
				+ SWidgetSwitcher::Slot()[ SNew(SProfileView) ]                      // Profile
				+ SWidgetSwitcher::Slot()[ SAssignNew(CleanupView, SCleanupView) ]    // Cleanup
			]
		]
	];

	SelectSection(Current);
}

TSharedRef<SWidget> SOptimizeShell::BuildNav()
{
	return SNew(SVerticalBox)

		// One Scan runs both the level analysis and the project sweep, so the pages
		// never carry their own scan buttons.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 12))
		[
			SNew(SButton)
			.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Button.Primary")
			.HAlign(HAlign_Fill)
			.ContentPadding(FMargin(8, 10))
			.ToolTipText(LOCTEXT("ScanTip", "Analyze the level and sweep the project."))
			.OnClicked(this, &SOptimizeShell::OnScanClicked)
			[
				// Icon stacked over the label, like Palatial's Analyze button.
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0, 0, 0, 4))
				[
					SNew(SBox).WidthOverride(22.0f).HeightOverride(22.0f)
					[
						SNew(SImage)
						.Image(FOptimizeStyle::Brush("Opt.Icon.Scan"))
						.ColorAndOpacity(FSlateColor(FOptimizeStyle::OnAccent))
					]
				]

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.NavLabel")
					.ColorAndOpacity(FSlateColor(FOptimizeStyle::OnAccent))
					.Text(LOCTEXT("Scan", "Scan"))
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Dashboard, LOCTEXT("NavDashboard", "Dashboard"), "Opt.Icon.Dashboard") ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Optimize, LOCTEXT("NavOptimize", "Optimize"), "Opt.Icon.Optimize") ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Analyzer, LOCTEXT("NavAnalyzer", "Analyzer"), "Opt.Icon.Analyze") ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Profile, LOCTEXT("NavProfile", "Profile"), "Opt.Icon.Profile") ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Cleanup, LOCTEXT("NavCleanup", "Clean Up"), "Opt.Icon.Cleanup") ]

		// Mascot fills the space between the nav and the footer, scaled to fit
		// (never upscaled past its natural size, never overlapping the nav).
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(0, 12, 0, 6))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			[
				SNew(SImage).Image(FOptimizeStyle::Brush("Opt.Mascot"))
			]
		]

		// Footer version pinned to the very bottom.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8, 0, 8, 4))
		[
			SNew(STextBlock)
			.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
			.Text(LOCTEXT("Version", "v1.0.0"))
		];
}

TSharedRef<SWidget> SOptimizeShell::MakeNavButton(EOptimizeSection Section, const FText& Label, const FName& IconName)
{
	// Active/inactive tint, shared by the icon and the label so the whole row reads
	// as one state.
	auto TintFor = [this, Section]()
	{
		return IsSelected(Section) ? FSlateColor(FOptimizeStyle::Accent) : FSlateColor(FOptimizeStyle::TextDim);
	};

	FText Tooltip;
	switch (Section)
	{
	case EOptimizeSection::Dashboard: Tooltip = LOCTEXT("TipDashboard", "Level scale, found problems, project size and scan scope at a glance."); break;
	case EOptimizeSection::Optimize:  Tooltip = LOCTEXT("TipOptimize", "Findings grouped by level and problem type, with one-click batch fixes."); break;
	case EOptimizeSection::Analyzer:  Tooltip = LOCTEXT("TipAnalyzer", "What the loaded content costs in GPU memory: textures, render targets, meshes."); break;
	case EOptimizeSection::Profile:   Tooltip = LOCTEXT("TipProfile", "Stat overlays and GPU visualizers for the active viewport."); break;
	default:                          Tooltip = LOCTEXT("TipCleanup", "Project-wide possible duplicates and unused assets, plus redirector/save actions."); break;
	}

	return SNew(SButton)
		.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Nav.Button")
		.HAlign(HAlign_Fill)
		.ContentPadding(FMargin(0))
		.ToolTipText(Tooltip)
		.OnClicked_Lambda([this, Section]()
		{
			SelectSection(Section);
			return FReply::Handled();
		})
		[
			// A teal wash marks the active section; icon + label go accent when active,
			// dim otherwise. The border owns the row padding so the wash spans it.
			SNew(SBorder)
			.BorderImage_Lambda([this, Section]()
			{
				return IsSelected(Section)
					? FOptimizeStyle::Brush("Opt.Nav.Selected")
					: FStyleDefaults::GetNoBrush();
			})
			.Padding(FMargin(9, 7))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 9, 0))
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
					[
						SNew(SImage)
						.Image(FOptimizeStyle::Brush(IconName))
						.ColorAndOpacity_Lambda(TintFor)
					]
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.NavLabel")
					.Text(Label)
					.ColorAndOpacity_Lambda(TintFor)
				]
			]
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
	// Level analysis first (cheap): one RunScan feeds both Optimize's findings tree
	// and the Dashboard's severity counts through the shared model's OnChanged.
	if (Model.IsValid())
	{
		Model->RunScan();
	}

	// Then the project-wide walks each view owns: the Dashboard's disk + memory
	// reports, the Analyzer's memory tables, the Cleanup sweep.
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
