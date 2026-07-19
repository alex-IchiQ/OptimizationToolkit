// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

/** What a tree row is: a problem-type header, a level header under one, or a leaf. */
enum class EFindingNodeKind : uint8
{
	Problem,
	Level,
	Finding,
};

/**
 * One row in a finding tree.
 *
 * With nine passes reporting, a flat list buries a critical mesh problem among
 * thirty texture notes. The top tier groups by stable problem type. When one
 * problem repeats across several loaded sub-levels — 79 over-budget lights spread
 * across a streamed map — a middle tier groups those by level, so a whole level's
 * worth of one problem folds away in a click. That tier only appears when it earns
 * its keep: a problem confined to one level stays a flat list.
 */
struct FFindingNode
{
	EFindingNodeKind Kind = EFindingNodeKind::Problem;

	FName TypeId = NAME_None;

	/** Problem title on a problem header; level name on a level header. */
	FText GroupTitle;
	ECategory Category = ECategory::Meshes;

	/** Null on a header; set on a leaf. */
	TSharedPtr<FFinding> Finding;

	/** Child rows on a header. */
	TArray<TSharedPtr<FFindingNode>> Children;

	bool IsGroup() const { return Kind != EFindingNodeKind::Finding; }
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
	TSharedRef<SWidget> MakeProblemHeader(TSharedPtr<FFindingNode> Node);
	TSharedRef<SWidget> MakeLevelHeader(TSharedPtr<FFindingNode> Node);

	/** Groups findings by problem type, then by level within a type when it spans several. */
	static void BuildProblemTree(const TArray<TSharedPtr<FFinding>>& Source, TArray<TSharedPtr<FFindingNode>>& OutTree);

	FOnMakeFindingCard OnMakeCard;

	TArray<TSharedPtr<FFindingNode>> Tree;
	TSharedPtr<STreeView<TSharedPtr<FFindingNode>>> TreeView;
};
