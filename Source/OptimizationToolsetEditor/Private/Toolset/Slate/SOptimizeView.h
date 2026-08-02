// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SListView.h"

class FToolsetModel;
class SHeaderRow;
class AActor;

/**
 * A row in the Affected-assets tree: a finding, or one member of a group finding.
 *
 * Most findings are a single row. A group finding — repeated actors an ISM/HISM
 * would collapse — is one parent row (the group, with a component count) that
 * expands into a child row per member actor, so the individual meshes are visible
 * without leaving the list.
 */
struct FAffectedNode
{
	/** The owning finding: carries severity, the fix, and navigation. */
	TSharedPtr<FFinding> Finding;

	/** Set only on a group child; the single actor this row stands for. */
	TWeakObjectPtr<AActor> MemberActor;

	TArray<TSharedPtr<FAffectedNode>> Children;

	bool IsMember() const { return MemberActor.IsValid(); }
};

/**
 * One column in the Affected-assets table.
 *
 * The set of columns depends on what the selected problem is about — a texture
 * problem shows resolution/format/mips, a mesh problem shows tris/verts/LODs — so
 * the columns are data, chosen per problem, with each value read from the row's
 * target (a finding, or a group member's actor).
 */
struct FAssetColumn
{
	FName Id;
	FText Header;
	float FillWidth = 1.0f;
	EHorizontalAlignment HAlign = HAlign_Left;
	TFunction<FText(const FAffectedNode&)> GetText;
};

/**
 * A row in the Optimize tree: a level, or a problem type under it.
 *
 * The tree is intentionally shallow — level then problem type — with the actual
 * offending assets living in the Affected list beside it, not as a third tier.
 */
enum class EOptimizeNodeKind : uint8
{
	Level,
	Problem,
};

struct FOptimizeNode
{
	EOptimizeNodeKind Kind = EOptimizeNodeKind::Level;

	FText Label;
	FName TypeId;
	ESeverity Worst = ESeverity::Good;

	/** Tooltip text on a problem node. */
	FText Why;
	FText How;

	/** Findings on a problem node; child levels/problems otherwise. */
	TArray<TSharedPtr<FFinding>> Findings;
	TArray<TSharedPtr<FOptimizeNode>> Children;

	int32 LeafCount() const;
};

/**
 * The Optimize page: a deliberately default-styled level workspace.
 *
 * Vertical master-detail: a level -> problem-type tree on top, a checkable list of
 * the affected assets for the selected problem below it, and a single Apply button
 * at the bottom that applies the ticked fixes and rescans once afterward.
 *
 * Reads and fixes through the shared model handed in by the shell, so a scan or a
 * fix here redraws every other page too.
 */
class SOptimizeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOptimizeView) {}
		/** The shared model. Falls back to a private one if none is supplied. */
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOptimizeView() override;

	/** Re-analyzes the level. Driven by the shell's single Scan action. */
	void Scan();

private:
	// ---- Data ---------------------------------------------------------------
	void Refresh();
	void RebuildTree();

	// ---- Tree ---------------------------------------------------------------
	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FOptimizeNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> MakeProblemCard(const TSharedPtr<FOptimizeNode>& Node);
	void GetTreeChildren(TSharedPtr<FOptimizeNode> Node, TArray<TSharedPtr<FOptimizeNode>>& OutChildren);
	void OnTreeSelectionChanged(TSharedPtr<FOptimizeNode> Node, ESelectInfo::Type SelectInfo);

	// ---- Affected tree ------------------------------------------------------
	void SelectProblem(TSharedPtr<FOptimizeNode> Problem);
	TSharedRef<ITableRow> GenerateAffectedRow(TSharedPtr<FAffectedNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void GetAffectedChildren(TSharedPtr<FAffectedNode> Node, TArray<TSharedPtr<FAffectedNode>>& OutChildren);

	/** Chooses the column set for a problem and rebuilds the header. */
	void BuildColumns(const TSharedPtr<FOptimizeNode>& Problem);

	/** Builds one cell; called by each row for each column id in the header. */
	TSharedRef<SWidget> GenerateCell(const FName& ColumnId, TSharedPtr<FAffectedNode> Node);

	ECheckBoxState GetItemCheck(TSharedPtr<FAffectedNode> Node) const;
	void OnItemCheckChanged(ECheckBoxState State, TSharedPtr<FAffectedNode> Node);

	// ---- Actions ------------------------------------------------------------
	FReply OnApplyClicked();
	bool IsApplyEnabled() const;
	int32 CheckedFixableCount() const;
	FText GetSummaryText() const;
	FText GetAffectedHeaderText() const;

	TSharedPtr<FToolsetModel> Model;
	FDelegateHandle ChangedHandle;

	TArray<TSharedPtr<FOptimizeNode>> TreeRoots;
	TSharedPtr<STreeView<TSharedPtr<FOptimizeNode>>> TreeView;

	TSharedPtr<FOptimizeNode> SelectedProblem;
	TArray<TSharedPtr<FAffectedNode>> AffectedItems;
	TSharedPtr<STreeView<TSharedPtr<FAffectedNode>>> AffectedList;

	/** The current column schema and the header it drives. */
	TArray<FAssetColumn> Columns;
	TSharedPtr<SHeaderRow> HeaderRow;

	/**
	 * The ticked selectable rows: non-group findings, and individual group members.
	 * A group parent's state is derived from its members, so it is never stored here.
	 */
	TSet<TSharedPtr<FAffectedNode>> CheckedNodes;
};
