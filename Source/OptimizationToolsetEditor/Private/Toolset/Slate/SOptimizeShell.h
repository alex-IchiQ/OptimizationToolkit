// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class SWidgetSwitcher;
class SDashboardView;
class SOptimizeView;
class SAnalyzerView;
class SCleanupView;

/** Left-nav sections, in tab order. */
enum class EOptimizeSection : uint8
{
	Dashboard,
	Optimize,
	Analyzer,
	Profile,
	Cleanup,
	Count
};

/**
 * Root of the toolset UI: a classic editor left-nav rail over a switched content
 * area, in plain default editor styling.
 *
 * Owns the one shared model and drives the single Scan that feeds every page.
 */
class SOptimizeShell : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOptimizeShell) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildNav();
	TSharedRef<SWidget> MakeNavButton(EOptimizeSection Section, const FText& Label, const FName& IconName);

	void SelectSection(EOptimizeSection Section);
	bool IsSelected(EOptimizeSection Section) const { return Current == Section; }

	/** One scan for the whole toolset: level analysis and the project sweep. */
	FReply OnScanClicked();

	EOptimizeSection Current = EOptimizeSection::Optimize;

	/**
	 * The one model the whole new UI shares. Dashboard and Optimize both read the
	 * same scan, and the Dashboard's level-scope toggles drive the next RunScan —
	 * so it has to be the same instance, not one per view.
	 */
	TSharedPtr<FToolsetModel> Model;

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SDashboardView> DashboardView;
	TSharedPtr<SOptimizeView> OptimizeView;
	TSharedPtr<SAnalyzerView> AnalyzerView;
	TSharedPtr<SCleanupView> CleanupView;
};
