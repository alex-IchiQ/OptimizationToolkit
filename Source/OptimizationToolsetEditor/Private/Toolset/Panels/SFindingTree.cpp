// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SFindingTree.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SFindingTree"

using namespace ToolsetUI;

namespace
{
	/** Total leaves under a node, so a problem header counts findings, not sub-groups. */
	int32 CountLeaves(const TSharedPtr<FFindingNode>& Node)
	{
		if (!Node.IsValid())
		{
			return 0;
		}
		if (Node->Kind == EFindingNodeKind::Finding)
		{
			return 1;
		}
		int32 Total = 0;
		for (const TSharedPtr<FFindingNode>& Child : Node->Children)
		{
			Total += CountLeaves(Child);
		}
		return Total;
	}

	/** Worst severity anywhere below a node, so a collapsed header keeps its dot. */
	ESeverity WorstSeverity(const TSharedPtr<FFindingNode>& Node)
	{
		if (!Node.IsValid())
		{
			return ESeverity::Good;
		}
		if (Node->Kind == EFindingNodeKind::Finding)
		{
			return Node->Finding.IsValid() ? Node->Finding->Severity : ESeverity::Good;
		}
		ESeverity Worst = ESeverity::Good;
		for (const TSharedPtr<FFindingNode>& Child : Node->Children)
		{
			const ESeverity ChildWorst = WorstSeverity(Child);
			if (static_cast<uint8>(ChildWorst) < static_cast<uint8>(Worst))
			{
				Worst = ChildWorst;
			}
		}
		return Worst;
	}

	/** The level a finding is grouped under: its sub-level, or the persistent level. */
	FText LevelGroupLabel(const FFinding& Finding, const FText& PersistentLabel)
	{
		return Finding.LevelName.IsEmpty() ? PersistentLabel : Finding.LevelName;
	}
}

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

	// Expand the problem headers but leave the level headers folded, so a fresh tree
	// shows every problem type while a busy problem stays a short list of levels to
	// open on demand. Only the top tier is expanded; level nodes keep the tree's
	// default collapsed state. (Re-done every rebuild — the nodes are new objects
	// each time and can't remember their own expansion.)
	for (const TSharedPtr<FFindingNode>& Group : Tree)
	{
		TreeView->SetItemExpansion(Group, true);
	}
}

void SFindingTree::BuildProblemTree(const TArray<TSharedPtr<FFinding>>& Source, TArray<TSharedPtr<FFindingNode>>& OutTree)
{
	OutTree.Reset();

	// The persistent level's name labels findings that carry no sub-level of their
	// own, so its header reads like a real level rather than "(none)".
	FText PersistentLabel = LOCTEXT("PersistentLevel", "Persistent Level");
	if (const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		if (const ULevel* Persistent = World->PersistentLevel)
		{
			PersistentLabel = FText::FromString(FPackageName::GetShortName(Persistent->GetOutermost()->GetName()));
		}
	}

	// First pass: group findings by problem type, collecting leaves flat.
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
			Group->Kind = EFindingNodeKind::Problem;
			Group->TypeId = GroupId;
			Group->GroupTitle = Finding->Title;
			Group->Category = Finding->Category;
		}

		TSharedPtr<FFindingNode> Leaf = MakeShared<FFindingNode>();
		Leaf->Kind = EFindingNodeKind::Finding;
		Leaf->Category = Finding->Category;
		Leaf->Finding = Finding;
		Group->Children.Add(Leaf);
	}

	// Second pass: within each problem group, insert a level tier — but only when
	// the problem actually spans more than one level. One level stays a flat list,
	// which keeps asset- and project-scoped groups (all one "level") uncluttered.
	Groups.GenerateValueArray(OutTree);
	for (const TSharedPtr<FFindingNode>& Group : OutTree)
	{
		TSet<FString> DistinctLevels;
		for (const TSharedPtr<FFindingNode>& Leaf : Group->Children)
		{
			DistinctLevels.Add(LevelGroupLabel(*Leaf->Finding, PersistentLabel).ToString());
		}
		if (DistinctLevels.Num() <= 1)
		{
			continue;	// leave the flat leaves in place
		}

		TArray<TSharedPtr<FFindingNode>> Leaves = MoveTemp(Group->Children);
		Group->Children.Reset();

		TMap<FString, TSharedPtr<FFindingNode>> LevelNodes;
		for (const TSharedPtr<FFindingNode>& Leaf : Leaves)
		{
			const FText Label = LevelGroupLabel(*Leaf->Finding, PersistentLabel);
			TSharedPtr<FFindingNode>& LevelNode = LevelNodes.FindOrAdd(Label.ToString());
			if (!LevelNode.IsValid())
			{
				LevelNode = MakeShared<FFindingNode>();
				LevelNode->Kind = EFindingNodeKind::Level;
				LevelNode->GroupTitle = Label;
				LevelNode->Category = Group->Category;
				Group->Children.Add(LevelNode);
			}
			LevelNode->Children.Add(Leaf);
		}

		// Levels alphabetical, so the sub-headers hold still between scans.
		Group->Children.Sort([](const TSharedPtr<FFindingNode>& A, const TSharedPtr<FFindingNode>& B)
		{
			return A->GroupTitle.CompareTo(B->GroupTitle) < 0;
		});
	}

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

TSharedRef<SWidget> SFindingTree::MakeProblemHeader(TSharedPtr<FFindingNode> Node)
{
	// The worst thing anywhere in the group decides its colour, so a collapsed group
	// still says whether it is worth opening.
	const FLinearColor WorstColor = FToolsetStyle::ColorForSeverity(WorstSeverity(Node));

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
				.Text(FText::AsNumber(CountLeaves(Node)))
			]
		];
}

TSharedRef<SWidget> SFindingTree::MakeLevelHeader(TSharedPtr<FFindingNode> Node)
{
	const FLinearColor WorstColor = FToolsetStyle::ColorForSeverity(WorstSeverity(Node));

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 8, 0))
		[
			SNew(SBox).WidthOverride(6).HeightOverride(6)
			[
				SNew(SBorder).BorderImage(Brush("Toolset.Fill.Rounded")).BorderBackgroundColor(FSlateColor(WorstColor))
				[
					SNullWidget::NullWidget
				]
			]
		]

		// The level name in accent, matching the level pill on each card below it.
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Body")
			.Text(Node->GroupTitle)
			.ColorAndOpacity(FSlateColor(FToolsetStyle::Accent))
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
		[
			SNew(SBorder).BorderImage(Brush("Toolset.Pill")).Padding(FMargin(7, 1))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle")
				.Text(FText::AsNumber(CountLeaves(Node)))
			]
		];
}

TSharedRef<ITableRow> SFindingTree::OnGenerateRow(TSharedPtr<FFindingNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Node->Kind == EFindingNodeKind::Problem)
	{
		// No SExpanderArrow of our own: STableRow::ConstructChildren already builds
		// one for a tree, and SetContent *preserves* it (SetRowContent is the one
		// that replaces it) — adding a second arrow drew two.
		return SNew(STableRow<TSharedPtr<FFindingNode>>, OwnerTable)
			.Style(&S(), "Toolset.TableRow")
			.ShowSelection(false)
			.Padding(FMargin(6, 10, 6, 2))
			[
				MakeProblemHeader(Node)
			];
	}

	if (Node->Kind == EFindingNodeKind::Level)
	{
		return SNew(STableRow<TSharedPtr<FFindingNode>>, OwnerTable)
			.Style(&S(), "Toolset.TableRow")
			.ShowSelection(false)
			.Padding(FMargin(6, 4, 6, 2))
			[
				MakeLevelHeader(Node)
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
