// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class SFindingTree;

/** Unified workspace for configuring, reviewing, navigating to, and fixing findings. */
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
	TSharedRef<SWidget> MakeSeverityFilterButton(ESeverity Severity);
	TSharedRef<SWidget> MakeFindingCard(TSharedPtr<FFinding> Item);

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<SFindingTree> Tree;
	FDelegateHandle ChangedHandle;
};
