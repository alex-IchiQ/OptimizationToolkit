// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/ProjectSizeReport.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * Dashboard for the new UI: where the project's disk footprint goes, as one
 * stacked bar over asset categories plus a legend — the same breakdown the old
 * Cleanup panel showed, now the landing page.
 *
 * Measured on demand (it walks every package under /Game), driven by the shell's
 * single Scan action.
 */
class SDashboardView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDashboardView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Recomputes the size report and redraws. Driven by the shell's Scan. */
	void Scan();

private:
	void RebuildBreakdown();
	FText GetSummaryText() const;

	FProjectSizeReport SizeReport;
	bool bHasReport = false;
	TSharedPtr<SVerticalBox> BreakdownBox;
};
