// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SDashboardPanel.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDashboardPanel"

using namespace ToolsetUI;

void SDashboardPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(FMargin(22, 20))
		[
			SNew(SVerticalBox)

			// Level scale first: what the level *is*, before what is wrong with it.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(6, 0, 6, 14))
			[
				BuildLevelStatsCard()
			]

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

			// Loaded-level scope: the user can exclude noisy or irrelevant sub-levels
			// before running the next scan.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(6, 14, 6, 0))
			[
				BuildLevelScopeCard()
			]
		]
	];
}

// ---------------------------------------------------------------------------
// Level stats
// ---------------------------------------------------------------------------
void SDashboardPanel::RefreshLevelTree()
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

	auto MakeItem = [World](ULevel* Level)
	{
		TSharedPtr<FDashboardLevelItem> Item = MakeShared<FDashboardLevelItem>();
		Item->PackageName = Level->GetOutermost()->GetFName();
		Item->Label = FText::FromString(FPackageName::GetShortName(Item->PackageName.ToString()));
		Item->bPersistentLevel = Level == World->PersistentLevel;
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

FReply SDashboardPanel::OnRefreshLevelsClicked()
{
	RefreshLevelTree();
	return FReply::Handled();
}

void SDashboardPanel::GetLevelChildren(
	TSharedPtr<FDashboardLevelItem> Item,
	TArray<TSharedPtr<FDashboardLevelItem>>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren.Append(Item->Children);
	}
}

TSharedRef<ITableRow> SDashboardPanel::GenerateLevelRow(
	TSharedPtr<FDashboardLevelItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FDashboardLevelItem>>, OwnerTable)
		.Padding(FMargin(4, 3))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 0, 10, 0))
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this, Item]()
				{
					return Model.IsValid() && Item.IsValid()
						&& Model->IsLevelIncluded(Item->PackageName, Item->bPersistentLevel)
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

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Body")
				.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(10, 0, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(Item.IsValid()
					? FText::Format(LOCTEXT("LevelActorCount", "{0} actors"), FText::AsNumber(Item->ActorCount))
					: FText::GetEmpty())
			]
		];
}

TSharedRef<SWidget> SDashboardPanel::BuildLevelScopeCard()
{
	RefreshLevelTree();

	TSharedRef<SBorder> Card = SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(18, 14))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.TextStyle(&S(), "Toolset.Text.Heading")
						.Text(LOCTEXT("LevelScopeTitle", "Level scan scope"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3, 0, 0))
					[
						SNew(STextBlock)
						.TextStyle(&S(), "Toolset.Text.Subtle")
						.Text(LOCTEXT("LevelScopeHint", "Changing scope clears stale results; unchecked levels are skipped by the next scan."))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&S(), "Toolset.Button.Ghost")
					.OnClicked(this, &SDashboardPanel::OnRefreshLevelsClicked)
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body").Text(LOCTEXT("RefreshLevels", "Refresh"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0, 10, 0, 0))
			[
				SNew(SBox)
				.HeightOverride(180.0f)
				[
					SAssignNew(LevelTree, STreeView<TSharedPtr<FDashboardLevelItem>>)
					.TreeItemsSource(&LevelRoots)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow(this, &SDashboardPanel::GenerateLevelRow)
					.OnGetChildren(this, &SDashboardPanel::GetLevelChildren)
				]
			]
		];

	if (LevelTree.IsValid() && !LevelRoots.IsEmpty())
	{
		LevelTree->SetItemExpansion(LevelRoots[0], true);
	}
	return Card;
}

FText SDashboardPanel::LabelForLevelStat(ELevelStat Stat)
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

FText SDashboardPanel::TooltipForLevelStat(ELevelStat Stat)
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

int64 SDashboardPanel::ValueForLevelStat(const FLevelStats& Stats, ELevelStat Stat)
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

int64 SDashboardPanel::DeltaForLevelStat(ELevelStat Stat) const
{
	if (!Model.IsValid() || !Model->HasPreviousStats())
	{
		return 0;
	}
	return ValueForLevelStat(Model->GetLastScan().Stats, Stat) - ValueForLevelStat(Model->GetPreviousStats(), Stat);
}

FLinearColor SDashboardPanel::DeltaColorForLevelStat(ELevelStat Stat) const
{
	// Down is green because every number here is a cost: triangles, draw
	// submissions, actors to tick. This is a hint about direction, not a verdict —
	// deleting the level would turn the whole row green.
	return DeltaForLevelStat(Stat) < 0 ? FToolsetStyle::SeverityGood : FToolsetStyle::SeverityMajor;
}

TSharedRef<SWidget> SDashboardPanel::BuildLevelStatsCard()
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4));
	for (uint8 Index = 0; Index < static_cast<uint8>(ELevelStat::Count); ++Index)
	{
		Grid->AddSlot(Index, 0)
		[
			MakeLevelStatCell(static_cast<ELevelStat>(Index))
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(14, 12))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(4, 0, 4, 10))
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Heading")
				.Text(LOCTEXT("DashScaleTitle", "Level at a glance"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Grid
			]
		];
}

TSharedRef<SWidget> SDashboardPanel::MakeLevelStatCell(ELevelStat Stat)
{
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card.Inner"))
		.Padding(FMargin(12, 10))
		.ToolTipText(TooltipForLevelStat(Stat))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(LabelForLevelStat(Stat))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0, 6, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&S(), "Toolset.Text.StatValue")
				.Text_Lambda([this, Stat]()
				{
					return (Model.IsValid() && Model->HasScanned())
						? FText::AsNumber(ValueForLevelStat(Model->GetLastScan().Stats, Stat))
						: FText::FromString(TEXT("—"));
				})
			]

			// Delta against the previous scan. Hidden until there is one, and when
			// nothing moved — a row of "0" chips would be noise on every rescan.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0, 6, 0, 0))
			[
				SNew(SBorder)
				.BorderImage(Brush("Toolset.Fill.Pill"))
				.Padding(FMargin(7, 2))
				.Visibility_Lambda([this, Stat]()
				{
					return DeltaForLevelStat(Stat) != 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
				// A translucent wash of the delta's own colour, so the pill reads as
				// a tint of the number rather than a second, competing element.
				.BorderBackgroundColor_Lambda([this, Stat]()
				{
					const FLinearColor Color = DeltaColorForLevelStat(Stat);
					return FSlateColor(FLinearColor(Color.R, Color.G, Color.B, 0.16f));
				})
				[
					SNew(STextBlock)
					.TextStyle(&S(), "Toolset.Text.Subtle")
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
			]
		];
}

TSharedRef<SWidget> SDashboardPanel::MakeSeverityStatCard(ESeverity Severity)
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
						SNew(SBorder).BorderImage(Brush("Toolset.Fill.Rounded")).BorderBackgroundColor(FSlateColor(Color))
						[
							SNullWidget::NullWidget
						]
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
					return (Model.IsValid() && Model->HasScanned())
						? FText::AsNumber(Model->GetLastScan().CountBySeverity(Severity))
						: FText::FromString(TEXT("—"));
				})
			]
		];
}

#undef LOCTEXT_NAMESPACE
