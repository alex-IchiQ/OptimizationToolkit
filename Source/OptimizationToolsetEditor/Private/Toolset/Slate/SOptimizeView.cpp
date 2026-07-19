// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SOptimizeView.h"
#include "Toolset/Analyzer/LevelAnalyzer.h"
#include "Toolset/Navigation/FindingNavigator.h"
#include "Toolset/ToolsetModel.h"
#include "Toolset/ToolsetStyle.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "PixelFormat.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SOptimizeView"

// Column ids shared between the header, the schema, and the fixed check/nav columns.
namespace OptimizeColumns
{
	static const FName Check("Check");
	static const FName Object("Object");
	static const FName Nav("Nav");
}

namespace
{
	const FText Dash = FText::FromString(TEXT("—"));

	const UStaticMesh* MeshFromActor(const AActor* Actor)
	{
		if (const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
		{
			if (const UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent())
			{
				return Component->GetStaticMesh();
			}
		}
		return nullptr;
	}

	/** The static mesh a finding is about, whether it points at the asset or an actor. */
	const UStaticMesh* MeshFromFinding(const FFinding& Finding)
	{
		if (const UStaticMesh* Mesh = Cast<UStaticMesh>(Finding.TargetAsset.Get()))
		{
			return Mesh;
		}
		return MeshFromActor(Finding.TargetActor.Get());
	}

	// ---- Node resolvers: a row is either a finding or one group member actor ----

	/** A finding row that fans out into member actors — the group's asset, not an actor. */
	bool NodeIsGroupParent(const FAffectedNode& Node)
	{
		return !Node.IsMember() && Node.Children.Num() > 0;
	}

	const UStaticMesh* MeshFromNode(const FAffectedNode& Node)
	{
		if (Node.IsMember())
		{
			return MeshFromActor(Node.MemberActor.Get());
		}
		return Node.Finding.IsValid() ? MeshFromFinding(*Node.Finding) : nullptr;
	}

	const UTexture2D* TextureFromNode(const FAffectedNode& Node)
	{
		// Only findings target textures; a group member is always an actor.
		return Node.Finding.IsValid() ? Cast<UTexture2D>(Node.Finding->TargetAsset.Get()) : nullptr;
	}

	FText NodeName(const FAffectedNode& Node)
	{
		if (Node.IsMember())
		{
			const AActor* Actor = Node.MemberActor.Get();
			return Actor ? FText::FromString(Actor->GetActorNameOrLabel()) : Dash;
		}
		if (!Node.Finding.IsValid())
		{
			return Dash;
		}
		// A group parent stands for the shared mesh, not its representative actor.
		if (NodeIsGroupParent(Node))
		{
			const UStaticMesh* Mesh = MeshFromFinding(*Node.Finding);
			return Mesh ? FText::FromString(Mesh->GetName()) : Node.Finding->Subject;
		}
		if (const UObject* Asset = Node.Finding->TargetAsset.Get())
		{
			return FText::FromString(Asset->GetName());
		}
		if (const AActor* Actor = Node.Finding->TargetActor.Get())
		{
			return FText::FromString(Actor->GetActorNameOrLabel());
		}
		return Node.Finding->Subject;
	}

	FText NodeType(const FAffectedNode& Node)
	{
		// The parent is the asset the members share; the members are the actors.
		if (NodeIsGroupParent(Node))
		{
			return LOCTEXT("KindStaticMesh", "Static Mesh");
		}
		const AActor* Actor = Node.IsMember() ? Node.MemberActor.Get()
			: (Node.Finding.IsValid() ? Node.Finding->TargetActor.Get() : nullptr);
		if (Actor)
		{
			return FText::FromString(Actor->GetClass()->GetName());
		}
		if (TextureFromNode(Node))
		{
			return LOCTEXT("KindTexture", "Texture");
		}
		if (MeshFromNode(Node))
		{
			return LOCTEXT("KindStaticMesh", "Static Mesh");
		}
		return Dash;
	}

	FText TextureStreamState(const UTexture2D& Texture)
	{
		if (Texture.VirtualTextureStreaming)
		{
			return LOCTEXT("StreamVT", "Virtual");
		}
		return Texture.NeverStream ? LOCTEXT("StreamNever", "NeverStream") : LOCTEXT("StreamOn", "Stream");
	}

	/** For a group parent: how many actors it would collapse. Blank on a member. */
	FAssetColumn ComponentsColumn()
	{
		return { FName("Components"), LOCTEXT("ColComponents", "Components"), 1.3f, HAlign_Center,
			[](const FAffectedNode& N)
			{
				if (N.IsMember() || !N.Finding.IsValid())
				{
					return Dash;
				}
				const int32 Count = N.Finding->RelatedActors.Num();
				return Count > 0 ? FText::AsNumber(Count) : Dash;
			} };
	}

	TArray<FAssetColumn> MakeMeshColumns()
	{
		return {
			{ OptimizeColumns::Object, LOCTEXT("ColObject", "Object"), 3.0f, HAlign_Left, &NodeName },
			{ FName("Type"), LOCTEXT("ColType", "Type"), 2.0f, HAlign_Left, &NodeType },
			{ FName("LOD"), LOCTEXT("ColLOD", "LODs"), 1.0f, HAlign_Center,
				[](const FAffectedNode& N) { const UStaticMesh* M = MeshFromNode(N); return M ? FText::AsNumber(M->GetNumLODs()) : Dash; } },
			{ FName("Nanite"), LOCTEXT("ColNanite", "Nanite"), 1.0f, HAlign_Center,
				[](const FAffectedNode& N) { const UStaticMesh* M = MeshFromNode(N); return M ? (M->IsNaniteEnabled() ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No")) : Dash; } },
			{ FName("Tris"), LOCTEXT("ColTris", "Triangles"), 1.6f, HAlign_Right,
				[](const FAffectedNode& N) { const UStaticMesh* M = MeshFromNode(N); return M ? FText::AsNumber(M->GetNumTriangles(0)) : Dash; } },
			{ FName("Verts"), LOCTEXT("ColVerts", "Verts"), 1.6f, HAlign_Right,
				[](const FAffectedNode& N) { const UStaticMesh* M = MeshFromNode(N); return M ? FText::AsNumber(M->GetNumVertices(0)) : Dash; } },
		};
	}

	TArray<FAssetColumn> MakeTextureColumns()
	{
		return {
			{ OptimizeColumns::Object, LOCTEXT("ColObject", "Object"), 3.0f, HAlign_Left, &NodeName },
			{ FName("Res"), LOCTEXT("ColRes", "Resolution"), 1.6f, HAlign_Center,
				[](const FAffectedNode& N) { const UTexture2D* T = TextureFromNode(N); return T ? FText::Format(LOCTEXT("ResFmt", "{0}x{1}"), FText::AsNumber(T->GetSizeX()), FText::AsNumber(T->GetSizeY())) : Dash; } },
			{ FName("Format"), LOCTEXT("ColFormat", "Format"), 1.2f, HAlign_Center,
				[](const FAffectedNode& N) { const UTexture2D* T = TextureFromNode(N); return T ? FText::FromString(GPixelFormats[T->GetPixelFormat()].Name) : Dash; } },
			{ FName("Size"), LOCTEXT("ColSize", "Size"), 1.4f, HAlign_Right,
				[](const FAffectedNode& N) { const UTexture2D* T = TextureFromNode(N); return T ? FText::AsMemory(T->CalcTextureMemorySizeEnum(TMC_AllMips)) : Dash; } },
			{ FName("Stream"), LOCTEXT("ColStream", "Streaming"), 1.6f, HAlign_Center,
				[](const FAffectedNode& N) { const UTexture2D* T = TextureFromNode(N); return T ? TextureStreamState(*T) : Dash; } },
			{ FName("Mips"), LOCTEXT("ColMips", "Mips"), 1.0f, HAlign_Center,
				[](const FAffectedNode& N) { const UTexture2D* T = TextureFromNode(N); return T ? FText::AsNumber(T->GetNumMips()) : Dash; } },
		};
	}

	TArray<FAssetColumn> MakeGenericColumns()
	{
		return {
			{ OptimizeColumns::Object, LOCTEXT("ColObject", "Object"), 3.0f, HAlign_Left,
				[](const FAffectedNode& N) { return N.Finding.IsValid() ? N.Finding->Subject : NodeName(N); } },
			{ FName("Type"), LOCTEXT("ColType", "Type"), 2.0f, HAlign_Left, &NodeType },
		};
	}
}

/**
 * A multi-column row that defers each cell to the owning view, so the column
 * schema lives in one place and rows stay a thin adapter over SHeaderRow.
 */
class SAffectedRow : public SMultiColumnTableRow<TSharedPtr<FAffectedNode>>
{
public:
	SLATE_BEGIN_ARGS(SAffectedRow) {}
		SLATE_ARGUMENT(TFunction<TSharedRef<SWidget>(const FName&)>, OnGenerateCell)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
	{
		OnGenerateCell = InArgs._OnGenerateCell;
		SMultiColumnTableRow<TSharedPtr<FAffectedNode>>::Construct(
			FSuperRowType::FArguments().Padding(FMargin(0, 2)), OwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		TSharedRef<SWidget> Cell = OnGenerateCell ? OnGenerateCell(ColumnName) : SNullWidget::NullWidget;

		// A multi-column tree row doesn't place the expander itself; we host it in
		// the Object column, so group rows get the classic indented arrow + name.
		if (ColumnName == OptimizeColumns::Object)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SExpanderArrow, SharedThis(this))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					Cell
				];
		}
		return Cell;
	}

private:
	TFunction<TSharedRef<SWidget>(const FName&)> OnGenerateCell;
};

int32 FOptimizeNode::LeafCount() const
{
	if (Kind == EOptimizeNodeKind::Problem)
	{
		return Findings.Num();
	}
	int32 Total = 0;
	for (const TSharedPtr<FOptimizeNode>& Child : Children)
	{
		if (Child.IsValid())
		{
			Total += Child->LeafCount();
		}
	}
	return Total;
}

void SOptimizeView::Construct(const FArguments& InArgs)
{
	Model = MakeShared<FToolsetModel>();
	ChangedHandle = Model->OnChanged().AddSP(this, &SOptimizeView::Refresh);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Top bar: scan + live summary.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SOptimizeView::OnScanClicked)
				[
					SNew(STextBlock).Text(LOCTEXT("Scan", "Scan Level"))
				]
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(12, 0, 0, 0))
			[
				SNew(STextBlock).Text(this, &SOptimizeView::GetSummaryText)
			]
		]

		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSeparator) ]

		// Master (tree) over detail (affected assets).
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<FOptimizeNode>>)
					.TreeItemsSource(&TreeRoots)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SOptimizeView::GenerateTreeRow)
					.OnGetChildren(this, &SOptimizeView::GetTreeChildren)
					.OnSelectionChanged(this, &SOptimizeView::OnTreeSelectionChanged)
				]
			]

			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(6, 4))
				[
					SNew(STextBlock)
					.Text(this, &SOptimizeView::GetAffectedHeaderText)
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SAssignNew(AffectedList, STreeView<TSharedPtr<FAffectedNode>>)
						.TreeItemsSource(&AffectedItems)
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow(this, &SOptimizeView::GenerateAffectedRow)
						.OnGetChildren(this, &SOptimizeView::GetAffectedChildren)
						.HeaderRow(SAssignNew(HeaderRow, SHeaderRow))
					]
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSeparator) ]

		// Bottom bar: selection count + apply.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]()
				{
					return FText::Format(LOCTEXT("SelectedFmt", "{0} selected, {1} to fix"),
						FText::AsNumber(CheckedNodes.Num()), FText::AsNumber(CheckedFixableCount()));
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.IsEnabled_Lambda([this]() { return IsApplyEnabled(); })
				.OnClicked(this, &SOptimizeView::OnApplyClicked)
				[
					SNew(STextBlock).Text(LOCTEXT("Apply", "Apply Fixes"))
				]
			]
		]
	];

	Refresh();
}

SOptimizeView::~SOptimizeView()
{
	if (Model.IsValid())
	{
		Model->OnChanged().Remove(ChangedHandle);
	}
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------
void SOptimizeView::Refresh()
{
	// Remember which problem was open so a rescan (e.g. after Apply) can restore it.
	const FName PreviousType = SelectedProblem.IsValid() ? SelectedProblem->TypeId : NAME_None;

	RebuildTree();

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
		for (const TSharedPtr<FOptimizeNode>& Root : TreeRoots)
		{
			TreeView->SetItemExpansion(Root, true);
		}
	}

	// Re-open the same problem type if it still exists after the rescan.
	TSharedPtr<FOptimizeNode> Restored;
	if (!PreviousType.IsNone())
	{
		for (const TSharedPtr<FOptimizeNode>& Level : TreeRoots)
		{
			for (const TSharedPtr<FOptimizeNode>& Problem : Level->Children)
			{
				if (Problem->TypeId == PreviousType)
				{
					Restored = Problem;
					break;
				}
			}
			if (Restored.IsValid())
			{
				break;
			}
		}
	}
	SelectProblem(Restored);
}

void SOptimizeView::RebuildTree()
{
	TreeRoots.Reset();

	// Persistent-level name labels findings with no sub-level of their own.
	FText PersistentLabel = LOCTEXT("PersistentLevel", "Persistent Level");
	if (const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		if (const ULevel* Persistent = World->PersistentLevel)
		{
			PersistentLabel = FText::FromString(FPackageName::GetShortName(Persistent->GetOutermost()->GetName()));
		}
	}

	if (!Model.IsValid())
	{
		return;
	}

	// Group by level, then by problem type within a level.
	TMap<FString, TSharedPtr<FOptimizeNode>> LevelNodes;
	for (const TSharedPtr<FFinding>& Finding : Model->GetAllFindings())
	{
		if (!Finding.IsValid())
		{
			continue;
		}

		const FText LevelLabel = Finding->LevelName.IsEmpty() ? PersistentLabel : Finding->LevelName;
		TSharedPtr<FOptimizeNode>& LevelNode = LevelNodes.FindOrAdd(LevelLabel.ToString());
		if (!LevelNode.IsValid())
		{
			LevelNode = MakeShared<FOptimizeNode>();
			LevelNode->Kind = EOptimizeNodeKind::Level;
			LevelNode->Label = LevelLabel;
			TreeRoots.Add(LevelNode);
		}

		const FName TypeId = Finding->TypeId.IsNone() ? FName(*Finding->Title.ToString()) : Finding->TypeId;
		TSharedPtr<FOptimizeNode> ProblemNode;
		for (const TSharedPtr<FOptimizeNode>& Existing : LevelNode->Children)
		{
			if (Existing->TypeId == TypeId)
			{
				ProblemNode = Existing;
				break;
			}
		}
		if (!ProblemNode.IsValid())
		{
			ProblemNode = MakeShared<FOptimizeNode>();
			ProblemNode->Kind = EOptimizeNodeKind::Problem;
			ProblemNode->TypeId = TypeId;
			ProblemNode->Label = Finding->Title;
			ProblemNode->Why = Finding->WhyItMatters;
			ProblemNode->How = Finding->HowToFix;
			LevelNode->Children.Add(ProblemNode);
		}

		ProblemNode->Findings.Add(Finding);
		if (static_cast<uint8>(Finding->Severity) < static_cast<uint8>(ProblemNode->Worst))
		{
			ProblemNode->Worst = Finding->Severity;
		}
	}

	// Worst severity per level, then stable ordering.
	for (const TSharedPtr<FOptimizeNode>& Level : TreeRoots)
	{
		for (const TSharedPtr<FOptimizeNode>& Problem : Level->Children)
		{
			if (static_cast<uint8>(Problem->Worst) < static_cast<uint8>(Level->Worst))
			{
				Level->Worst = Problem->Worst;
			}
		}
		Level->Children.Sort([](const TSharedPtr<FOptimizeNode>& A, const TSharedPtr<FOptimizeNode>& B)
		{
			if (A->Worst != B->Worst)
			{
				return static_cast<uint8>(A->Worst) < static_cast<uint8>(B->Worst);
			}
			return A->Label.CompareTo(B->Label) < 0;
		});
	}
	TreeRoots.Sort([](const TSharedPtr<FOptimizeNode>& A, const TSharedPtr<FOptimizeNode>& B)
	{
		return A->Label.CompareTo(B->Label) < 0;
	});
}

// ---------------------------------------------------------------------------
// Tree
// ---------------------------------------------------------------------------
void SOptimizeView::GetTreeChildren(TSharedPtr<FOptimizeNode> Node, TArray<TSharedPtr<FOptimizeNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

TSharedRef<ITableRow> SOptimizeView::GenerateTreeRow(TSharedPtr<FOptimizeNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Problem nodes are cards carrying their own explanation; level nodes stay a
	// plain grouping header.
	if (Node->Kind == EOptimizeNodeKind::Problem)
	{
		return SNew(STableRow<TSharedPtr<FOptimizeNode>>, OwnerTable)
			.Padding(FMargin(2, 3))
			[
				MakeProblemCard(Node)
			];
	}

	return SNew(STableRow<TSharedPtr<FOptimizeNode>>, OwnerTable)
		.Padding(FMargin(0, 4, 0, 2))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 6, 0))
			[
				SNew(SColorBlock).Color(FToolsetStyle::ColorForSeverity(Node->Worst)).Size(FVector2D(8.0f, 8.0f))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Node->Label).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("CountFmt", "({0})"), FText::AsNumber(Node->LeafCount())))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

TSharedRef<SWidget> SOptimizeView::MakeProblemCard(const TSharedPtr<FOptimizeNode>& Node)
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	// Header: severity marker + title + count.
	Body->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 7, 0))
		[
			SNew(SColorBlock).Color(FToolsetStyle::ColorForSeverity(Node->Worst)).Size(FVector2D(9.0f, 9.0f))
		]

		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Node->Label).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("CardCountFmt", "{0}"), FText::AsNumber(Node->LeafCount())))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];

	// Why it matters (moved out of the old tooltip, into the card).
	if (!Node->Why.IsEmpty())
	{
		Body->AddSlot().AutoHeight().Padding(FMargin(16, 5, 0, 0))
		[
			SNew(STextBlock)
			.Text(Node->Why)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}

	// How to fix.
	if (!Node->How.IsEmpty())
	{
		Body->AddSlot().AutoHeight().Padding(FMargin(16, 3, 0, 0))
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("CardFixFmt", "Fix: {0}"), Node->How))
			.AutoWrapText(true)
		];
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10, 8))
		[
			Body
		];
}

void SOptimizeView::OnTreeSelectionChanged(TSharedPtr<FOptimizeNode> Node, ESelectInfo::Type)
{
	SelectProblem(Node.IsValid() && Node->Kind == EOptimizeNodeKind::Problem ? Node : nullptr);
}

// ---------------------------------------------------------------------------
// Affected list
// ---------------------------------------------------------------------------
void SOptimizeView::SelectProblem(TSharedPtr<FOptimizeNode> Problem)
{
	SelectedProblem = Problem;
	AffectedItems.Reset();
	CheckedNodes.Reset();

	if (Problem.IsValid())
	{
		for (const TSharedPtr<FFinding>& Finding : Problem->Findings)
		{
			if (!Finding.IsValid())
			{
				continue;
			}

			TSharedPtr<FAffectedNode> Node = MakeShared<FAffectedNode>();
			Node->Finding = Finding;

			// A group finding (repeated actors an ISM/HISM collapses) expands into
			// one child per member, so the meshes are inspectable individually.
			for (const TWeakObjectPtr<AActor>& Member : Finding->RelatedActors)
			{
				if (Member.IsValid())
				{
					TSharedPtr<FAffectedNode> Child = MakeShared<FAffectedNode>();
					Child->Finding = Finding;
					Child->MemberActor = Member;
					Node->Children.Add(Child);
				}
			}

			AffectedItems.Add(Node);

			// Default-tick the fixable rows: a group starts with every member ticked
			// (i.e. the parent full), a single finding with itself. Non-fixable rows
			// can't be selected, so they start clear.
			if (FToolsetModel::HasSupportedFix(*Finding))
			{
				if (Node->Children.Num() > 0)
				{
					for (const TSharedPtr<FAffectedNode>& Child : Node->Children)
					{
						CheckedNodes.Add(Child);
					}
				}
				else
				{
					CheckedNodes.Add(Node);
				}
			}
		}
	}

	BuildColumns(Problem);

	if (AffectedList.IsValid())
	{
		AffectedList->RequestTreeRefresh();
	}
}

void SOptimizeView::GetAffectedChildren(TSharedPtr<FAffectedNode> Node, TArray<TSharedPtr<FAffectedNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

void SOptimizeView::BuildColumns(const TSharedPtr<FOptimizeNode>& Problem)
{
	// Column set follows what the problem is about: the first finding that resolves
	// to a texture or mesh decides it (a problem type is homogeneous).
	Columns.Reset();
	if (Problem.IsValid())
	{
		bool bTexture = false;
		bool bMesh = false;
		for (const TSharedPtr<FFinding>& Finding : Problem->Findings)
		{
			if (!Finding.IsValid())
			{
				continue;
			}
			if (Finding->TargetAsset.Get() && Finding->TargetAsset.Get()->IsA<UTexture2D>())
			{
				bTexture = true;
				break;
			}
			if (MeshFromFinding(*Finding))
			{
				bMesh = true;
				break;
			}
		}
		Columns = bTexture ? MakeTextureColumns() : (bMesh ? MakeMeshColumns() : MakeGenericColumns());

		// If this problem is about groups (an ISM/HISM candidate), show how many
		// components each group holds, right after the name.
		const bool bHasGroups = Problem->Findings.ContainsByPredicate(
			[](const TSharedPtr<FFinding>& F) { return F.IsValid() && F->RelatedActors.Num() > 0; });
		if (bHasGroups && Columns.Num() > 0)
		{
			Columns.Insert(ComponentsColumn(), 1);
		}
	}

	if (!HeaderRow.IsValid())
	{
		return;
	}

	HeaderRow->ClearColumns();

	// Fixed check column first.
	HeaderRow->AddColumn(SHeaderRow::Column(OptimizeColumns::Check)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(28.0f)
		.HAlignHeader(HAlign_Center));

	for (const FAssetColumn& Column : Columns)
	{
		HeaderRow->AddColumn(SHeaderRow::Column(Column.Id)
			.DefaultLabel(Column.Header)
			.FillWidth(Column.FillWidth)
			.HAlignHeader(Column.HAlign)
			.HAlignCell(Column.HAlign));
	}

	// Fixed navigation column last.
	HeaderRow->AddColumn(SHeaderRow::Column(OptimizeColumns::Nav)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(32.0f)
		.HAlignHeader(HAlign_Center));
}

TSharedRef<ITableRow> SOptimizeView::GenerateAffectedRow(TSharedPtr<FAffectedNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SAffectedRow, OwnerTable)
		.OnGenerateCell([this, Node](const FName& ColumnId)
		{
			return GenerateCell(ColumnId, Node);
		});
}

TSharedRef<SWidget> SOptimizeView::GenerateCell(const FName& ColumnId, TSharedPtr<FAffectedNode> Node)
{
	if (!Node.IsValid())
	{
		return SNullWidget::NullWidget;
	}
	const bool bMember = Node->IsMember();
	const TSharedPtr<FFinding> Finding = Node->Finding;

	if (ColumnId == OptimizeColumns::Check)
	{
		// Every fixable row is selectable now, members included, so a subset of a
		// group can be converted. The parent is a tri-state roll-up of its members.
		const bool bFixable = Finding.IsValid() && FToolsetModel::HasSupportedFix(*Finding);
		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsEnabled(bFixable)
			.ToolTipText(bFixable ? FText::GetEmpty() : LOCTEXT("NoAutoFix", "No automatic fix for this finding."))
			.IsChecked(this, &SOptimizeView::GetItemCheck, Node)
			.OnCheckStateChanged(this, &SOptimizeView::OnItemCheckChanged, Node)
		];
	}

	if (ColumnId == OptimizeColumns::Nav)
	{
		// A member focuses its own actor; a finding uses the shared navigator.
		const bool bCanNavigate = bMember ? Node->MemberActor.IsValid()
			: (Finding.IsValid() && FFindingNavigator::CanNavigate(*Finding));
		const FText Tip = bMember ? LOCTEXT("FocusMember", "Focus Actor")
			: (Finding.IsValid() ? FFindingNavigator::GetActionLabel(*Finding) : FText::GetEmpty());

		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4, 2))
			.IsEnabled(bCanNavigate)
			.ToolTipText(Tip)
			.OnClicked_Lambda([this, Node]()
			{
				if (Node->IsMember())
				{
					FLevelAnalyzer::FocusActor(Node->MemberActor);
				}
				else if (Node->Finding.IsValid())
				{
					FFindingNavigator::Navigate(*Node->Finding, Model);
				}
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Search"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	// A data column: find its extractor and render the value.
	const FAssetColumn* Column = Columns.FindByPredicate([&ColumnId](const FAssetColumn& C) { return C.Id == ColumnId; });
	if (!Column)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<STextBlock> Value = SNew(STextBlock)
		.Text(Column->GetText ? Column->GetText(*Node) : FText::GetEmpty())
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		.Justification(Column->HAlign == HAlign_Right ? ETextJustify::Right
			: (Column->HAlign == HAlign_Center ? ETextJustify::Center : ETextJustify::Left));

	// The Object column leads with the severity marker (parent rows only — a member
	// inherits the group's severity and reads better plain).
	if (ColumnId == OptimizeColumns::Object && !bMember && Finding.IsValid())
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6, 0, 6, 0))
			[
				SNew(SColorBlock)
				.Color(FToolsetStyle::ColorForSeverity(Finding->Severity))
				.Size(FVector2D(8.0f, 8.0f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Value
			];
	}

	return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(6, 0))
	[
		Value
	];
}

ECheckBoxState SOptimizeView::GetItemCheck(TSharedPtr<FAffectedNode> Node) const
{
	if (!Node.IsValid())
	{
		return ECheckBoxState::Unchecked;
	}

	// A group parent is the roll-up of its members: all ticked, none, or some.
	if (NodeIsGroupParent(*Node))
	{
		int32 Checked = 0;
		for (const TSharedPtr<FAffectedNode>& Child : Node->Children)
		{
			if (CheckedNodes.Contains(Child))
			{
				++Checked;
			}
		}
		if (Checked == 0)
		{
			return ECheckBoxState::Unchecked;
		}
		return Checked == Node->Children.Num() ? ECheckBoxState::Checked : ECheckBoxState::Undetermined;
	}

	return CheckedNodes.Contains(Node) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOptimizeView::OnItemCheckChanged(ECheckBoxState State, TSharedPtr<FAffectedNode> Node)
{
	if (!Node.IsValid())
	{
		return;
	}

	// Parent toggles every member: a partial or empty group fills, a full one clears.
	if (NodeIsGroupParent(*Node))
	{
		bool bAllChecked = Node->Children.Num() > 0;
		for (const TSharedPtr<FAffectedNode>& Child : Node->Children)
		{
			bAllChecked &= CheckedNodes.Contains(Child);
		}
		for (const TSharedPtr<FAffectedNode>& Child : Node->Children)
		{
			if (bAllChecked)
			{
				CheckedNodes.Remove(Child);
			}
			else
			{
				CheckedNodes.Add(Child);
			}
		}
		return;
	}

	if (State == ECheckBoxState::Checked)
	{
		CheckedNodes.Add(Node);
	}
	else
	{
		CheckedNodes.Remove(Node);
	}
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
FReply SOptimizeView::OnScanClicked()
{
	if (Model.IsValid())
	{
		Model->RunScan();
	}
	return FReply::Handled();
}

int32 SOptimizeView::CheckedFixableCount() const
{
	// The number of assets Apply would actually touch: selected members of a group
	// (only if enough remain to be worth instancing), or a selected single finding.
	int32 Count = 0;
	for (const TSharedPtr<FAffectedNode>& Parent : AffectedItems)
	{
		if (!Parent.IsValid() || !Parent->Finding.IsValid() || !FToolsetModel::HasSupportedFix(*Parent->Finding))
		{
			continue;
		}
		if (Parent->Children.Num() > 0)
		{
			int32 Selected = 0;
			for (const TSharedPtr<FAffectedNode>& Child : Parent->Children)
			{
				Selected += CheckedNodes.Contains(Child) ? 1 : 0;
			}
			// Instancing one actor is pointless; the fix needs at least two.
			if (Selected >= 2)
			{
				Count += Selected;
			}
		}
		else if (CheckedNodes.Contains(Parent))
		{
			++Count;
		}
	}
	return Count;
}

bool SOptimizeView::IsApplyEnabled() const
{
	return CheckedFixableCount() > 0;
}

FReply SOptimizeView::OnApplyClicked()
{
	if (!Model.IsValid())
	{
		return FReply::Handled();
	}

	TArray<TSharedPtr<FFinding>> ToFix;
	for (const TSharedPtr<FAffectedNode>& Parent : AffectedItems)
	{
		if (!Parent.IsValid() || !Parent->Finding.IsValid() || !FToolsetModel::HasSupportedFix(*Parent->Finding))
		{
			continue;
		}

		if (Parent->Children.Num() > 0)
		{
			// Group: fix only the ticked members, by handing the fix a copy of the
			// finding narrowed to that subset. The original finding is untouched.
			TArray<TWeakObjectPtr<AActor>> Selected;
			for (const TSharedPtr<FAffectedNode>& Child : Parent->Children)
			{
				if (CheckedNodes.Contains(Child) && Child->MemberActor.IsValid())
				{
					Selected.Add(Child->MemberActor);
				}
			}
			if (Selected.Num() >= 2)
			{
				FFinding Subset = *Parent->Finding;
				Subset.RelatedActors = MoveTemp(Selected);
				ToFix.Add(MakeShared<FFinding>(MoveTemp(Subset)));
			}
		}
		else if (CheckedNodes.Contains(Parent))
		{
			ToFix.Add(Parent->Finding);
		}
	}

	// ApplyFixes rescans once at the end, which triggers Refresh() through OnChanged.
	Model->ApplyFixes(ToFix);
	return FReply::Handled();
}

FText SOptimizeView::GetSummaryText() const
{
	if (!Model.IsValid() || !Model->HasScanned())
	{
		return LOCTEXT("NoScan", "No scan yet — press Scan Level.");
	}
	const FScanResult& Scan = Model->GetLastScan();
	return FText::Format(LOCTEXT("SummaryFmt", "{0} findings across {1} actors"),
		FText::AsNumber(Scan.Findings.Num()), FText::AsNumber(Scan.ActorsScanned));
}

FText SOptimizeView::GetAffectedHeaderText() const
{
	if (!SelectedProblem.IsValid())
	{
		return LOCTEXT("NoProblem", "Affected assets — select a problem in the tree above.");
	}
	return FText::Format(LOCTEXT("AffectedFmt", "Affected assets — {0} ({1})"),
		SelectedProblem->Label, FText::AsNumber(AffectedItems.Num()));
}

#undef LOCTEXT_NAMESPACE
