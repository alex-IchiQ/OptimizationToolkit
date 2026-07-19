// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SDashboardView.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetStyle.h"	// ColorForAssetCategory / ColorForSeverity (colour helpers)

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
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

			// 1. What the scanned level is (object counts).
			+ SVerticalBox::Slot().AutoHeight()
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

			// 4. Which levels the next scan should look at.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
			[
				BuildLevelScopeCard()
			]
		]
	];
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
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(14, 12))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
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
	return DeltaForLevelStat(Stat) < 0 ? FToolsetStyle::SeverityGood : FToolsetStyle::SeverityMajor;
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

	return MakeCard(LOCTEXT("StatsTitle", "Level at a glance"), Grid);
}

TSharedRef<SWidget> SDashboardView::MakeStatCell(ELevelStat Stat)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(12, 10))
		.ToolTipText(TooltipForLevelStat(Stat))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LabelForLevelStat(Stat))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0, 6, 0, 0))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
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
		LOCTEXT("FindingsTitle", "Found problems"),
		SNew(SUniformGridPanel)
		.SlotPadding(FMargin(4))
		+ SUniformGridPanel::Slot(0, 0)[ MakeSeverityStatCard(ESeverity::Critical) ]
		+ SUniformGridPanel::Slot(1, 0)[ MakeSeverityStatCard(ESeverity::Major) ]
		+ SUniformGridPanel::Slot(2, 0)[ MakeSeverityStatCard(ESeverity::Minor) ]);
}

TSharedRef<SWidget> SDashboardView::MakeSeverityStatCard(ESeverity Severity)
{
	const FLinearColor Color = FToolsetStyle::ColorForSeverity(Severity);

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(14, 12))
		[
			SNew(SVerticalBox)

			// Coloured severity label with a leading dot.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 8, 0))
				[
					SNew(SColorBlock).Color(Color).Size(FVector2D(9.0f, 9.0f))
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FToolsetStyle::LabelForSeverity(Severity))
					.ColorAndOpacity(FSlateColor(Color))
				]
			]

			// Big count, bound so a scan (or an applied fix) redraws it.
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
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
		LOCTEXT("SizeTitle", "Project size"),
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SDashboardView::GetSizeSummaryText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
		const FLinearColor Color = FToolsetStyle::ColorForAssetCategory(Entry.Category);
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
// 4. Level scan scope
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
				.Text(LOCTEXT("LevelScopeHint", "Changing scope clears stale results; unchecked levels are skipped by the next scan."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SDashboardView::OnRefreshLevelsClicked)
				[
					SNew(STextBlock).Text(LOCTEXT("RefreshLevels", "Refresh"))
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 10, 0, 0))
		[
			SNew(SBox).HeightOverride(180.0f)
			[
				SAssignNew(LevelTree, STreeView<TSharedPtr<FDashboardLevelItem>>)
				.TreeItemsSource(&LevelRoots)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SDashboardView::GenerateLevelRow)
				.OnGetChildren(this, &SDashboardView::GetLevelChildren)
			]
		];

	TSharedRef<SWidget> Card = MakeCard(LOCTEXT("LevelScopeTitle", "Level scan scope"), Body);

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
		.Padding(FMargin(4, 3))
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
				.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
				.Font(bPersistent ? FCoreStyle::GetDefaultFontStyle("Bold", 10) : FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10, 0, 6, 0))
			[
				SNew(STextBlock)
				.Text(Item.IsValid()
					? FText::Format(LOCTEXT("LevelActorCount", "{0} actors"), FText::AsNumber(Item->ActorCount))
					: FText::GetEmpty())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

#undef LOCTEXT_NAMESPACE
