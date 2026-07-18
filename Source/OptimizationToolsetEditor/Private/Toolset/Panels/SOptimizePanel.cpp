// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SOptimizePanel.h"
#include "Toolset/Panels/SFindingTree.h"
#include "Toolset/Panels/SFindingCard.h"
#include "Toolset/Panels/SCategorySettingsPanel.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SOptimizePanel"

using namespace ToolsetUI;

void SOptimizePanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Search and severity filters apply to the unified findings tree.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(22, 16, 22, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
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

		// Category-specific thresholds come before the category's object tree.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(22, 6, 22, 8))
		[
			SNew(SCategorySettingsPanel)
			.Model(Model)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(16, 0, 16, 16))
		[
			SAssignNew(Tree, SFindingTree)
			.OnMakeCard(FOnMakeFindingCard::CreateSP(this, &SOptimizePanel::MakeFindingCard))
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
		Tree->SetFindings(Model->GetVisibleFindings());
	}
}

TSharedRef<SWidget> SOptimizePanel::MakeSeverityFilterButton(ESeverity Severity)
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
			.ColorAndOpacity_Lambda([this, Severity, Color]()
			{
				return Model.IsValid() && Model->IsSeverityEnabled(Severity)
					? FSlateColor(Color) : FSlateColor(FToolsetStyle::TextSecondary);
			})
		];
}

TSharedRef<SWidget> SOptimizePanel::MakeFindingCard(TSharedPtr<FFinding> Item)
{
	return SNew(SFindingCard)
		.Model(Model)
		.Finding(Item);
}

#undef LOCTEXT_NAMESPACE
