// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SOptimizeShell.h"
#include "Toolset/Slate/SCleanupView.h"
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

			+ SWidgetSwitcher::Slot()[ BuildDashboardPlaceholder() ]   // Dashboard
			+ SWidgetSwitcher::Slot()[ SNew(SOptimizeView) ]           // Optimize
			+ SWidgetSwitcher::Slot()[ SNew(SProfileView) ]            // Profile
			+ SWidgetSwitcher::Slot()[ SNew(SCleanupView) ]            // Cleanup
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

		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Dashboard, LOCTEXT("NavDashboard", "Dashboard")) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeNavButton(EOptimizeSection::Optimize, LOCTEXT("NavOptimize", "Optimize")) ]
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

TSharedRef<SWidget> SOptimizeShell::BuildDashboardPlaceholder()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DashboardSoon", "Dashboard"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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

#undef LOCTEXT_NAMESPACE
