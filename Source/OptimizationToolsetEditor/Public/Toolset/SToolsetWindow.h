// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/Cleanup/ProjectSizeReport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class SWidgetSwitcher;
class SSearchBox;
class ICleanupAction;

/** Top-level sections shown in the left sidebar. */
enum class EToolsetSection : uint8
{
	Dashboard = 0,
	Analyze,
	Optimize,
	Profile,
	Cleanup,
	Reports,
	Count
};

/**
 * One row in the Analyze / Optimize trees: either a category header or a finding
 * sitting under one.
 *
 * With nine passes reporting, a flat list buries a critical mesh problem among
 * thirty texture notes. Grouping by ECategory means a user can collapse what they
 * are not working on right now.
 */
struct FFindingNode
{
	ECategory Category = ECategory::Meshes;

	/** Null on a category header; set on a leaf. */
	TSharedPtr<FFinding> Finding;

	/** Only populated on a category header. */
	TArray<TSharedPtr<FFindingNode>> Children;

	bool IsCategory() const { return !Finding.IsValid(); }
};

/**
 * The main dockable panel: a left navigation rail, a header with the scan
 * action and live health score, and a switched content area. Analyze and
 * Profile are functional; the remaining sections are structured placeholders
 * ready to be filled in.
 */
class SToolsetWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SToolsetWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ---- Layout builders ----------------------------------------------------
	TSharedRef<SWidget> BuildSidebar();
	TSharedRef<SWidget> BuildNavItem(EToolsetSection Section, const FText& Label, const FName& IconName);
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildContent();

	TSharedRef<SWidget> BuildDashboardPanel();
	TSharedRef<SWidget> BuildAnalyzePanel();
	TSharedRef<SWidget> BuildOptimizePanel();
	TSharedRef<SWidget> BuildProfilePanel();
	TSharedRef<SWidget> BuildCleanupPanel();
	TSharedRef<SWidget> BuildPlaceholderPanel(const FText& Title, const FText& Body, const TArray<FText>& PlannedActions);

	// ---- Small reusable pieces ---------------------------------------------
	TSharedRef<SWidget> MakeSeverityStatCard(ESeverity Severity);
	TSharedRef<SWidget> MakeStatCommandButton(const FText& Label, const FString& ConsoleCommand);

	// ---- Navigation ---------------------------------------------------------
	void SelectSection(EToolsetSection Section);
	bool IsSectionSelected(EToolsetSection Section) const { return CurrentSection == Section; }

	// ---- Scan / filtering ---------------------------------------------------
	FReply OnScanClicked();
	void RunScan();
	void RebuildVisibleFindings();
	void OnSearchChanged(const FText& NewText);
	FReply OnToggleSeverityFilter(ESeverity Severity);
	bool PassesFilter(const FFinding& F) const;

	// ---- Grouped trees ------------------------------------------------------
	/** Groups findings under category headers, in ECategory order. */
	static void BuildCategoryTree(const TArray<TSharedPtr<FFinding>>& Source, TArray<TSharedPtr<FFindingNode>>& OutTree);
	void OnGetNodeChildren(TSharedPtr<FFindingNode> Node, TArray<TSharedPtr<FFindingNode>>& OutChildren);
	TSharedRef<SWidget> MakeCategoryHeader(TSharedPtr<FFindingNode> Node);
	TSharedRef<ITableRow> OnGenerateFindingRow(TSharedPtr<FFindingNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateFixRow(TSharedPtr<FFindingNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> MakeFindingCard(TSharedPtr<FFinding> Item);
	TSharedRef<SWidget> MakeFixCard(TSharedPtr<FFinding> Item);

	// ---- Optimize / fixes ---------------------------------------------------
	bool HasSupportedFix(const FFinding& F) const;
	void ApplyFix(TSharedPtr<FFinding> Finding);
	FReply OnApplyAllFixes();

	// ---- Cleanup ------------------------------------------------------------
	TSharedRef<SWidget> MakeCleanupActionCard(const ICleanupAction& Action);
	FReply OnRunCleanupAction(const ICleanupAction* Action);
	TSharedRef<SWidget> BuildProjectSizeCard();
	FReply OnComputeProjectSize();
	void RebuildSizeBreakdown();

	// ---- Bound getters (drive live text) -----------------------------------
	FText GetHeaderTitle() const;
	FText GetScoreText() const;
	FSlateColor GetScoreColor() const;
	TOptional<float> GetScorePercent() const;
	FText GetSummaryText() const;

private:
	EToolsetSection CurrentSection = EToolsetSection::Dashboard;
	TSharedPtr<SWidgetSwitcher> ContentSwitcher;

	// Scan state.
	FScanResult LastScan;
	bool bHasScanned = false;

	// Analyze: flat results, plus the same grouped under category headers.
	TArray<TSharedPtr<FFinding>> AllFindings;
	TArray<TSharedPtr<FFinding>> VisibleFindings;
	TArray<TSharedPtr<FFindingNode>> VisibleTree;
	TSharedPtr<STreeView<TSharedPtr<FFindingNode>>> FindingsTreeView;
	TSharedPtr<SSearchBox> SearchBox;

	// Optimize: findings that have a registered, supported fix, grouped the same way.
	TArray<TSharedPtr<FFinding>> FixableFindings;
	TArray<TSharedPtr<FFindingNode>> FixableTree;
	TSharedPtr<STreeView<TSharedPtr<FFindingNode>>> FixTreeView;

	// Filters.
	FString SearchFilter;
	TSet<ESeverity> EnabledSeverities = { ESeverity::Critical, ESeverity::Major, ESeverity::Minor };

	// Cleanup: last run summary per action id, shown on its card.
	TMap<FName, FText> CleanupResults;

	// Cleanup: project size breakdown, computed on demand (it walks every package).
	FProjectSizeReport SizeReport;
	bool bHasSizeReport = false;
	TSharedPtr<SVerticalBox> SizeBreakdownBox;
};
