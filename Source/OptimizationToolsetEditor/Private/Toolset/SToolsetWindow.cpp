// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/SToolsetWindow.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetStyle.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Toolset/Panels/SCleanupPanel.h"
#include "Toolset/Panels/SDashboardPanel.h"
#include "Toolset/Panels/SOptimizePanel.h"
#include "Toolset/Panels/SPlaceholderPanel.h"
#include "Toolset/Panels/SProfilePanel.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"

#define LOCTEXT_NAMESPACE "SToolsetWindow"

using namespace ToolsetUI;

void SToolsetWindow::Construct(const FArguments& InArgs)
{
	Model = MakeShared<FToolsetModel>();
	ExpandedNavSections.Add(EToolsetSection::Optimize);

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

				// Scan leads the sidebar: it is the one action every section depends
				// on, so it belongs where the eye starts, not at the far end of a
				// header that changes with the section. The window's own tab already
				// names the plugin, so no brand block competes with it here.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 16))
				[
					BuildScanButton()
				]

				// Optimize is the single findings workspace. Its categories remain
				// available before a scan so their thresholds can be configured first.
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Dashboard, LOCTEXT("NavDashboard", "Dashboard"), "Toolset.Icon.Dashboard") ]

				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 1))[ BuildNavItem(EToolsetSection::Optimize,  LOCTEXT("NavOptimize",  "Optimize"),  "Toolset.Icon.Optimize") ]
				+ SVerticalBox::Slot().AutoHeight()[ BuildNavCategoryList(EToolsetSection::Optimize) ]

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

TSharedRef<SWidget> SToolsetWindow::BuildScanButton()
{
	return SNew(SBox)
		.HeightOverride(84.0f)
		[
			SNew(SButton)
			.ButtonStyle(&S(), "Toolset.Button.Scan")
			.ContentPadding(FMargin(0))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(LOCTEXT("ScanTooltip", "Analyze the active level and refresh every panel."))
			.OnClicked(this, &SToolsetWindow::OnScanClicked)
			[
				SNew(SVerticalBox)

				// Icon above label, so the button reads as a block rather than a
				// text pill. The SVG is white, so a dark tint lands as authored —
				// on the accent fill, dark is what stays legible.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(26).HeightOverride(26)
					[
						SNew(SImage)
						.Image(Brush("Toolset.Icon.Scan"))
						.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0, 8, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.NavLabel")
					.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
					.Text(LOCTEXT("ScanBtn", "Scan Level"))
				]
			]
		];
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
bool SToolsetWindow::SectionHasCategories(EToolsetSection Section)
{
	return Section == EToolsetSection::Optimize;
}

bool SToolsetWindow::IsNavExpanded(EToolsetSection Section) const
{
	return ExpandedNavSections.Contains(Section);
}

FReply SToolsetWindow::OnNavItemClicked(EToolsetSection Section)
{
	// Clicking the section you are already in folds its category list away,
	// so the arrow isn't a separate hit target inside a button.
	if (IsSectionSelected(Section) && SectionHasCategories(Section) && !Model->GetCategoryFilter().IsSet())
	{
		if (IsNavExpanded(Section))
		{
			ExpandedNavSections.Remove(Section);
		}
		else
		{
			ExpandedNavSections.Add(Section);
		}
		return FReply::Handled();
	}

	// The section header means "everything in here", so it clears any category.
	SelectSection(Section);
	if (SectionHasCategories(Section))
	{
		ExpandedNavSections.Add(Section);
	}
	Model->SetCategoryFilter(TOptional<ECategory>());
	return FReply::Handled();
}

void SToolsetWindow::SelectSectionCategory(EToolsetSection Section, ECategory Category)
{
	SelectSection(Section);
	Model->SetCategoryFilter(Category);
}

bool SToolsetWindow::IsNavCategorySelected(EToolsetSection Section, ECategory Category) const
{
	const TOptional<ECategory> Filter = Model->GetCategoryFilter();
	return IsSectionSelected(Section) && Filter.IsSet() && Filter.GetValue() == Category;
}

int32 SToolsetWindow::CountForNavCategory(ECategory Category) const
{
	return Model->CountForCategory(Category);
}

TSharedRef<SWidget> SToolsetWindow::BuildNavCategoryList(EToolsetSection Section)
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (uint8 Index = 0; Index < static_cast<uint8>(ECategory::Count); ++Index)
	{
		List->AddSlot().AutoHeight()
		[
			BuildNavSubItem(Section, static_cast<ECategory>(Index))
		];
	}
	return List;
}

TSharedRef<SWidget> SToolsetWindow::BuildNavSubItem(EToolsetSection Section, ECategory Category)
{
	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Nav.Button")
		.ContentPadding(FMargin(0))
		.Visibility_Lambda([this, Section, Category]()
		{
			return IsNavExpanded(Section) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.OnClicked_Lambda([this, Section, Category]()
		{
			SelectSectionCategory(Section, Category);
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			// Indented to sit under the parent's label, not its icon.
			.Padding(FMargin(32, 4, 10, 4))
			.BorderImage(Brush("Toolset.Nav.Selected"))
			.BorderBackgroundColor_Lambda([this, Section, Category]()
			{
				return IsNavCategorySelected(Section, Category) ? FLinearColor::White : FLinearColor(1, 1, 1, 0.0f);
			})
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Body")
					.Text(FToolsetStyle::LabelForCategory(Category))
					.ColorAndOpacity_Lambda([this, Section, Category]()
					{
						return IsNavCategorySelected(Section, Category)
							? FSlateColor(FToolsetStyle::TextPrimary)
							: FSlateColor(FToolsetStyle::TextSecondary);
					})
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
					.Text_Lambda([this, Category]()
					{
						return FText::AsNumber(CountForNavCategory(Category));
					})
				]
			]
		];
}

TSharedRef<SWidget> SToolsetWindow::BuildNavItem(EToolsetSection Section, const FText& Label, const FName& IconName)
{
	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Nav.Button")
		.ContentPadding(FMargin(0))
		.OnClicked(this, &SToolsetWindow::OnNavItemClicked, Section)
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

				// Expansion arrow: an indicator, not a second hit target — the row
				// itself toggles the list.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(10).HeightOverride(10)
					.Visibility(SectionHasCategories(Section) ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(SImage)
						.Image_Lambda([this, Section]()
						{
							return IsNavExpanded(Section)
								? FAppStyle::GetBrush("TreeArrow_Expanded")
								: FAppStyle::GetBrush("TreeArrow_Collapsed");
						})
						.ColorAndOpacity(FSlateColor(FToolsetStyle::TextSecondary))
					]
				]
			]
		];
}

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
	Model->RunScan();
	SelectSection(EToolsetSection::Optimize);	// jump to the results
	ExpandedNavSections.Add(EToolsetSection::Optimize);
	return FReply::Handled();
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

			// Health score chip. Scan used to sit to its right; it now lives in the
			// sidebar, so this is the header's last slot.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
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
		];
}

// ---------------------------------------------------------------------------
// Content switcher
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SToolsetWindow::BuildContent()
{
	TArray<FText> ReportActions = {
		LOCTEXT("RepAct2", "Before / after snapshots to prove wins"),
		LOCTEXT("RepAct3", "Per-category summary for milestone reviews"),
	};

	// Slot order must match EToolsetSection: the switcher is indexed by it.
	ContentSwitcher = SNew(SWidgetSwitcher);
	ContentSwitcher->AddSlot()[ SNew(SDashboardPanel).Model(Model) ];
	ContentSwitcher->AddSlot()[ SNew(SOptimizePanel).Model(Model) ];
	ContentSwitcher->AddSlot()[ SNew(SProfilePanel) ];
	ContentSwitcher->AddSlot()[ SNew(SCleanupPanel) ];
	ContentSwitcher->AddSlot()
	[
		SNew(SPlaceholderPanel)
		.Title(LOCTEXT("ReportsTitle", "Reports & exports"))
		.Body(LOCTEXT("ReportsBody", "Turn a scan into a shareable report."))
		.PlannedActions(ReportActions)
	];

	// Wrap the switched content in its own island so it floats like the rest.
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Island"))
		.Padding(FMargin(4))
		[
			ContentSwitcher.ToSharedRef()
		];
}

// ---------------------------------------------------------------------------
// Bound getters
// ---------------------------------------------------------------------------
FText SToolsetWindow::GetHeaderTitle() const
{
	switch (CurrentSection)
	{
	case EToolsetSection::Dashboard: return LOCTEXT("HdrDashboard", "Dashboard");
	case EToolsetSection::Optimize:  return LOCTEXT("HdrOptimize", "Optimize");
	case EToolsetSection::Profile:   return LOCTEXT("HdrProfile", "Profile");
	case EToolsetSection::Cleanup:   return LOCTEXT("HdrCleanup", "Cleanup");
	default:                         return LOCTEXT("HdrReports", "Reports");
	}
}

FText SToolsetWindow::GetSummaryText() const
{
	if (!Model->HasScanned())
	{
		return LOCTEXT("NoScan", "No scan yet — press Scan Level to begin.");
	}

	const FScanResult& Scan = Model->GetLastScan();
	return FText::Format(
		LOCTEXT("SummaryFmt", "{0} findings across {1} actors  •  scanned in {2} ms"),
		FText::AsNumber(Scan.Findings.Num()),
		FText::AsNumber(Scan.ActorsScanned),
		FText::AsNumber(FMath::RoundToInt(Scan.ScanSeconds * 1000.0)));
}

FText SToolsetWindow::GetScoreText() const
{
	return Model->HasScanned() ? FText::AsNumber(Model->GetLastScan().HealthScore()) : FText::FromString(TEXT("—"));
}

FSlateColor SToolsetWindow::GetScoreColor() const
{
	if (!Model->HasScanned())
	{
		return FSlateColor(FToolsetStyle::TextSecondary);
	}
	const int32 Score = Model->GetLastScan().HealthScore();
	if (Score >= 80) return FSlateColor(FToolsetStyle::SeverityGood);
	if (Score >= 55) return FSlateColor(FToolsetStyle::SeverityMajor);
	return FSlateColor(FToolsetStyle::SeverityCritical);
}

TOptional<float> SToolsetWindow::GetScorePercent() const
{
	return Model->HasScanned() ? TOptional<float>(Model->GetLastScan().HealthScore() / 100.0f) : TOptional<float>(0.0f);
}

#undef LOCTEXT_NAMESPACE
