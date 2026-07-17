// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class SFindingTree;

/**
 * Every finding, grouped by category, under a severity + search toolbar.
 *
 * The toolbar writes to the model rather than to local fields, because the nav's
 * category badges have to count what these filters would leave — a badge
 * promising rows the panel then hides is worse than no badge.
 */
class SAnalyzePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnalyzePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAnalyzePanel() override;

private:
	/** Pushes the model's visible findings into the tree. Bound to OnChanged. */
	void Refresh();

	TSharedRef<SWidget> MakeSeverityFilterButton(ESeverity Severity);
	TSharedRef<SWidget> MakeFindingCard(TSharedPtr<FFinding> Item);

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<SFindingTree> Tree;
	FDelegateHandle ChangedHandle;
};
