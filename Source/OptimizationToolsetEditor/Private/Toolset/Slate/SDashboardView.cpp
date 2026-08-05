// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Slate/SDashboardView.h"
#include "Toolset/Slate/OptimizeStyle.h"
#include "Toolset/OptimizationToolsetSettings.h"
#include "Toolset/ToolsetModel.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/StyleDefaults.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SDashboardView"

void SDashboardView::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(12, 10))
		[
			SNew(SVerticalBox)

			// Always visible onboarding: installation is documented in the package,
			// while these steps cover the first use directly in the editor.
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildGettingStartedCard()
			]

			// 1. What the scanned level is (object counts).
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
			[
				BuildStatsCard()
			]

			// 2. What is wrong with it (severity counts).
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
			[
				BuildFindingsCard()
			]

			// 3. What the project weighs on disk (footprint by asset category).
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
			[
				BuildSizeCard()
			]

			// 4. Analyze thresholds, grouped by section — tune what the scan flags.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
			[
				BuildSettingsCard()
			]

			// 5. Which levels the next scan should look at.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
			[
				BuildLevelScopeCard()
			]
		]
	];
}

TSharedRef<SWidget> SDashboardView::BuildGettingStartedCard()
{
	auto Step = [](const FText& Number, const FText& Text)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0, 0, 8, 0))
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Heading")
				.ColorAndOpacity(FSlateColor(FOptimizeStyle::Accent))
				.Text(Number)
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Body")
				.AutoWrapText(true)
				.Text(Text)
			];
	};

	return MakeCard(
		LOCTEXT("GettingStartedTitle", "Getting Started"),
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			Step(LOCTEXT("StepOneNumber", "1."),
				LOCTEXT("StepOne", "Open the map you want to review. Loaded sub-levels appear in Scanning Scope below and can be included or excluded."))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
		[
			Step(LOCTEXT("StepTwoNumber", "2."),
				LOCTEXT("StepTwo", "Optionally adjust the analysis thresholds, then press Scan in the left sidebar."))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
		[
			Step(LOCTEXT("StepThreeNumber", "3."),
				LOCTEXT("StepThree", "Review findings in Optimize. Navigate to the affected object or apply a supported fix; use Analyzer, Profile and Clean Up for deeper review."))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 8, 0, 0))
		[
			SNew(STextBlock)
			.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
			.AutoWrapText(true)
			.Text(LOCTEXT("GettingStartedDocs", "Use the Docs button in the lower-left corner for installation, safety notes and the complete workflow."))
		]);
}

void SDashboardView::Scan()
{
	FScopedSlowTask SlowTask(0.0f, LOCTEXT("Measuring", "Measuring project size..."));
	SlowTask.MakeDialog();

	SizeReport = FProjectSizeReport::Compute();
	bHasSizeReport = true;
	RebuildBreakdown();

	// Loaded levels may have changed since the panel was built.
	RefreshLevelTree();
}

TSharedRef<SWidget> SDashboardView::MakeCard(const FText& Title, const TSharedRef<SWidget>& Body)
{
	return SNew(SBorder)
		.BorderImage(FOptimizeStyle::Brush("Opt.Card"))
		.Padding(FMargin(16, 14))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Heading")
				.Text(Title)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))
			[
				Body
			]
		];
}

// ---------------------------------------------------------------------------
// 1. Object counts
// ---------------------------------------------------------------------------
FText SDashboardView::LabelForLevelStat(ELevelStat Stat)
{
	switch (Stat)
	{
	case ELevelStat::Meshes:    return LOCTEXT("StatMeshes", "MESHES");
	case ELevelStat::Triangles: return LOCTEXT("StatTriangles", "POLYCOUNT");
	case ELevelStat::Actors:    return LOCTEXT("StatActors", "ACTORS");
	case ELevelStat::Materials: return LOCTEXT("StatMaterials", "MATERIALS");
	default:                    return LOCTEXT("StatLights", "LIGHTS");
	}
}

FText SDashboardView::TooltipForLevelStat(ELevelStat Stat)
{
	// Each number is a choice among several defensible ones, so it says which it
	// made: a user comparing this against the editor's own statistics window will
	// otherwise read a difference as a bug.
	switch (Stat)
	{
	case ELevelStat::Meshes:
		return LOCTEXT("StatMeshesTip", "Distinct static mesh assets placed in this level, however many actors use each.");
	case ELevelStat::Triangles:
		return LOCTEXT("StatTrianglesTip", "LOD0 triangles across every placed static mesh instance. Instanced components count once per instance. Skeletal meshes are not included.");
	case ELevelStat::Actors:
		return LOCTEXT("StatActorsTip", "Every actor in the level, including ones with nothing to render.");
	case ELevelStat::Materials:
		return LOCTEXT("StatMaterialsTip", "Distinct materials and material instances assigned on mesh components. Engine defaults are excluded.");
	default:
		return LOCTEXT("StatLightsTip", "Light components in the level, including lights on Blueprint actors.");
	}
}

int64 SDashboardView::ValueForLevelStat(const FLevelStats& Stats, ELevelStat Stat)
{
	switch (Stat)
	{
	case ELevelStat::Meshes:    return Stats.Meshes;
	case ELevelStat::Triangles: return Stats.Triangles;
	case ELevelStat::Actors:    return Stats.Actors;
	case ELevelStat::Materials: return Stats.Materials;
	default:                    return Stats.Lights;
	}
}

int64 SDashboardView::DeltaForLevelStat(ELevelStat Stat) const
{
	if (!Model.IsValid() || !Model->HasPreviousStats())
	{
		return 0;
	}
	return ValueForLevelStat(Model->GetLastScan().Stats, Stat) - ValueForLevelStat(Model->GetPreviousStats(), Stat);
}

FLinearColor SDashboardView::DeltaColorForLevelStat(ELevelStat Stat) const
{
	// Down is green because every number here is a cost: triangles, draw
	// submissions, actors to tick.
	return DeltaForLevelStat(Stat) < 0 ? FOptimizeStyle::SeverityGood : FOptimizeStyle::SeverityMajor;
}

TSharedRef<SWidget> SDashboardView::BuildStatsCard()
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4));
	for (uint8 Index = 0; Index < static_cast<uint8>(ELevelStat::Count); ++Index)
	{
		Grid->AddSlot(Index, 0)
		[
			MakeStatCell(static_cast<ELevelStat>(Index))
		];
	}

	return MakeCard(LOCTEXT("StatsTitle", "Statistics"), Grid);
}

TSharedRef<SWidget> SDashboardView::MakeStatCell(ELevelStat Stat)
{
	// Palatial stat tile: title, a hairline divider, then the big number, centred.
	return SNew(SBorder)
		.BorderImage(FOptimizeStyle::Brush("Opt.Tile"))
		.Padding(FMargin(10, 12))
		.ToolTipText(TooltipForLevelStat(Stat))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
				.Text(LabelForLevelStat(Stat))
			]

			// The signature divider between label and value.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6, 8))
			[
				SNew(SBox).HeightOverride(1.0f)
				[
					SNew(SImage).Image(FOptimizeStyle::Brush("Opt.Divider"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.StatValue")
				.Text_Lambda([this, Stat]()
				{
					return (Model.IsValid() && Model->HasScanned())
						? FText::AsNumber(ValueForLevelStat(Model->GetLastScan().Stats, Stat))
						: FText::FromString(TEXT("—"));
				})
			]

			// Delta against the previous scan. Hidden until there is one, and when
			// nothing moved — a row of "0"s would be noise on every rescan.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0, 4, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
				.Visibility_Lambda([this, Stat]()
				{
					return DeltaForLevelStat(Stat) != 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.ColorAndOpacity_Lambda([this, Stat]() { return FSlateColor(DeltaColorForLevelStat(Stat)); })
				.Text_Lambda([this, Stat]()
				{
					const int64 Delta = DeltaForLevelStat(Stat);
					// AsNumber already signs a negative; a rise needs the + spelled out.
					return Delta > 0
						? FText::Format(LOCTEXT("StatDeltaUp", "+{0}"), FText::AsNumber(Delta))
						: FText::AsNumber(Delta);
				})
			]
		];
}

// ---------------------------------------------------------------------------
// 2. Found problems
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SDashboardView::BuildFindingsCard()
{
	return MakeCard(
		LOCTEXT("FindingsTitle", "Issues"),
		SNew(SUniformGridPanel)
		.SlotPadding(FMargin(4))
		+ SUniformGridPanel::Slot(0, 0)[ MakeSeverityStatCard(ESeverity::Critical) ]
		+ SUniformGridPanel::Slot(1, 0)[ MakeSeverityStatCard(ESeverity::Major) ]
		+ SUniformGridPanel::Slot(2, 0)[ MakeSeverityStatCard(ESeverity::Minor) ]);
}

TSharedRef<SWidget> SDashboardView::MakeSeverityStatCard(ESeverity Severity)
{
	const FLinearColor Color = FOptimizeStyle::ColorForSeverity(Severity);

	FText Tooltip;
	switch (Severity)
	{
	case ESeverity::Critical: Tooltip = LOCTEXT("TipCritical", "Ships broken or a hard performance cliff — fix before the milestone."); break;
	case ESeverity::Major:    Tooltip = LOCTEXT("TipMajor", "Notable cost worth fixing soon."); break;
	default:                  Tooltip = LOCTEXT("TipMinor", "Hygiene and polish; low individual impact."); break;
	}

	// Same Palatial tile shape as the level stats: coloured label + dot, a hairline
	// divider, then the big count centred below.
	return SNew(SBorder)
		.BorderImage(FOptimizeStyle::Brush("Opt.Tile"))
		.Padding(FMargin(10, 12))
		.ToolTipText(Tooltip)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 7, 0))
				[
					SNew(SColorBlock).Color(Color).Size(FVector2D(9.0f, 9.0f))
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
					.Text(FOptimizeStyle::LabelForSeverity(Severity))
					.ColorAndOpacity(FSlateColor(Color))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6, 8))
			[
				SNew(SBox).HeightOverride(1.0f)
				[
					SNew(SImage).Image(FOptimizeStyle::Brush("Opt.Divider"))
				]
			]

			// Big count, bound so a scan (or an applied fix) redraws it.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Metric")
				.ColorAndOpacity(FSlateColor(Color))
				.Text_Lambda([this, Severity]()
				{
					return (Model.IsValid() && Model->HasScanned())
						? FText::AsNumber(Model->GetLastScan().CountBySeverity(Severity))
						: FText::FromString(TEXT("—"));
				})
			]
		];
}

// ---------------------------------------------------------------------------
// 3. Project size
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SDashboardView::BuildSizeCard()
{
	return MakeCard(
		LOCTEXT("SizeTitle", "Project Size"),
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
			.Text(this, &SDashboardView::GetSizeSummaryText)
			.AutoWrapText(true)
		]

		// Stacked bar + legend, rebuilt on each scan.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 14, 0, 0))
		[
			SAssignNew(BreakdownBox, SVerticalBox)
		]);
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
	// nothing truncated.
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	TSharedRef<SWrapBox> Legend = SNew(SWrapBox).UseAllottedSize(true);

	for (const FProjectSizeEntry& Entry : SizeReport.Entries)
	{
		const FLinearColor Color = FOptimizeStyle::ColorForAssetCategory(Entry.Category);
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

FText SDashboardView::GetSizeSummaryText() const
{
	if (!bHasSizeReport)
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

// ---------------------------------------------------------------------------
// 4. Settings
// ---------------------------------------------------------------------------
namespace
{
	using FIntSetting = int32 UOptimizationToolsetSettings::*;
	using FBoolSetting = bool UOptimizationToolsetSettings::*;

	/** A reset-to-default button, shown only while the value differs from Default. */
	template <typename Getter, typename Resetter>
	TSharedRef<SWidget> MakeResetButton(const Getter& DiffersFromDefault, const Resetter& Reset)
	{
		return SNew(SButton)
			.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Button.Icon")
			.ContentPadding(FMargin(2))
			.ToolTipText(LOCTEXT("ResetOne", "Reset to default"))
			.Visibility_Lambda([DiffersFromDefault]() { return DiffersFromDefault() ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnClicked_Lambda([Reset]() { Reset(); return FReply::Handled(); })
			[
				SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	/** One boolean row: label in the left half, checkbox + reset at the start of the right half. */
	TSharedRef<SWidget> MakeBoolSettingRow(const FText& Label, const FText& Tooltip, FBoolSetting Member, bool Default,
		const FSimpleDelegate& OnSettingsChanged)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Body")
				.Text(Label)
				.ToolTipText(Tooltip)
			]

			+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center).HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.ToolTipText(Tooltip)
					.IsChecked_Lambda([Member]()
					{
						return GetDefault<UOptimizationToolsetSettings>()->*Member ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([Member, OnSettingsChanged](ECheckBoxState State)
					{
						UOptimizationToolsetSettings* Settings = GetMutableDefault<UOptimizationToolsetSettings>();
						Settings->*Member = (State == ECheckBoxState::Checked);
						Settings->TryUpdateDefaultConfigFile();
						OnSettingsChanged.ExecuteIfBound();
					})
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
				[
					MakeResetButton(
						[Member, Default]() { return GetDefault<UOptimizationToolsetSettings>()->*Member != Default; },
						[Member, Default, OnSettingsChanged]()
						{
							UOptimizationToolsetSettings* Settings = GetMutableDefault<UOptimizationToolsetSettings>();
							Settings->*Member = Default;
							Settings->TryUpdateDefaultConfigFile();
							OnSettingsChanged.ExecuteIfBound();
						})
				]
			];
	}

	/** One threshold row: label on the left, a themed spin box + reset in the right half. */
	TSharedRef<SWidget> MakeIntSettingRow(const FText& Label, const FText& Tooltip, FIntSetting Member, int32 Default,
		int32 Min, int32 Max, int32 UiMin, int32 UiMax, const FSimpleDelegate& OnSettingsChanged)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Body")
				.Text(Label)
				.ToolTipText(Tooltip)
			]

			// A fixed-width spin box at the start of the right half — the two-column
			// feel of a stock details view — with the per-row reset beside it.
			+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center).HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(140.0f)
					[
						SNew(SSpinBox<int32>)
						.Style(&FOptimizeStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Opt.SpinBox"))
						.MinValue(Min).MaxValue(Max)
						.MinSliderValue(UiMin).MaxSliderValue(UiMax)
						.ToolTipText(Tooltip)
						.Value_Lambda([Member]() { return GetDefault<UOptimizationToolsetSettings>()->*Member; })
						.OnValueChanged_Lambda([Member](int32 V) { GetMutableDefault<UOptimizationToolsetSettings>()->*Member = V; })
						.OnValueCommitted_Lambda([Member, OnSettingsChanged](int32 V, ETextCommit::Type)
						{
							UOptimizationToolsetSettings* Settings = GetMutableDefault<UOptimizationToolsetSettings>();
							Settings->*Member = V;
							Settings->TryUpdateDefaultConfigFile();	// persist to DefaultEditor config
							OnSettingsChanged.ExecuteIfBound();
						})
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
				[
					MakeResetButton(
						[Member, Default]() { return GetDefault<UOptimizationToolsetSettings>()->*Member != Default; },
						[Member, Default, OnSettingsChanged]()
						{
							UOptimizationToolsetSettings* Settings = GetMutableDefault<UOptimizationToolsetSettings>();
							Settings->*Member = Default;
							Settings->TryUpdateDefaultConfigFile();
							OnSettingsChanged.ExecuteIfBound();
						})
				]
			];
	}
}

TSharedRef<SWidget> SDashboardView::BuildSettingsCard()
{
	// Custom rows (rather than a stock details view) so the thresholds sit in our
	// own dark/teal style. Each section is a collapsible area; edits persist to
	// config and the analyzer reads them on the next scan.
	using S = UOptimizationToolsetSettings;
	const FSimpleDelegate OnSettingsChanged = FSimpleDelegate::CreateLambda(
		[WeakModel = TWeakPtr<FToolsetModel>(Model)]()
		{
			if (const TSharedPtr<FToolsetModel> PinnedModel = WeakModel.Pin())
			{
				PinnedModel->InvalidateScan();
			}
		});

	// A collapsible section: a titled header over its threshold rows. Flat (no default
	// rounded border/background) so it matches the rest of the card.
	auto MakeSection = [](const FText& Title, const TSharedRef<SVerticalBox>& Rows)
	{
		return SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.BorderBackgroundColor(FLinearColor::Transparent)
			.HeaderPadding(FMargin(2, 6))
			// Left inset indents the rows under the section header.
			.Padding(FMargin(36, 2, 8, 8))
			.HeaderContent()
			[
				SNew(STextBlock).TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Heading").Text(Title)
			]
			.BodyContent()
			[
				Rows
			];
	};

	auto Rows = []() { return SNew(SVerticalBox); };
	auto Add = [](const TSharedRef<SVerticalBox>& Box, const TSharedRef<SWidget>& Row)
	{
		Box->AddSlot().AutoHeight().Padding(FMargin(0, 3))[ Row ];
	};

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto AddSection = [&Body, &MakeSection](const FText& Title, const TSharedRef<SVerticalBox>& Rows)
	{
		Body->AddSlot().AutoHeight().Padding(FMargin(0, 1))[ MakeSection(Title, Rows) ];
	};

	// Scan
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeBoolSettingRow(LOCTEXT("SetIncludeSub", "Include sub-levels by default"),
			LOCTEXT("SetIncludeSubTip", "When on, newly discovered loaded sub-levels start included in the scan scope. Each level can still be toggled below."),
			&S::bIncludeSubLevels, S::Default_bIncludeSubLevels, OnSettingsChanged));
		AddSection(LOCTEXT("SecScan", "Scan"), R);
	}

	// Meshes
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeIntSettingRow(LOCTEXT("SetNaniteMin", "Nanite low-poly threshold"),
			LOCTEXT("SetNaniteMinTip", "Triangles at or below which an enabled Nanite mesh is flagged for review (0 disables)."),
			&S::NaniteMinimumTriangles, S::Default_NaniteMinimumTriangles, 0, 1000000, 0, 20000, OnSettingsChanged));
		Add(R, MakeIntSettingRow(LOCTEXT("SetNaniteCand", "Nanite candidate triangles"),
			LOCTEXT("SetNaniteCandTip", "Triangles above which a non-Nanite mesh is suggested for Nanite."),
			&S::NaniteCandidateTriangles, S::Default_NaniteCandidateTriangles, 1000, 10000000, 1000, 1000000, OnSettingsChanged));
		Add(R, MakeIntSettingRow(LOCTEXT("SetExcessive", "Excessive triangles"),
			LOCTEXT("SetExcessiveTip", "Triangle count treated as a critical cost on a single non-Nanite mesh."),
			&S::ExcessiveTriangles, S::Default_ExcessiveTriangles, 10000, 100000000, 10000, 5000000, OnSettingsChanged));
		AddSection(LOCTEXT("SecMeshes", "Meshes"), R);
	}

	// Textures
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeIntSettingRow(LOCTEXT("SetTexDensity", "Texture density budget (texels/m)"),
			LOCTEXT("SetTexDensityTip", "Texels per metre a texture may deliver before it is called oversized."),
			&S::TextureDensityBudget, S::Default_TextureDensityBudget, 128, 16384, 512, 8192, OnSettingsChanged));
		Add(R, MakeIntSettingRow(LOCTEXT("SetOversized", "Oversized texture size"),
			LOCTEXT("SetOversizedTip", "Fallback size limit used when streaming data can't say how large a texture appears."),
			&S::OversizedTextureSize, S::Default_OversizedTextureSize, 256, 16384, 256, 8192, OnSettingsChanged));
		AddSection(LOCTEXT("SecTextures", "Textures"), R);
	}

	// Materials
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeIntSettingRow(LOCTEXT("SetSlots", "Material slot budget"),
			LOCTEXT("SetSlotsTip", "Material slots allowed on a mesh before it is flagged."),
			&S::MaterialSlotBudget, S::Default_MaterialSlotBudget, 1, 64, 1, 32, OnSettingsChanged));
		Add(R, MakeIntSettingRow(LOCTEXT("SetSamplers", "Material sampler budget"),
			LOCTEXT("SetSamplersTip", "Texture samplers a material may use before it is flagged (hard limit 16)."),
			&S::MaterialSamplerBudget, S::Default_MaterialSamplerBudget, 1, 16, 4, 16, OnSettingsChanged));
		Add(R, MakeIntSettingRow(LOCTEXT("SetInstr", "Material instruction budget"),
			LOCTEXT("SetInstrTip", "Shader instructions a material may reach before review."),
			&S::MaterialInstructionBudget, S::Default_MaterialInstructionBudget, 50, 10000, 100, 2000, OnSettingsChanged));
		AddSection(LOCTEXT("SecMaterials", "Materials"), R);
	}

	// Lighting
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeIntSettingRow(LOCTEXT("SetMovable", "Movable light budget"),
			LOCTEXT("SetMovableTip", "Movable lights allowed in each loaded level before per-light findings appear."),
			&S::MovableLightBudget, S::Default_MovableLightBudget, 0, 512, 0, 128, OnSettingsChanged));
		Add(R, MakeIntSettingRow(LOCTEXT("SetLightmap", "Lightmap resolution budget"),
			LOCTEXT("SetLightmapTip", "Lightmap resolution a single component may use before review."),
			&S::LightmapResolutionBudget, S::Default_LightmapResolutionBudget, 32, 4096, 64, 2048, OnSettingsChanged));
		AddSection(LOCTEXT("SecLighting", "Lighting"), R);
	}

	// Instancing
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeIntSettingRow(LOCTEXT("SetInstancing", "Instancing candidate count"),
			LOCTEXT("SetInstancingTip", "Minimum compatible repeated actors required for an ISM/HISM recommendation."),
			&S::InstancingCandidateCount, S::Default_InstancingCandidateCount, 2, 10000, 2, 100, OnSettingsChanged));
		AddSection(LOCTEXT("SecInstancing", "Instancing"), R);
	}

	// Blueprints
	{
		TSharedRef<SVerticalBox> R = Rows();
		Add(R, MakeIntSettingRow(LOCTEXT("SetDepChain", "Dependency chain size (MB)"),
			LOCTEXT("SetDepChainTip", "Disk a Blueprint's hard-reference chain may reach before it is flagged."),
			&S::DependencyChainSizeMB, S::Default_DependencyChainSizeMB, 1, 100000, 8, 1024, OnSettingsChanged));
		AddSection(LOCTEXT("SecBlueprints", "Blueprints"), R);
	}

	// Reset-to-defaults, right-aligned under the sections. The spin boxes read the
	// settings through Value lambdas, so they redraw with the restored values.
	Body->AddSlot().AutoHeight().Padding(FMargin(0, 12, 8, 0))
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]

		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Button.Secondary")
			.ToolTipText(LOCTEXT("ResetTip", "Restore every threshold to its shipped default."))
			.OnClicked_Lambda([OnSettingsChanged]()
			{
				GetMutableDefault<UOptimizationToolsetSettings>()->ResetToDefaults();
				OnSettingsChanged.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Body").Text(LOCTEXT("ResetDefaults", "Reset to Defaults"))
			]
		]
	];

	return MakeCard(LOCTEXT("SettingsTitle", "Settings"), Body);
}

// ---------------------------------------------------------------------------
// 5. Level scan scope
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SDashboardView::BuildLevelScopeCard()
{
	RefreshLevelTree();

	TSharedRef<SWidget> Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
				.Text(LOCTEXT("LevelScopeHint", "Changing scope clears stale results; unchecked levels are skipped by the next scan."))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
			[
				SNew(SButton)
				.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Button.Secondary")
				.OnClicked(this, &SDashboardView::OnRefreshLevelsClicked)
				[
					SNew(STextBlock)
					.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Body")
					.Text(LOCTEXT("RefreshLevels", "Refresh"))
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))
		[
			SNew(SBox).HeightOverride(180.0f)
			[
				SAssignNew(LevelTree, STreeView<TSharedPtr<FDashboardLevelItem>>)
				.TreeViewStyle(&FOptimizeStyle::Get().GetWidgetStyle<FTableViewStyle>("Opt.TreeView"))
				.TreeItemsSource(&LevelRoots)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SDashboardView::GenerateLevelRow)
				.OnGetChildren(this, &SDashboardView::GetLevelChildren)
			]
		];

	TSharedRef<SWidget> Card = MakeCard(LOCTEXT("ScopeTitle", "Scanning Scope"), Body);

	if (LevelTree.IsValid() && !LevelRoots.IsEmpty())
	{
		LevelTree->SetItemExpansion(LevelRoots[0], true);
	}
	return Card;
}

void SDashboardView::RefreshLevelTree()
{
	LevelRoots.Reset();

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !World->PersistentLevel)
	{
		if (LevelTree.IsValid())
		{
			LevelTree->RequestTreeRefresh();
		}
		return;
	}

	auto MakeItem = [](ULevel* Level)
	{
		TSharedPtr<FDashboardLevelItem> Item = MakeShared<FDashboardLevelItem>();
		Item->PackageName = Level->GetOutermost()->GetFName();
		Item->Label = FText::FromString(FPackageName::GetShortName(Item->PackageName.ToString()));
		for (AActor* Actor : Level->Actors)
		{
			if (Actor)
			{
				++Item->ActorCount;
			}
		}
		return Item;
	};

	TSharedPtr<FDashboardLevelItem> Root = MakeItem(World->PersistentLevel);
	Root->bPersistentLevel = true;
	for (ULevel* Level : World->GetLevels())
	{
		if (Level && Level != World->PersistentLevel)
		{
			Root->Children.Add(MakeItem(Level));
		}
	}
	Root->Children.Sort([](const TSharedPtr<FDashboardLevelItem>& A, const TSharedPtr<FDashboardLevelItem>& B)
	{
		return A.IsValid() && B.IsValid() && A->Label.CompareTo(B->Label) < 0;
	});
	LevelRoots.Add(Root);

	if (LevelTree.IsValid())
	{
		LevelTree->RequestTreeRefresh();
		LevelTree->SetItemExpansion(Root, true);
	}
}

FReply SDashboardView::OnRefreshLevelsClicked()
{
	RefreshLevelTree();
	return FReply::Handled();
}

void SDashboardView::GetLevelChildren(
	TSharedPtr<FDashboardLevelItem> Item,
	TArray<TSharedPtr<FDashboardLevelItem>>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren.Append(Item->Children);
	}
}

TSharedRef<ITableRow> SDashboardView::GenerateLevelRow(
	TSharedPtr<FDashboardLevelItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bPersistent = Item.IsValid() && Item->bPersistentLevel;

	return SNew(STableRow<TSharedPtr<FDashboardLevelItem>>, OwnerTable)
		.Style(&FOptimizeStyle::Get(), "Opt.TableRow")
		.Padding(FMargin(8, 5))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this, Item]()
				{
					return (Model.IsValid() && Item.IsValid()
						&& Model->IsLevelIncluded(Item->PackageName, Item->bPersistentLevel))
						? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState State)
				{
					if (Model.IsValid() && Item.IsValid())
					{
						Model->SetLevelIncluded(Item->PackageName, State == ECheckBoxState::Checked);
					}
				})
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), bPersistent ? "Opt.Text.Heading" : "Opt.Text.Body")
				.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
				.ColorAndOpacity(bPersistent ? FSlateColor(FOptimizeStyle::Accent) : FSlateColor(FOptimizeStyle::TextPrimary))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10, 0, 6, 0))
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
				.Text(Item.IsValid()
					? FText::Format(LOCTEXT("LevelActorCount", "{0} actors"), FText::AsNumber(Item->ActorCount))
					: FText::GetEmpty())
			]
		];
}

#undef LOCTEXT_NAMESPACE
