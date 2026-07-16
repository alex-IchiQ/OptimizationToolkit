// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

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
	TSharedRef<ITableRow> OnGenerateFindingRow(TSharedPtr<FFinding> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSearchChanged(const FText& NewText);
	FReply OnToggleSeverityFilter(ESeverity Severity);
	bool PassesFilter(const FFinding& F) const;

	// ---- Optimize / fixes ---------------------------------------------------
	bool HasSupportedFix(const FFinding& F) const;
	void ApplyFix(TSharedPtr<FFinding> Finding);
	FReply OnApplyAllFixes();
	TSharedRef<ITableRow> OnGenerateFixRow(TSharedPtr<FFinding> Item, const TSharedRef<STableViewBase>& OwnerTable);

	// ---- Cleanup ------------------------------------------------------------
	TSharedRef<SWidget> MakeCleanupActionCard(const ICleanupAction& Action);
	FReply OnRunCleanupAction(const ICleanupAction* Action);

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

	// Analyze list.
	TArray<TSharedPtr<FFinding>> AllFindings;
	TArray<TSharedPtr<FFinding>> VisibleFindings;
	TSharedPtr<SListView<TSharedPtr<FFinding>>> FindingsListView;
	TSharedPtr<SSearchBox> SearchBox;

	// Optimize list: findings that have a registered, supported fix.
	TArray<TSharedPtr<FFinding>> FixableFindings;
	TSharedPtr<SListView<TSharedPtr<FFinding>>> FixListView;

	// Filters.
	FString SearchFilter;
	TSet<ESeverity> EnabledSeverities = { ESeverity::Critical, ESeverity::Major, ESeverity::Minor };

	// Cleanup: last run summary per action id, shown on its card.
	TMap<FName, FText> CleanupResults;
};
