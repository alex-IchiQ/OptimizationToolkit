// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/SToolsetWindow.h"
#include "Toolset/ToolsetStyle.h"
#include "Toolset/ToolsetCompat.h"
#include "Toolset/ToolsetRegistry.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SToolsetWindow"

namespace
{
	// Convenience accessors so widget code stays terse.
	const ISlateStyle& S() { return FToolsetStyle::Get(); }
	const FSlateBrush* Brush(const FName& Name) { return FToolsetStyle::Get().GetBrush(Name); }
}

void SToolsetWindow::Construct(const FArguments& InArgs)
{
	// Islands layout: a flat backdrop with rounded panels floating on it,
	// separated by consistent gaps (outer padding + inter-slot margins).
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(Brush("Toolset.Surface.Base"))
		.Padding(FMargin(7))
		[
			SNew(SHorizontalBox)

			// Left navigation rail (island).
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0, 0, 7, 0))
			[
				BuildSidebar()
			]

			// Right side: header island + content island, stacked with a gap.
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 7))
				[
					BuildHeader()
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					BuildContent()
				]
			]
		]
	];

	SelectSection(EToolsetSection::Dashboard);
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildSidebar()
{
	return SNew(SBox)
		.WidthOverride(216.0f)
		[
			SNew(SBorder)
			.BorderImage(Brush("Toolset.Island"))
			.Padding(FMargin(12, 18))
			[
				SNew(SVerticalBox)

				// Brand block.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(8, 0, 8, 4))
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Heading")
					.Text(LOCTEXT("BrandTitle", "Optimization"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(8, 0, 8, 18))
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
					.Text(LOCTEXT("BrandSub", "PROFILING TOOLSET"))
				]

				// Nav items (icon + label).
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Dashboard, LOCTEXT("NavDashboard", "Dashboard"), "Toolset.Icon.Dashboard") ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Analyze,   LOCTEXT("NavAnalyze",   "Analyze"),   "Toolset.Icon.Analyze") ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Optimize,  LOCTEXT("NavOptimize",  "Optimize"),  "Toolset.Icon.Optimize") ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Profile,   LOCTEXT("NavProfile",   "Profile"),   "Toolset.Icon.Profile") ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Cleanup,   LOCTEXT("NavCleanup",   "Cleanup"),   "Toolset.Icon.Cleanup") ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Reports,   LOCTEXT("NavReports",   "Reports"),   "Toolset.Icon.Reports") ]

				// Mascot fills the space between the nav and the footer, scaled to
				// fit (never upscaled past its natural size, never overlapping nav).
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
						SNew(SImage).Image(Brush("Toolset.Mascot"))
					]
				]

				// Footer version pinned to the very bottom.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(8, 0, 8, 0))
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
					.Text(LOCTEXT("Version", "v1.0.0"))
				]
			]
		];
}

TSharedRef<SWidget> SToolsetWindow::BuildNavItem(EToolsetSection Section, const FText& Label, const FName& IconName)
{
	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Nav.Button")
		.ContentPadding(FMargin(0))
		.OnClicked_Lambda([this, Section]() { SelectSection(Section); return FReply::Handled(); })
		[
			SNew(SBorder)
			.Padding(FMargin(10, 8))
			// Selected: translucent teal fill; unselected: transparent.
			.BorderImage(Brush("Toolset.Nav.Selected"))
			.BorderBackgroundColor_Lambda([this, Section]()
			{
				return IsSectionSelected(Section) ? FLinearColor::White : FLinearColor(1, 1, 1, 0.0f);
			})
			[
				SNew(SHorizontalBox)

				// Icon (white SVG tinted by state).
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2, 0, 12, 0))
				[
					SNew(SBox).WidthOverride(18).HeightOverride(18)
					[
						SNew(SImage)
						.Image(Brush(IconName))
						.ColorAndOpacity_Lambda([this, Section]()
						{
							return IsSectionSelected(Section)
								? FSlateColor(FToolsetStyle::Accent)
								: FSlateColor(FToolsetStyle::TextSecondary);
						})
					]
				]

				// Label.
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.NavLabel")
					.Text(Label)
					.ColorAndOpacity_Lambda([this, Section]()
					{
						return IsSectionSelected(Section)
							? FSlateColor(FToolsetStyle::TextPrimary)
							: FSlateColor(FToolsetStyle::TextSecondary);
					})
				]
			]
		];
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildHeader()
{
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Island"))
		.Padding(FMargin(22, 16))
		[
			SNew(SHorizontalBox)

			// Section title + live summary.
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Title").Text(this, &SToolsetWindow::GetHeaderTitle)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(this, &SToolsetWindow::GetSummaryText)
				]
			]

			// Health score chip.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 0, 14, 0))
			[
				SNew(SBox).WidthOverride(150)
				[
					SNew(SBorder)
					.BorderImage(Brush("Toolset.Card"))
					.Padding(FMargin(12, 8))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(LOCTEXT("HealthLabel", "HEALTH"))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
								.Text(this, &SToolsetWindow::GetScoreText)
								.ColorAndOpacity(this, &SToolsetWindow::GetScoreColor)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
						[
							SNew(SBox).HeightOverride(6)
							[
								SNew(SProgressBar)
								.Percent(this, &SToolsetWindow::GetScorePercent)
								.FillColorAndOpacity(this, &SToolsetWindow::GetScoreColor)
							]
						]
					]
				]
			]

			// Scan button (primary action).
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Primary")
				.OnClicked(this, &SToolsetWindow::OnScanClicked)
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.NavLabel")
					.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0x16, 0x17, 0x19))))
					.Text(LOCTEXT("ScanBtn", "Scan Level"))
				]
			]
		];
}

// ---------------------------------------------------------------------------
// Content switcher
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildContent()
{
	TArray<FText> ReportActions = {
		LOCTEXT("RepAct1", "Export findings to CSV / JSON"),
		LOCTEXT("RepAct2", "Before / after snapshots to prove wins"),
		LOCTEXT("RepAct3", "Per-category summary for milestone reviews"),
	};

	ContentSwitcher = SNew(SWidgetSwitcher);
	ContentSwitcher->AddSlot()[ BuildDashboardPanel() ];   // Dashboard
	ContentSwitcher->AddSlot()[ BuildAnalyzePanel() ];     // Analyze
	ContentSwitcher->AddSlot()[ BuildOptimizePanel() ];    // Optimize
	ContentSwitcher->AddSlot()[ BuildProfilePanel() ];     // Profile
	ContentSwitcher->AddSlot()[ BuildCleanupPanel() ];    // Cleanup
	ContentSwitcher->AddSlot()[ BuildPlaceholderPanel(     // Reports
		LOCTEXT("ReportsTitle", "Reports & exports"),
		LOCTEXT("ReportsBody", "Turn a scan into a shareable report."),
		ReportActions) ];

	// Wrap the switched content in its own island so it floats like the rest.
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Island"))
		.Padding(FMargin(4))
		[
			ContentSwitcher.ToSharedRef()
		];
}

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildDashboardPanel()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(FMargin(22, 20))
		[
			SNew(SVerticalBox)

			// Severity summary row.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(6))
				+ SUniformGridPanel::Slot(0, 0)[ MakeSeverityStatCard(ESeverity::Critical) ]
				+ SUniformGridPanel::Slot(1, 0)[ MakeSeverityStatCard(ESeverity::Major) ]
				+ SUniformGridPanel::Slot(2, 0)[ MakeSeverityStatCard(ESeverity::Minor) ]
			]

			// Hint card.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(6, 14, 6, 0))
			[
				SNew(SBorder)
				.BorderImage(Brush("Toolset.Card"))
				.Padding(FMargin(18, 16))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(LOCTEXT("DashHowTitle", "How it works"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 8, 0, 0))
					[
						SNew(STextBlock)
						.TextStyle(&S(), "Toolset.Text.Body")
						.AutoWrapText(true)
						.Text(LOCTEXT("DashHowBody",
							"1.  Press Scan Level to analyze the active level.\n"
							"2.  Review findings by severity and category in Analyze.\n"
							"3.  Apply safe, Undo-able fixes in Optimize.\n"
							"4.  Watch performance live in Profile, then export a report."))
					]
				]
			]
		];
}

TSharedRef<SWidget> SToolsetWindow::MakeSeverityStatCard(ESeverity Severity)
{
	const FLinearColor Color = FToolsetStyle::ColorForSeverity(Severity);

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(18, 16))
		[
			SNew(SVerticalBox)

			// Colored severity label with a leading dot.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 8, 0))
				[
					SNew(SBox).WidthOverride(9).HeightOverride(9)
					[
						SNew(SBorder).BorderImage(Brush("Toolset.Pill")).BorderBackgroundColor(FSlateColor(Color))[ SNullWidget::NullWidget ]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
					.Text(FToolsetStyle::LabelForSeverity(Severity))
					.ColorAndOpacity(FSlateColor(Color))
				]
			]

			// Big count.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Metric")
				.Text_Lambda([this, Severity]()
				{
					return bHasScanned ? FText::AsNumber(LastScan.CountBySeverity(Severity)) : FText::FromString(TEXT("—"));
				})
			]
		];
}

// ---------------------------------------------------------------------------
// Analyze
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildAnalyzePanel()
{
	return SNew(SVerticalBox)

		// Filter toolbar.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(22, 16, 22, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Filter findings…"))
				.OnTextChanged(this, &SToolsetWindow::OnSearchChanged)
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10, 0, 0, 0)).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))
				[
					SNew(SButton).ButtonStyle(&S(), "Toolset.Button.Ghost")
					.OnClicked_Lambda([this]() { return OnToggleSeverityFilter(ESeverity::Critical); })
					[ SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(LOCTEXT("FCritical", "Critical"))
						.ColorAndOpacity_Lambda([this]() { return EnabledSeverities.Contains(ESeverity::Critical) ? FSlateColor(FToolsetStyle::SeverityCritical) : FSlateColor(FToolsetStyle::TextSecondary); }) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))
				[
					SNew(SButton).ButtonStyle(&S(), "Toolset.Button.Ghost")
					.OnClicked_Lambda([this]() { return OnToggleSeverityFilter(ESeverity::Major); })
					[ SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(LOCTEXT("FMajor", "Major"))
						.ColorAndOpacity_Lambda([this]() { return EnabledSeverities.Contains(ESeverity::Major) ? FSlateColor(FToolsetStyle::SeverityMajor) : FSlateColor(FToolsetStyle::TextSecondary); }) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))
				[
					SNew(SButton).ButtonStyle(&S(), "Toolset.Button.Ghost")
					.OnClicked_Lambda([this]() { return OnToggleSeverityFilter(ESeverity::Minor); })
					[ SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(LOCTEXT("FMinor", "Minor"))
						.ColorAndOpacity_Lambda([this]() { return EnabledSeverities.Contains(ESeverity::Minor) ? FSlateColor(FToolsetStyle::SeverityMinor) : FSlateColor(FToolsetStyle::TextSecondary); }) ]
				]
			]
		]

		// Findings list.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(16, 6, 16, 16))
		[
			SAssignNew(FindingsListView, SListView<TSharedPtr<FFinding>>)
			.ListItemsSource(&VisibleFindings)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SToolsetWindow::OnGenerateFindingRow)
		];
}

TSharedRef<ITableRow> SToolsetWindow::OnGenerateFindingRow(TSharedPtr<FFinding> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FLinearColor SevColor = FToolsetStyle::ColorForSeverity(Item->Severity);

	TSharedRef<SHorizontalBox> Pills = SNew(SHorizontalBox);

	// Severity pill.
	Pills->AddSlot().AutoWidth().Padding(FMargin(0, 0, 6, 0))
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Pill")).BorderBackgroundColor(FSlateColor(FLinearColor(SevColor.R, SevColor.G, SevColor.B, 0.18f)))
		.Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
			.Text(FToolsetStyle::LabelForSeverity(Item->Severity)).ColorAndOpacity(FSlateColor(SevColor))
		]
	];
	// Category pill.
	Pills->AddSlot().AutoWidth()
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(FToolsetStyle::LabelForCategory(Item->Category))
		]
	];

	return SNew(STableRow<TSharedPtr<FFinding>>, OwnerTable)
		.Padding(FMargin(6, 5))
		[
			SNew(SBorder)
			.BorderImage(Brush("Toolset.Card"))
			.Padding(0)
			[
				SNew(SHorizontalBox)

				// Severity colour stripe.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(4)
					[
						SNew(SBorder).BorderImage(Brush("Toolset.Card.Inner")).BorderBackgroundColor(FSlateColor(SevColor))[ SNullWidget::NullWidget ]
					]
				]

				// Body.
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
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 8, 0, 0))
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true)
						.Text(FText::Format(LOCTEXT("WhyFmt", "Why:  {0}"), Item->WhyItMatters))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3, 0, 0))
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true)
						.ColorAndOpacity(FSlateColor(FToolsetStyle::SeverityGood))
						.Text(FText::Format(LOCTEXT("FixFmt", "Fix:  {0}"), Item->HowToFix))
					]
				]

				// Focus button.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 12, 0))
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Ghost")
					.Visibility(Item->TargetActor.IsValid() ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked_Lambda([Item]()
					{
						FLevelAnalyzer::FocusActor(Item->TargetActor);
						return FReply::Handled();
					})
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(LOCTEXT("FocusBtn", "Focus"))
					]
				]
			]
		];
}

// ---------------------------------------------------------------------------
// Optimize (registry-driven safe fixes)
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildOptimizePanel()
{
	return SNew(SVerticalBox)

		// Header: summary + "Apply all".
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(22, 16, 22, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(LOCTEXT("OptHeading", "Safe auto-fixes"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 0))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").AutoWrapText(true)
					.Text_Lambda([this]()
					{
						if (!bHasScanned)
						{
							return LOCTEXT("OptNoScan", "Run a scan to find fixable issues.");
						}
						if (FixableFindings.Num() == 0)
						{
							return LOCTEXT("OptNone", "No auto-fixable issues found.");
						}
						return FText::Format(
							LOCTEXT("OptCount", "{0} issues can be fixed automatically — each fix is transactional and Undo-able."),
							FText::AsNumber(FixableFindings.Num()));
					})
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Primary")
				.Visibility_Lambda([this]() { return FixableFindings.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnClicked(this, &SToolsetWindow::OnApplyAllFixes)
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
					.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0x16, 0x17, 0x19))))
					.Text(LOCTEXT("ApplyAll", "Apply all"))
				]
			]
		]

		// Fixable findings.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(16, 6, 16, 16))
		[
			SAssignNew(FixListView, SListView<TSharedPtr<FFinding>>)
			.ListItemsSource(&FixableFindings)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SToolsetWindow::OnGenerateFixRow)
		];
}

TSharedRef<ITableRow> SToolsetWindow::OnGenerateFixRow(TSharedPtr<FFinding> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FLinearColor SevColor = FToolsetStyle::ColorForSeverity(Item->Severity);
	IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Item->FixId);
	const FText FixLabel = Fix ? Fix->GetLabel() : LOCTEXT("FixGeneric", "Fix");

	return SNew(STableRow<TSharedPtr<FFinding>>, OwnerTable)
		.Padding(FMargin(6, 5))
		[
			SNew(SBorder)
			.BorderImage(Brush("Toolset.Card"))
			.Padding(0)
			[
				SNew(SHorizontalBox)

				// Severity stripe.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(4)
					[
						SNew(SBorder).BorderImage(Brush("Toolset.Card.Inner")).BorderBackgroundColor(FSlateColor(SevColor))[ SNullWidget::NullWidget ]
					]
				]

				// Body.
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(14, 12))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(Item->Title)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 0))
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").Text(Item->Subject)
					]
				]

				// Focus.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 8, 0))
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Ghost")
					.Visibility(Item->TargetActor.IsValid() ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked_Lambda([Item]() { FLevelAnalyzer::FocusActor(Item->TargetActor); return FReply::Handled(); })
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(LOCTEXT("FocusBtn2", "Focus"))
					]
				]

				// Apply fix.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 12, 0))
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Primary")
					.OnClicked_Lambda([this, Item]() { ApplyFix(Item); return FReply::Handled(); })
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
						.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0x16, 0x17, 0x19))))
						.Text(FixLabel)
					]
				]
			]
		];
}

// ---------------------------------------------------------------------------
// Cleanup (registry-driven project-wide actions)
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildCleanupPanel()
{
	TSharedRef<SVerticalBox> Cards = SNew(SVerticalBox);
	for (const TUniquePtr<ICleanupAction>& Action : FToolsetRegistry::Get().GetActions())
	{
		if (Action && Action->IsSupported())
		{
			Cards->AddSlot().AutoHeight().Padding(FMargin(0, 0, 0, 10))
			[
				MakeCleanupActionCard(*Action)
			];
		}
	}

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(22, 20))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(LOCTEXT("CleanHeading", "Project hygiene"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 4, 0, 16))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").AutoWrapText(true)
				.Text(LOCTEXT("CleanHint", "Project-wide operations. Unlike Optimize fixes these are not Undo-able, so anything that rewrites assets asks first."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 10))
			[
				BuildProjectSizeCard()
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Cards
			]
		];
}

TSharedRef<SWidget> SToolsetWindow::BuildProjectSizeCard()
{
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(18, 16))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(LOCTEXT("SizeTitle", "Project size"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 12, 0))
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true)
						.Text_Lambda([this]()
						{
							if (!bHasSizeReport)
							{
								return LOCTEXT("SizeNotRun", "Measures every package under /Game on disk, grouped by asset type. Walks the whole project, so it runs on demand.");
							}
							if (SizeReport.bRegistryIncomplete)
							{
								return LOCTEXT("SizeIncomplete", "The asset registry is still scanning the project — the numbers would be short. Try again once it finishes.");
							}
							return FText::Format(
								LOCTEXT("SizeSummary", "{0} across {1} packages  •  measured in {2} ms"),
								FText::AsMemory(SizeReport.TotalBytes),
								FText::AsNumber(SizeReport.PackageCount),
								FText::AsNumber(FMath::RoundToInt(SizeReport.ComputeSeconds * 1000.0)));
						})
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Primary")
					.OnClicked(this, &SToolsetWindow::OnComputeProjectSize)
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
						.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0x16, 0x17, 0x19))))
						.Text_Lambda([this]()
						{
							return bHasSizeReport ? LOCTEXT("SizeRefresh", "Refresh") : LOCTEXT("SizeMeasure", "Measure");
						})
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 14, 0, 0))
			[
				SAssignNew(SizeBreakdownBox, SVerticalBox)
			]
		];
}

FReply SToolsetWindow::OnComputeProjectSize()
{
	FScopedSlowTask SlowTask(0.0f, LOCTEXT("MeasuringSize", "Measuring project size..."));
	SlowTask.MakeDialog();

	SizeReport = FProjectSizeReport::Compute();
	bHasSizeReport = true;
	RebuildSizeBreakdown();
	return FReply::Handled();
}

void SToolsetWindow::RebuildSizeBreakdown()
{
	if (!SizeBreakdownBox.IsValid())
	{
		return;
	}

	SizeBreakdownBox->ClearChildren();
	if (SizeReport.TotalBytes <= 0)
	{
		return;
	}

	// One stacked bar plus a legend: every category is on screen at once, so
	// nothing about the project's footprint is hidden behind a truncated list.
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	TSharedRef<SWrapBox> Legend = SNew(SWrapBox).UseAllottedSize(true);

	for (const FProjectSizeEntry& Entry : SizeReport.Entries)
	{
		const FLinearColor Color = FToolsetStyle::ColorForAssetCategory(Entry.Category);
		const float Fraction = static_cast<float>(
			static_cast<double>(Entry.TotalBytes) / static_cast<double>(SizeReport.TotalBytes));

		Bar->AddSlot()
		.FillWidth(FMath::Max(Fraction, KINDA_SMALL_NUMBER))
		[
			SNew(SBorder)
			.BorderImage(Brush("Toolset.Surface.Panel"))
			.BorderBackgroundColor(FSlateColor(Color))
			[
				SNullWidget::NullWidget
			]
		];

		Legend->AddSlot()
		.Padding(FMargin(0, 0, 18, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 7, 0))
			[
				SNew(SBox).WidthOverride(9).HeightOverride(9)
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Pill")).BorderBackgroundColor(FSlateColor(Color))
					[
						SNullWidget::NullWidget
					]
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
				.Text(FProjectSizeReport::LabelForCategory(Entry.Category))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(FText::Format(LOCTEXT("SizeLegendValue", "{0} · {1}%"),
					FText::AsMemory(Entry.TotalBytes),
					FText::AsNumber(FMath::RoundToInt(Fraction * 100.0f))))
			]
		];
	}

	SizeBreakdownBox->AddSlot().AutoHeight()
	[
		SNew(SBox).HeightOverride(14)
		[
			Bar
		]
	];

	SizeBreakdownBox->AddSlot().AutoHeight().Padding(FMargin(0, 14, 0, 0))
	[
		Legend
	];
}

TSharedRef<SWidget> SToolsetWindow::MakeCleanupActionCard(const ICleanupAction& Action)
{
	const ICleanupAction* ActionPtr = &Action;	// registry owns it; outlives the widget
	const FName ActionId = Action.GetId();

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(18, 16))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(Action.GetTitle())
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
						[
							SNew(SBorder)
							.BorderImage(Brush("Toolset.Pill"))
							.BorderBackgroundColor(FSlateColor(FLinearColor(
								FToolsetStyle::SeverityMajor.R, FToolsetStyle::SeverityMajor.G, FToolsetStyle::SeverityMajor.B, 0.18f)))
							.Padding(FMargin(8, 2))
							.Visibility(Action.IsDestructive() ? EVisibility::Visible : EVisibility::Collapsed)
							[
								SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
								.Text(LOCTEXT("NotUndoable", "NOT UNDOABLE"))
								.ColorAndOpacity(FSlateColor(FToolsetStyle::SeverityMajor))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 12, 0))
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true)
						.Text(Action.GetDescription())
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Primary")
					.OnClicked(this, &SToolsetWindow::OnRunCleanupAction, ActionPtr)
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
						.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0x16, 0x17, 0x19))))
						.Text(Action.GetButtonLabel())
					]
				]
			]

			// Last run summary, only once this action has been run.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Body")
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FToolsetStyle::SeverityGood))
				.Visibility_Lambda([this, ActionId]()
				{
					return CleanupResults.Contains(ActionId) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Text_Lambda([this, ActionId]()
				{
					const FText* Result = CleanupResults.Find(ActionId);
					return Result ? *Result : FText::GetEmpty();
				})
			]
		];
}

FReply SToolsetWindow::OnRunCleanupAction(const ICleanupAction* Action)
{
	if (!Action)
	{
		return FReply::Handled();
	}

	// These rewrite assets and no transaction can take them back, so make the
	// user say yes before anything touches the project.
	if (Action->IsDestructive())
	{
		const FText Message = FText::Format(
			LOCTEXT("ConfirmDestructive", "{0}\n\n{1}\n\nThis cannot be undone. Continue?"),
			Action->GetTitle(), Action->GetDescription());

		if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}
	}

	FScopedSlowTask SlowTask(0.0f, Action->GetTitle());
	SlowTask.MakeDialog();

	CleanupResults.Add(Action->GetId(), Action->Execute());
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Profile (functional console stat stacks)
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildProfilePanel()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(22, 20))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(LOCTEXT("ProfStacks", "Profiling stat stacks"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 4, 0, 14))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").AutoWrapText(true)
				.Text(LOCTEXT("ProfHint", "One-click console stat toggles for the active viewport / PIE session."))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SUniformGridPanel).SlotPadding(FMargin(6))
				+ SUniformGridPanel::Slot(0, 0)[ MakeStatCommandButton(LOCTEXT("StatFPS", "FPS"), TEXT("stat fps")) ]
				+ SUniformGridPanel::Slot(1, 0)[ MakeStatCommandButton(LOCTEXT("StatUnit", "Unit"), TEXT("stat unit")) ]
				+ SUniformGridPanel::Slot(2, 0)[ MakeStatCommandButton(LOCTEXT("StatGPU", "GPU"), TEXT("stat gpu")) ]
				+ SUniformGridPanel::Slot(0, 1)[ MakeStatCommandButton(LOCTEXT("StatScene", "Scene Rendering"), TEXT("stat scenerendering")) ]
				+ SUniformGridPanel::Slot(1, 1)[ MakeStatCommandButton(LOCTEXT("StatRHI", "RHI"), TEXT("stat rhi")) ]
				+ SUniformGridPanel::Slot(2, 1)[ MakeStatCommandButton(LOCTEXT("StatInitViews", "Init Views"), TEXT("stat initviews")) ]
				+ SUniformGridPanel::Slot(0, 2)[ MakeStatCommandButton(LOCTEXT("StatMemory", "Streaming"), TEXT("stat streaming")) ]
				+ SUniformGridPanel::Slot(1, 2)[ MakeStatCommandButton(LOCTEXT("StatGPUProfile", "GPU Profiler"), TEXT("profilegpu")) ]
				+ SUniformGridPanel::Slot(2, 2)[ MakeStatCommandButton(LOCTEXT("StatNone", "Clear All"), TEXT("stat none")) ]
			]
		];
}

TSharedRef<SWidget> SToolsetWindow::MakeStatCommandButton(const FText& Label, const FString& ConsoleCommand)
{
	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Button.Ghost")
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(10, 14))
		.OnClicked_Lambda([ConsoleCommand]()
		{
			if (GEditor)
			{
				UWorld* World = GEditor->GetEditorWorldContext().World();
				GEditor->Exec(World, *ConsoleCommand);
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel").Text(Label)
		];
}

// ---------------------------------------------------------------------------
// Placeholder panel (Optimize / Cleanup / Reports)
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildPlaceholderPanel(const FText& Title, const FText& Body, const TArray<FText>& PlannedActions)
{
	TSharedRef<SVerticalBox> ActionList = SNew(SVerticalBox);
	for (const FText& Action : PlannedActions)
	{
		ActionList->AddSlot().AutoHeight().Padding(FMargin(0, 4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 10, 0))
			[
				SNew(SBox).WidthOverride(6).HeightOverride(6)
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Pill")).BorderBackgroundColor(FSlateColor(FToolsetStyle::Accent))[ SNullWidget::NullWidget ]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true).Text(Action)
			]
		];
	}

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(22, 20))
		[
			SNew(SBorder)
			.BorderImage(Brush("Toolset.Card"))
			.Padding(FMargin(22, 20))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Title").Text(Title)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 16))
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").AutoWrapText(true).Text(Body)
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
		];
}

// ---------------------------------------------------------------------------
// Navigation / scanning / filtering
// ---------------------------------------------------------------------------
void SToolsetWindow::SelectSection(EToolsetSection Section)
{
	CurrentSection = Section;
	if (ContentSwitcher.IsValid())
	{
		ContentSwitcher->SetActiveWidgetIndex(static_cast<int32>(Section));
	}
}

FReply SToolsetWindow::OnScanClicked()
{
	RunScan();
	SelectSection(EToolsetSection::Analyze);	// jump to the results
	return FReply::Handled();
}

void SToolsetWindow::RunScan()
{
	LastScan = FLevelAnalyzer::AnalyzeCurrentLevel();
	bHasScanned = true;

	AllFindings.Reset();
	FixableFindings.Reset();
	for (const FFinding& F : LastScan.Findings)
	{
		TSharedPtr<FFinding> Ptr = MakeShared<FFinding>(F);
		AllFindings.Add(Ptr);
		if (HasSupportedFix(F))
		{
			FixableFindings.Add(Ptr);
		}
	}

	RebuildVisibleFindings();
	if (FixListView.IsValid())
	{
		FixListView->RequestListRefresh();
	}
}

bool SToolsetWindow::HasSupportedFix(const FFinding& F) const
{
	IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(F.FixId);
	return Fix && Fix->IsSupported();
}

void SToolsetWindow::ApplyFix(TSharedPtr<FFinding> Finding)
{
	if (!Finding.IsValid())
	{
		return;
	}
	if (IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Finding->FixId))
	{
		if (Fix->IsSupported())
		{
			Fix->Apply(*Finding);
		}
	}
	RunScan();	// refresh counts + both lists
}

FReply SToolsetWindow::OnApplyAllFixes()
{
	// Snapshot first — RunScan() (via each fix) would otherwise mutate the array we iterate.
	TArray<TSharedPtr<FFinding>> Snapshot = FixableFindings;
	for (const TSharedPtr<FFinding>& F : Snapshot)
	{
		if (F.IsValid())
		{
			if (IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(F->FixId))
			{
				if (Fix->IsSupported())
				{
					Fix->Apply(*F);
				}
			}
		}
	}
	RunScan();
	return FReply::Handled();
}

void SToolsetWindow::RebuildVisibleFindings()
{
	VisibleFindings.Reset();
	for (const TSharedPtr<FFinding>& F : AllFindings)
	{
		if (F.IsValid() && PassesFilter(*F))
		{
			VisibleFindings.Add(F);
		}
	}
	if (FindingsListView.IsValid())
	{
		FindingsListView->RequestListRefresh();
	}
}

bool SToolsetWindow::PassesFilter(const FFinding& F) const
{
	if (!EnabledSeverities.Contains(F.Severity))
	{
		return false;
	}
	if (!SearchFilter.IsEmpty())
	{
		const FString Haystack = F.Title.ToString() + TEXT(" ") + F.Subject.ToString();
		if (!Haystack.Contains(SearchFilter))
		{
			return false;
		}
	}
	return true;
}

void SToolsetWindow::OnSearchChanged(const FText& NewText)
{
	SearchFilter = NewText.ToString();
	RebuildVisibleFindings();
}

FReply SToolsetWindow::OnToggleSeverityFilter(ESeverity Severity)
{
	if (EnabledSeverities.Contains(Severity))
	{
		EnabledSeverities.Remove(Severity);
	}
	else
	{
		EnabledSeverities.Add(Severity);
	}
	RebuildVisibleFindings();
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Bound getters
// ---------------------------------------------------------------------------
FText SToolsetWindow::GetHeaderTitle() const
{
	switch (CurrentSection)
	{
	case EToolsetSection::Dashboard: return LOCTEXT("HdrDashboard", "Dashboard");
	case EToolsetSection::Analyze:   return LOCTEXT("HdrAnalyze", "Analyze");
	case EToolsetSection::Optimize:  return LOCTEXT("HdrOptimize", "Optimize");
	case EToolsetSection::Profile:   return LOCTEXT("HdrProfile", "Profile");
	case EToolsetSection::Cleanup:   return LOCTEXT("HdrCleanup", "Cleanup");
	default:                         return LOCTEXT("HdrReports", "Reports");
	}
}

FText SToolsetWindow::GetSummaryText() const
{
	if (!bHasScanned)
	{
		return LOCTEXT("NoScan", "No scan yet — press Scan Level to begin.");
	}
	return FText::Format(
		LOCTEXT("SummaryFmt", "{0} findings across {1} actors  •  scanned in {2} ms"),
		FText::AsNumber(LastScan.Findings.Num()),
		FText::AsNumber(LastScan.ActorsScanned),
		FText::AsNumber(FMath::RoundToInt(LastScan.ScanSeconds * 1000.0)));
}

FText SToolsetWindow::GetScoreText() const
{
	return bHasScanned ? FText::AsNumber(LastScan.HealthScore()) : FText::FromString(TEXT("—"));
}

FSlateColor SToolsetWindow::GetScoreColor() const
{
	if (!bHasScanned)
	{
		return FSlateColor(FToolsetStyle::TextSecondary);
	}
	const int32 Score = LastScan.HealthScore();
	if (Score >= 80) return FSlateColor(FToolsetStyle::SeverityGood);
	if (Score >= 55) return FSlateColor(FToolsetStyle::SeverityMajor);
	return FSlateColor(FToolsetStyle::SeverityCritical);
}

TOptional<float> SToolsetWindow::GetScorePercent() const
{
	return bHasScanned ? TOptional<float>(LastScan.HealthScore() / 100.0f) : TOptional<float>(0.0f);
}

#undef LOCTEXT_NAMESPACE
