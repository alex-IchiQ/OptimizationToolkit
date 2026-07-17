// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SAnalyzePanel.h"
#include "Toolset/Panels/SFindingTree.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetWidgetUtils.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SAnalyzePanel"

using namespace ToolsetUI;

void SAnalyzePanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	ChildSlot
	[
		SNew(SVerticalBox)

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
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Filter findings…"))
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					if (Model.IsValid())
					{
						Model->SetSearchFilter(NewText.ToString());
					}
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10, 0, 0, 0)).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))[ MakeSeverityFilterButton(ESeverity::Critical) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))[ MakeSeverityFilterButton(ESeverity::Major) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3, 0))[ MakeSeverityFilterButton(ESeverity::Minor) ]
			]
		]

		// Findings list.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(16, 6, 16, 16))
		[
			SAssignNew(Tree, SFindingTree)
			.OnMakeCard(FOnMakeFindingCard::CreateSP(this, &SAnalyzePanel::MakeFindingCard))
		]
	];

	if (Model.IsValid())
	{
		ChangedHandle = Model->OnChanged().AddSP(this, &SAnalyzePanel::Refresh);
		Refresh();
	}
}

SAnalyzePanel::~SAnalyzePanel()
{
	// The model outlives its panels (the window owns both), so the handle would
	// dangle into a destroyed widget without this.
	if (Model.IsValid())
	{
		Model->OnChanged().Remove(ChangedHandle);
	}
}

void SAnalyzePanel::Refresh()
{
	if (Model.IsValid() && Tree.IsValid())
	{
		Tree->SetFindings(Model->GetVisibleFindings());
	}
}

TSharedRef<SWidget> SAnalyzePanel::MakeSeverityFilterButton(ESeverity Severity)
{
	const FLinearColor Color = FToolsetStyle::ColorForSeverity(Severity);

	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Button.Ghost")
		.OnClicked_Lambda([this, Severity]()
		{
			if (Model.IsValid())
			{
				Model->ToggleSeverity(Severity);
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.TextStyle(&S(), "Toolset.Text.Body")
			.Text(FToolsetStyle::LabelForSeverity(Severity))
			// Lit in its severity colour when on, greyed when off: the button is
			// its own state readout, so no separate checkbox is needed.
			.ColorAndOpacity_Lambda([this, Severity, Color]()
			{
				return (Model.IsValid() && Model->IsSeverityEnabled(Severity))
					? FSlateColor(Color) : FSlateColor(FToolsetStyle::TextSecondary);
			})
		];
}

TSharedRef<SWidget> SAnalyzePanel::MakeFindingCard(TSharedPtr<FFinding> Item)
{
	const FLinearColor SevColor = FToolsetStyle::ColorForSeverity(Item->Severity);

	TSharedRef<SHorizontalBox> Pills = SNew(SHorizontalBox);

	// Severity pill.
	Pills->AddSlot().AutoWidth().Padding(FMargin(0, 0, 6, 0))
	[
		SNew(SBorder).BorderImage(Brush("Toolset.Fill.Pill")).BorderBackgroundColor(FSlateColor(FLinearColor(SevColor.R, SevColor.G, SevColor.B, 0.18f)))
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
	// Sub-level pill, only when the actor isn't in the level the user has open —
	// on a streamed map the name alone doesn't say where to go looking.
	if (!Item->LevelName.IsEmpty())
	{
		Pills->AddSlot().AutoWidth().Padding(FMargin(6, 0, 0, 0))
		[
			SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(8, 2))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(Item->LevelName)
				.ColorAndOpacity(FSlateColor(FToolsetStyle::Accent))
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(0)
		[
			SNew(SHorizontalBox)

			// Severity colour stripe.
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
		];
}

#undef LOCTEXT_NAMESPACE
