// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SFindingTree.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Styling/SlateTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SFindingTree"

using namespace ToolsetUI;

void SFindingTree::Construct(const FArguments& InArgs)
{
	OnMakeCard = InArgs._OnMakeCard;

	ChildSlot
	[
		SAssignNew(TreeView, STreeView<TSharedPtr<FFindingNode>>)
		.TreeViewStyle(&S().GetWidgetStyle<FTableViewStyle>("Toolset.TreeView"))
		.TreeItemsSource(&Tree)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SFindingTree::OnGenerateRow)
		.OnGetChildren(this, &SFindingTree::OnGetChildren)
	];
}

void SFindingTree::SetFindings(const TArray<TSharedPtr<FFinding>>& Findings)
{
	BuildProblemTree(Findings, Tree);

	if (!TreeView.IsValid())
	{
		return;
	}

	TreeView->RequestTreeRefresh();

	// Re-expand every time: the tree is rebuilt from scratch, so the previous
	// expansion state refers to nodes that no longer exist.
	for (const TSharedPtr<FFindingNode>& Group : Tree)
	{
		TreeView->SetItemExpansion(Group, true);
	}
}

void SFindingTree::BuildProblemTree(const TArray<TSharedPtr<FFinding>>& Source, TArray<TSharedPtr<FFindingNode>>& OutTree)
{
	OutTree.Reset();

	TMap<FName, TSharedPtr<FFindingNode>> Groups;
	for (const TSharedPtr<FFinding>& Finding : Source)
	{
		if (!Finding.IsValid())
		{
			continue;
		}

		const FName GroupId = Finding->TypeId.IsNone() ? FName(*Finding->Title.ToString()) : Finding->TypeId;
		TSharedPtr<FFindingNode>& Group = Groups.FindOrAdd(GroupId);
		if (!Group.IsValid())
		{
			Group = MakeShared<FFindingNode>();
			Group->TypeId = GroupId;
			Group->GroupTitle = Finding->Title;
			Group->Category = Finding->Category;
		}

		TSharedPtr<FFindingNode> Leaf = MakeShared<FFindingNode>();
		Leaf->Category = Finding->Category;
		Leaf->Finding = Finding;
		Group->Children.Add(Leaf);
	}

	Groups.GenerateValueArray(OutTree);

	// Keep categories stable in the all-results view, then sort problem types by
	// their display title. Findings retain the analyzer's severity order.
	OutTree.Sort([](const TSharedPtr<FFindingNode>& A, const TSharedPtr<FFindingNode>& B)
	{
		if (A->Category != B->Category)
		{
			return static_cast<uint8>(A->Category) < static_cast<uint8>(B->Category);
		}
		return A->GroupTitle.CompareTo(B->GroupTitle) < 0;
	});
}

void SFindingTree::OnGetChildren(TSharedPtr<FFindingNode> Node, TArray<TSharedPtr<FFindingNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

TSharedRef<SWidget> SFindingTree::MakeGroupHeader(TSharedPtr<FFindingNode> Node)
{
	// The worst thing in the group decides its colour, so a collapsed group still
	// says whether it is worth opening.
	ESeverity Worst = ESeverity::Good;
	for (const TSharedPtr<FFindingNode>& Child : Node->Children)
	{
		if (Child->Finding.IsValid() && static_cast<uint8>(Child->Finding->Severity) < static_cast<uint8>(Worst))
		{
			Worst = Child->Finding->Severity;
		}
	}
	const FLinearColor WorstColor = FToolsetStyle::ColorForSeverity(Worst);

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 8, 0))
		[
			SNew(SBox).WidthOverride(7).HeightOverride(7)
			[
				SNew(SBorder).BorderImage(Brush("Toolset.Fill.Rounded")).BorderBackgroundColor(FSlateColor(WorstColor))
				[
					SNullWidget::NullWidget
				]
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading")
			.Text(Node->GroupTitle)
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
		[
			SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(7, 1))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(FToolsetStyle::LabelForCategory(Node->Category))
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
		[
			SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(7, 1))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(FText::AsNumber(Node->Children.Num()))
			]
		];
}

TSharedRef<ITableRow> SFindingTree::OnGenerateRow(TSharedPtr<FFindingNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Node->IsGroup())
	{
		// No SExpanderArrow of our own: STableRow::ConstructChildren already builds
		// one for a tree, and SetContent *preserves* it (SetRowContent is the one
		// that replaces it) — adding a second arrow drew two.
		return SNew(STableRow<TSharedPtr<FFindingNode>>, OwnerTable)
			.Style(&S(), "Toolset.TableRow")
			.ShowSelection(false)
			.Padding(FMargin(6, 10, 6, 2))
			[
				MakeGroupHeader(Node)
			];
	}

	return SNew(STableRow<TSharedPtr<FFindingNode>>, OwnerTable)
		.Style(&S(), "Toolset.TableRow")
		.ShowSelection(false)
		.Padding(FMargin(6, 5))
		[
			OnMakeCard.IsBound() ? OnMakeCard.Execute(Node->Finding) : SNullWidget::NullWidget
		];
}

#undef LOCTEXT_NAMESPACE
