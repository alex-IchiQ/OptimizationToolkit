// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SOptimizePanel.h"
#include "Toolset/Panels/SFindingTree.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/ToolsetWidgetUtils.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SOptimizePanel"

using namespace ToolsetUI;

void SOptimizePanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	ChildSlot
	[
		SNew(SVerticalBox)

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
						if (!Model.IsValid() || !Model->HasScanned())
						{
							return LOCTEXT("OptNoScan", "Run a scan to find fixable issues.");
						}
						const int32 Count = Model->GetFixableFindings().Num();
						if (Count == 0)
						{
							return LOCTEXT("OptNone", "No auto-fixable issues found.");
						}
						return FText::Format(
							LOCTEXT("OptCount", "{0} issues can be fixed automatically — each fix is transactional and Undo-able."),
							FText::AsNumber(Count));
					})
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(&S(), "Toolset.Button.Primary")
				.Visibility_Lambda([this]()
				{
					return (Model.IsValid() && Model->GetFixableFindings().Num() > 0)
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this]()
				{
					if (Model.IsValid())
					{
						Model->ApplyAllFixes();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
					.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
					.Text(LOCTEXT("ApplyAll", "Apply all"))
				]
			]
		]

		// Fixable findings.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(16, 6, 16, 16))
		[
			SAssignNew(Tree, SFindingTree)
			.OnMakeCard(FOnMakeFindingCard::CreateSP(this, &SOptimizePanel::MakeFixCard))
		]
	];

	if (Model.IsValid())
	{
		ChangedHandle = Model->OnChanged().AddSP(this, &SOptimizePanel::Refresh);
		Refresh();
	}
}

SOptimizePanel::~SOptimizePanel()
{
	if (Model.IsValid())
	{
		Model->OnChanged().Remove(ChangedHandle);
	}
}

void SOptimizePanel::Refresh()
{
	if (Model.IsValid() && Tree.IsValid())
	{
		Tree->SetFindings(Model->GetVisibleFixable());
	}
}

TSharedRef<SWidget> SOptimizePanel::MakeFixCard(TSharedPtr<FFinding> Item)
{
	const FLinearColor SevColor = FToolsetStyle::ColorForSeverity(Item->Severity);
	IOptimizationFix* Fix = FToolsetRegistry::Get().FindFix(Item->FixId);
	const FText FixLabel = Fix ? Fix->GetLabel() : LOCTEXT("FixGeneric", "Fix");

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(0)
		[
			SNew(SHorizontalBox)

			// Severity stripe.
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(4)
				[
					SNew(SBorder).BorderImage(Brush("Toolset.Fill")).BorderBackgroundColor(FSlateColor(SevColor))[ SNullWidget::NullWidget ]
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
				// Which sub-level this is in — Apply changes a level the user may not
				// even have open, so it matters more here than in Analyze.
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 2, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
					.ColorAndOpacity(FSlateColor(FToolsetStyle::Accent))
					.Visibility(Item->LevelName.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					.Text(Item->LevelName)
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
				.OnClicked_Lambda([this, Item]()
				{
					if (Model.IsValid())
					{
						Model->ApplyFix(Item);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
					.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
					.Text(FixLabel)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
