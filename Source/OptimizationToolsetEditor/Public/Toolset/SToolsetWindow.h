// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class SWidgetSwitcher;

/** Top-level sections shown in the left sidebar. */
enum class EToolsetSection : uint8
{
	Dashboard = 0,
	Optimize,
	Profile,
	Cleanup,
	Reports,
	Count
};

/**
 * The window's chrome: a left navigation rail with the scan action, a header
 * with the live health score, and a switched content area.
 *
 * Deliberately owns no panel content and no scan state. It builds the panels,
 * hands each the shared FToolsetModel, and otherwise only decides which one is
 * on screen — everything a panel shows, it reads from the model itself.
 */
class SToolsetWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SToolsetWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ---- Layout -------------------------------------------------------------
	TSharedRef<SWidget> BuildSidebar();
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildContent();

	/** The chunky square scan block at the top of the sidebar. */
	TSharedRef<SWidget> BuildScanButton();

	// ---- Navigation ---------------------------------------------------------
	TSharedRef<SWidget> BuildNavItem(EToolsetSection Section, const FText& Label, const FName& IconName);
	void SelectSection(EToolsetSection Section);
	bool IsSectionSelected(EToolsetSection Section) const { return CurrentSection == Section; }
	FReply OnNavItemClicked(EToolsetSection Section);

	/** Optimize lists its categories underneath; the rest don't. */
	static bool SectionHasCategories(EToolsetSection Section);
	bool IsNavExpanded(EToolsetSection Section) const;

	/** The category sub-items under Optimize. */
	TSharedRef<SWidget> BuildNavCategoryList(EToolsetSection Section);
	TSharedRef<SWidget> BuildNavSubItem(EToolsetSection Section, ECategory Category);
	void SelectSectionCategory(EToolsetSection Section, ECategory Category);
	bool IsNavCategorySelected(EToolsetSection Section, ECategory Category) const;
	int32 CountForNavCategory(ECategory Category) const;

	// ---- Scan ---------------------------------------------------------------
	FReply OnScanClicked();

	// ---- Bound getters (drive live header text) -----------------------------
	FText GetHeaderTitle() const;
	FText GetScoreText() const;
	FSlateColor GetScoreColor() const;
	TOptional<float> GetScorePercent() const;
	FText GetSummaryText() const;

private:
	/** The one thing every panel shares. Created here, handed to each of them. */
	TSharedPtr<FToolsetModel> Model;

	EToolsetSection CurrentSection = EToolsetSection::Dashboard;
	TSharedPtr<SWidgetSwitcher> ContentSwitcher;

	/** Which sections currently have their category list unfolded. */
	TSet<EToolsetSection> ExpandedNavSections;
};
