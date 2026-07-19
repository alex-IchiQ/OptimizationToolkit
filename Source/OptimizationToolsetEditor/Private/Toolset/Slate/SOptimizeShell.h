// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

/** Left-nav sections of the new UI. Dashboard is a placeholder for now. */
enum class EOptimizeSection : uint8
{
	Dashboard,
	Optimize,
	Profile,
	Cleanup,
	Count
};

/**
 * Root of the new UI: a classic editor left-nav rail over a switched content area.
 *
 * Deliberately plain (default editor styling, no toolset islands) and separate
 * from the existing SToolsetWindow, so the redesign can be built and compared in
 * isolation. Only Optimize is filled in; Dashboard is an empty section.
 */
class SOptimizeShell : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOptimizeShell) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildNav();
	TSharedRef<SWidget> MakeNavButton(EOptimizeSection Section, const FText& Label);
	TSharedRef<SWidget> BuildDashboardPlaceholder();

	void SelectSection(EOptimizeSection Section);
	bool IsSelected(EOptimizeSection Section) const { return Current == Section; }

	EOptimizeSection Current = EOptimizeSection::Optimize;
	TSharedPtr<SWidgetSwitcher> Switcher;
};
