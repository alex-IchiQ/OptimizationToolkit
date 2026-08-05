// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/ProjectSizeReport.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FToolsetModel;
class ITableRow;
class STableViewBase;
class SVerticalBox;

/** One loaded level in the Dashboard scan-scope tree. */
struct FDashboardLevelItem
{
	FName PackageName;
	FText Label;
	int32 ActorCount = 0;
	bool bPersistentLevel = false;
	TArray<TSharedPtr<FDashboardLevelItem>> Children;
};

/**
 * The level-scale numbers on the Dashboard, in display order.
 *
 * An enum rather than five hand-built cards: the label, the tooltip, the value
 * and the delta all key off it, so a new number is one switch arm per property
 * and no new widget code.
 */
enum class ELevelStat : uint8
{
	Meshes = 0,
	Triangles,
	Actors,
	Materials,
	Lights,
	Count
};

/**
 * Dashboard for the new UI: the landing page that reads top to bottom —
 *
 *   1. Object counts: the scale of the scanned level (meshes, triangles, actors,
 *      materials, lights), with a delta against the previous scan.
 *   2. Found problems: the last scan's severity counts.
 *   3. Project size: where the project's disk footprint goes, one stacked bar
 *      over asset categories (measured on demand under /Game).
 *   4. Settings: the analyze thresholds, grouped by section (a details view onto
 *      the project settings object; edits apply on the next scan).
 *   5. Scanning scope: which loaded levels the next scan analyzes.
 *
 * Counts, findings and scope come from the shared model; the size report is this
 * view's own, recomputed by the shell's single Scan action.
 */
class SDashboardView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDashboardView) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Recomputes the size report and redraws. Driven by the shell's Scan. */
	void Scan();

private:
	// ---- First use ----------------------------------------------------------
	TSharedRef<SWidget> BuildGettingStartedCard();

	// ---- Object counts ------------------------------------------------------
	TSharedRef<SWidget> BuildStatsCard();
	TSharedRef<SWidget> MakeStatCell(ELevelStat Stat);

	static FText LabelForLevelStat(ELevelStat Stat);
	static FText TooltipForLevelStat(ELevelStat Stat);
	static int64 ValueForLevelStat(const FLevelStats& Stats, ELevelStat Stat);

	/** Value now minus value at the previous scan. Meaningless until there are two. */
	int64 DeltaForLevelStat(ELevelStat Stat) const;
	FLinearColor DeltaColorForLevelStat(ELevelStat Stat) const;

	// ---- Found problems -----------------------------------------------------
	TSharedRef<SWidget> BuildFindingsCard();
	TSharedRef<SWidget> MakeSeverityStatCard(ESeverity Severity);

	// ---- Project size -------------------------------------------------------
	TSharedRef<SWidget> BuildSizeCard();
	void RebuildBreakdown();
	FText GetSizeSummaryText() const;

	// ---- Settings -----------------------------------------------------------
	TSharedRef<SWidget> BuildSettingsCard();

	// ---- Level scan scope ---------------------------------------------------
	TSharedRef<SWidget> BuildLevelScopeCard();
	void RefreshLevelTree();
	FReply OnRefreshLevelsClicked();
	TSharedRef<ITableRow> GenerateLevelRow(TSharedPtr<FDashboardLevelItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void GetLevelChildren(TSharedPtr<FDashboardLevelItem> Item, TArray<TSharedPtr<FDashboardLevelItem>>& OutChildren) const;

	/** A generic card wrapper: titled group border, matching the rest of the new UI. */
	TSharedRef<SWidget> MakeCard(const FText& Title, const TSharedRef<SWidget>& Body);

	TSharedPtr<FToolsetModel> Model;

	// Project size.
	FProjectSizeReport SizeReport;
	bool bHasSizeReport = false;
	TSharedPtr<SVerticalBox> BreakdownBox;

	// Level scope.
	TArray<TSharedPtr<FDashboardLevelItem>> LevelRoots;
	TSharedPtr<STreeView<TSharedPtr<FDashboardLevelItem>>> LevelTree;
};
