// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

/**
 * One row in a finding tree: either a problem-type header or a finding under one.
 *
 * With nine passes reporting, a flat list buries a critical mesh problem among
 * thirty texture notes. Grouping by ECategory means a user can collapse what they
 * are not working on right now.
 */
struct FFindingNode
{
	FName TypeId = NAME_None;
	FText GroupTitle;
	ECategory Category = ECategory::Meshes;

	/** Null on a problem-type header; set on a leaf. */
	TSharedPtr<FFinding> Finding;

	/** Only populated on a problem-type header. */
	TArray<TSharedPtr<FFindingNode>> Children;

	bool IsGroup() const { return !Finding.IsValid(); }
};

/** Builds the card a leaf row shows. */
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnMakeFindingCard, TSharedPtr<FFinding>);

/**
 * Findings grouped by their stable TypeId. Category navigation narrows the
 * workspace; grouping then answers which concrete problem repeats inside it.
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
	TSharedRef<SWidget> MakeGroupHeader(TSharedPtr<FFindingNode> Node);

	/** Groups findings under stable problem type ids. */
	static void BuildProblemTree(const TArray<TSharedPtr<FFinding>>& Source, TArray<TSharedPtr<FFindingNode>>& OutTree);

	FOnMakeFindingCard OnMakeCard;

	TArray<TSharedPtr<FFindingNode>> Tree;
	TSharedPtr<STreeView<TSharedPtr<FFindingNode>>> TreeView;
};
