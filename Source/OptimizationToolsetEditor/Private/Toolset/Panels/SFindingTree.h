// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

/**
 * One row in a finding tree: either a category header or a finding under one.
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

/** Builds the card a leaf row shows. The only thing Analyze and Optimize differ in. */
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnMakeFindingCard, TSharedPtr<FFinding>);

/**
 * Findings grouped under category headers.
 *
 * Analyze and Optimize were the same tree written twice — same grouping, same
 * headers, same padding, differing only in the card on a leaf. That card is now
 * a delegate, and this owns everything else.
 */
class SFindingTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFindingTree) {}
		SLATE_EVENT(FOnMakeFindingCard, OnMakeCard)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Regroups the given findings and refreshes. Call on every model change. */
	void SetFindings(const TArray<TSharedPtr<FFinding>>& Findings);

private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FFindingNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FFindingNode> Node, TArray<TSharedPtr<FFindingNode>>& OutChildren);
	TSharedRef<SWidget> MakeCategoryHeader(TSharedPtr<FFindingNode> Node);

	/** Groups findings under category headers, in ECategory order. */
	static void BuildCategoryTree(const TArray<TSharedPtr<FFinding>>& Source, TArray<TSharedPtr<FFindingNode>>& OutTree);

	FOnMakeFindingCard OnMakeCard;

	TArray<TSharedPtr<FFindingNode>> Tree;
	TSharedPtr<STreeView<TSharedPtr<FFindingNode>>> TreeView;
};
