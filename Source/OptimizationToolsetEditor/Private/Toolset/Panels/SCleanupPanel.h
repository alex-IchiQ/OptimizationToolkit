// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/ProjectSizeReport.h"
#include "Widgets/SCompoundWidget.h"

class ICleanupAction;
class SVerticalBox;

/**
 * Project-wide hygiene: a size breakdown plus the registry's cleanup actions.
 *
 * Takes no model — nothing here is about the current level, and nothing here is
 * Undo-able, which is exactly why it is kept apart from Optimize.
 */
class SCleanupPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCleanupPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildProjectSizeCard();
	TSharedRef<SWidget> MakeCleanupActionCard(const ICleanupAction& Action);
	FReply OnComputeProjectSize();
	FReply OnRunCleanupAction(const ICleanupAction* Action);
	void RebuildSizeBreakdown();

	/** Last run summary per action id, shown on that action's card. */
	TMap<FName, FText> CleanupResults;

	/** Computed on demand: it walks every package under /Game. */
	FProjectSizeReport SizeReport;
	bool bHasSizeReport = false;
	TSharedPtr<SVerticalBox> SizeBreakdownBox;
};
