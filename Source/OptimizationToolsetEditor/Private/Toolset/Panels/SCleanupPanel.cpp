// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SCleanupPanel.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SCleanupPanel"

using namespace ToolsetUI;

void SCleanupPanel::Construct(const FArguments& InArgs)
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

	ChildSlot
	[
		SNew(SScrollBox)
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
		]
	];
}

TSharedRef<SWidget> SCleanupPanel::BuildProjectSizeCard()
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
					.OnClicked(this, &SCleanupPanel::OnComputeProjectSize)
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
						.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
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

FReply SCleanupPanel::OnComputeProjectSize()
{
	FScopedSlowTask SlowTask(0.0f, LOCTEXT("MeasuringSize", "Measuring project size..."));
	SlowTask.MakeDialog();

	SizeReport = FProjectSizeReport::Compute();
	bHasSizeReport = true;
	RebuildSizeBreakdown();
	return FReply::Handled();
}

void SCleanupPanel::RebuildSizeBreakdown()
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
			.BorderImage(Brush("Toolset.Fill"))
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
					SNew(SBorder).BorderImage(Brush("Toolset.Fill.Rounded")).BorderBackgroundColor(FSlateColor(Color))
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

TSharedRef<SWidget> SCleanupPanel::MakeCleanupActionCard(const ICleanupAction& Action)
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
							.BorderImage(Brush("Toolset.Fill.Pill"))
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
					.OnClicked(this, &SCleanupPanel::OnRunCleanupAction, ActionPtr)
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
						.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
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

FReply SCleanupPanel::OnRunCleanupAction(const ICleanupAction* Action)
{
	if (!Action)
	{
		return FReply::Handled();
	}

	// These rewrite assets and no transaction can take them back, so make the
	// user say yes before anything touches the project — unless the action runs
	// a better review of its own.
	if (Action->NeedsConfirmation())
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

#undef LOCTEXT_NAMESPACE
