// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class SFindingTree;

/**
 * The findings that have a registered, supported fix — grouped the same way
 * Analyze groups them — with a per-row Apply and an Apply all.
 *
 * Deliberately does not inherit Analyze's severity/search toolbar: that toolbar
 * belongs to that panel, and a fix list silently narrowed by a filter set on
 * another screen would hide fixable work.
 */
class SOptimizePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOptimizePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOptimizePanel() override;

private:
	void Refresh();
	TSharedRef<SWidget> MakeFixCard(TSharedPtr<FFinding> Item);

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<SFindingTree> Tree;
	FDelegateHandle ChangedHandle;
};
