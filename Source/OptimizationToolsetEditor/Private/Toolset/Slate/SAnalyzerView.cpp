// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SAnalyzerView.h"

#include "Editor.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UObject/SoftObjectPath.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SAnalyzerView"

namespace
{
	/** The trailing magnifier column, shared across all three lists. */
	const FName NavColumn("Nav");

	void BrowseToAsset(const FString& Path)
	{
		if (GEditor && !Path.IsEmpty())
		{
			if (UObject* Object = FSoftObjectPath(Path).TryLoad())
			{
				GEditor->SyncBrowserToObject(Object);
			}
		}
	}

	/** A read-only multi-column row that defers each cell to a supplied lambda. */
	template <typename ItemType>
	class SColumnRow : public SMultiColumnTableRow<TSharedPtr<ItemType>>
	{
	public:
		SLATE_BEGIN_ARGS(SColumnRow) {}
			SLATE_ARGUMENT(TFunction<TSharedRef<SWidget>(const FName&)>, OnCell)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
		{
			OnCell = InArgs._OnCell;
			SMultiColumnTableRow<TSharedPtr<ItemType>>::Construct(
				typename SMultiColumnTableRow<TSharedPtr<ItemType>>::FSuperRowType::FArguments().Padding(FMargin(0, 1)), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			return OnCell ? OnCell(Column) : SNullWidget::NullWidget;
		}

	private:
		TFunction<TSharedRef<SWidget>(const FName&)> OnCell;
	};

	/** Builds a sortable list bound to a TMemoryList: header sorting + cells. */
	template <typename Item>
	TSharedRef<SWidget> BuildList(TSharedRef<TMemoryList<Item>> State)
	{
		TSharedRef<SHeaderRow> Header = SNew(SHeaderRow);
		for (const TMemoryColumn<Item>& Column : State->Columns)
		{
			const FName ColumnId = Column.Id;

			// The magnifier column is a fixed, unsortable action, not data.
			if (ColumnId == NavColumn)
			{
				Header->AddColumn(SHeaderRow::Column(ColumnId).DefaultLabel(FText::GetEmpty())
					.FixedWidth(32.0f).HAlignHeader(HAlign_Center));
				continue;
			}

			Header->AddColumn(SHeaderRow::Column(ColumnId)
				.DefaultLabel(Column.Header)
				.FillWidth(Column.Fill)
				.HAlignHeader(Column.Align)
				.HAlignCell(Column.Align)
				.SortMode_Lambda([State, ColumnId]()
				{
					return State->SortColumn == ColumnId ? State->SortMode : EColumnSortMode::None;
				})
				.OnSort_Lambda([State](EColumnSortPriority::Type, const FName& Column, EColumnSortMode::Type NewMode)
				{
					State->SortColumn = Column;
					State->SortMode = NewMode;
					State->ApplySort();
				}));
		}

		return SAssignNew(State->List, SListView<TSharedPtr<Item>>)
			.ListItemsSource(&State->Items)
			.SelectionMode(ESelectionMode::None)
			.HeaderRow(Header)
			.OnGenerateRow_Lambda([State](TSharedPtr<Item> ItemPtr, const TSharedRef<STableViewBase>& OwnerTable)
			{
				return SNew(SColumnRow<Item>, OwnerTable)
					.OnCell([ItemPtr, ColumnsCopy = State->Columns](const FName& ColumnId) -> TSharedRef<SWidget>
					{
						if (!ItemPtr.IsValid())
						{
							return SNullWidget::NullWidget;
						}

						// Magnifier: jump to the asset in the Content Browser.
						if (ColumnId == NavColumn)
						{
							const FString Path = ItemPtr->Path;
							return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(&FAppStyle::Get(), "SimpleButton")
								.ContentPadding(FMargin(4, 2))
								.ToolTipText(LOCTEXT("ShowAsset", "Show Asset"))
								.OnClicked_Lambda([Path]() { BrowseToAsset(Path); return FReply::Handled(); })
								[
									SNew(SImage).Image(FAppStyle::GetBrush("Icons.Search")).ColorAndOpacity(FSlateColor::UseForeground())
								]
							];
						}

						const TMemoryColumn<Item>* Column = ColumnsCopy.FindByPredicate(
							[&ColumnId](const TMemoryColumn<Item>& C) { return C.Id == ColumnId; });
						if (!Column)
						{
							return SNullWidget::NullWidget;
						}
						return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(6, 0))
						[
							SNew(STextBlock)
							.Text(Column->Get(*ItemPtr))
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							.Justification(Column->Align == HAlign_Right ? ETextJustify::Right
								: (Column->Align == HAlign_Center ? ETextJustify::Center : ETextJustify::Left))
						];
					});
			});
	}

	FLinearColor RoleColor(const FString& Role)
	{
		if (Role == TEXT("Diffuse"))     return FLinearColor(FColor(0x4A, 0xA3, 0xED));
		if (Role == TEXT("Normal"))      return FLinearColor(FColor(0xA9, 0x7B, 0xF0));
		if (Role == TEXT("UI"))          return FLinearColor(FColor(0xE8, 0x61, 0x9D));
		if (Role == TEXT("Lightmap"))    return FLinearColor(FColor(0xF5, 0xA7, 0x23));
		if (Role == TEXT("RenderTarget"))return FLinearColor(FColor(0x2E, 0xCC, 0x71));
		return FLinearColor(FColor(0x7A, 0x82, 0x8C));	// Other
	}

	FText Resolution(const FMemoryTextureRow& Row)
	{
		return FText::Format(LOCTEXT("ResFmt", "{0}x{1}"), FText::AsNumber(Row.SizeX), FText::AsNumber(Row.SizeY));
	}

	/** A single stacked bar from coloured fractions, height 16. */
	TSharedRef<SWidget> MakeBar(const TArray<TPair<FLinearColor, float>>& Segments)
	{
		TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
		for (const TPair<FLinearColor, float>& Segment : Segments)
		{
			if (Segment.Value <= 0.0f)
			{
				continue;
			}
			Bar->AddSlot().FillWidth(Segment.Value)
			[
				SNew(SColorBlock).Color(Segment.Key)
			];
		}
		return SNew(SBox).HeightOverride(16.0f)[ Bar ];
	}
}

void SAnalyzerView::Construct(const FArguments& InArgs)
{
	Textures = MakeShared<TMemoryList<FMemoryTextureRow>>();
	RenderTargets = MakeShared<TMemoryList<FMemoryTextureRow>>();
	Meshes = MakeShared<TMemoryList<FMemoryMeshRow>>();

	Textures->Columns = {
		{ FName("Name"), LOCTEXT("CName", "Texture"),    3.0f, HAlign_Left,   [](const FMemoryTextureRow& R) { return FText::FromString(R.Name); } },
		{ FName("Res"),  LOCTEXT("CRes", "Resolution"),  1.5f, HAlign_Center, &Resolution, [](const FMemoryTextureRow& R) { return double(R.SizeX) * R.SizeY; } },
		{ FName("Fmt"),  LOCTEXT("CFmt", "Format"),      1.2f, HAlign_Center, [](const FMemoryTextureRow& R) { return FText::FromString(R.Format); } },
		{ FName("Full"), LOCTEXT("CFull", "Full"),       1.2f, HAlign_Right,  [](const FMemoryTextureRow& R) { return FText::AsMemory(R.FullBytes); }, [](const FMemoryTextureRow& R) { return double(R.FullBytes); } },
		{ FName("Res2"), LOCTEXT("CRnt", "Resident"),    1.2f, HAlign_Right,  [](const FMemoryTextureRow& R) { return FText::AsMemory(R.ResidentBytes); }, [](const FMemoryTextureRow& R) { return double(R.ResidentBytes); } },
		{ FName("Mips"), LOCTEXT("CMips", "Mips"),       1.3f, HAlign_Center, [](const FMemoryTextureRow& R) { return FText::Format(LOCTEXT("MipFmt", "{0}/{1}"), FText::AsNumber(R.ResidentMips), FText::AsNumber(R.NumMips)); }, [](const FMemoryTextureRow& R) { return double(R.NumMips); } },
		{ FName("Str"),  LOCTEXT("CStr", "Streaming"),   1.6f, HAlign_Center, [](const FMemoryTextureRow& R) { return FText::FromString(R.StreamState); } },
		{ FName("Role"), LOCTEXT("CRole", "Role"),       1.2f, HAlign_Left,   [](const FMemoryTextureRow& R) { return FText::FromString(R.Role); } },
		{ NavColumn,     FText::GetEmpty(),              0.5f, HAlign_Center, [](const FMemoryTextureRow&) { return FText::GetEmpty(); } },
	};
	Textures->SortColumn = FName("Full");

	RenderTargets->Columns = {
		{ FName("Name"), LOCTEXT("RtName", "Render Target"), 3.0f, HAlign_Left,   [](const FMemoryTextureRow& R) { return FText::FromString(R.Name); } },
		{ FName("Res"),  LOCTEXT("RtRes", "Resolution"),     1.5f, HAlign_Center, &Resolution, [](const FMemoryTextureRow& R) { return double(R.SizeX) * R.SizeY; } },
		{ FName("Fmt"),  LOCTEXT("RtFmt", "Format"),         1.2f, HAlign_Center, [](const FMemoryTextureRow& R) { return FText::FromString(R.Format); } },
		{ FName("Size"), LOCTEXT("RtSize", "Size"),          1.2f, HAlign_Right,  [](const FMemoryTextureRow& R) { return FText::AsMemory(R.FullBytes); }, [](const FMemoryTextureRow& R) { return double(R.FullBytes); } },
		{ NavColumn,     FText::GetEmpty(),                  0.5f, HAlign_Center, [](const FMemoryTextureRow&) { return FText::GetEmpty(); } },
	};
	RenderTargets->SortColumn = FName("Size");

	Meshes->Columns = {
		{ FName("Name"), LOCTEXT("MName", "Mesh"),      3.0f, HAlign_Left,   [](const FMemoryMeshRow& R) { return FText::FromString(R.Name); } },
		{ FName("Type"), LOCTEXT("MType", "Type"),      1.2f, HAlign_Left,   [](const FMemoryMeshRow& R) { return FText::FromString(R.Type); } },
		{ FName("Tris"), LOCTEXT("MTris", "Triangles"), 1.4f, HAlign_Right,  [](const FMemoryMeshRow& R) { return FText::AsNumber(R.Triangles); }, [](const FMemoryMeshRow& R) { return double(R.Triangles); } },
		{ FName("LODs"), LOCTEXT("MLods", "LODs"),      1.0f, HAlign_Center, [](const FMemoryMeshRow& R) { return FText::AsNumber(R.LODs); }, [](const FMemoryMeshRow& R) { return double(R.LODs); } },
		{ FName("Size"), LOCTEXT("MSize", "Size"),      1.2f, HAlign_Right,  [](const FMemoryMeshRow& R) { return FText::AsMemory(R.Bytes); }, [](const FMemoryMeshRow& R) { return double(R.Bytes); } },
		{ NavColumn,     FText::GetEmpty(),             0.5f, HAlign_Center, [](const FMemoryMeshRow&) { return FText::GetEmpty(); } },
	};
	Meshes->SortColumn = FName("Size");

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(12, 10, 12, 6))
		[
			BuildSummary()
		]

		// Tab strip + search.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(12, 0, 12, 4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 3, 0))[ MakeTabButton(EAnalyzerTab::Textures, LOCTEXT("TabTex", "Textures")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 3, 0))[ MakeTabButton(EAnalyzerTab::RenderTargets, LOCTEXT("TabRT", "Render Targets")) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeTabButton(EAnalyzerTab::Meshes, LOCTEXT("TabMesh", "Meshes")) ]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(12, 0, 0, 0))
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Filter by name…"))
				.OnTextChanged(this, &SAnalyzerView::OnSearchChanged)
			]
		]

		// Switched list.
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(12, 0, 12, 10))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(ListSwitcher, SWidgetSwitcher)
				+ SWidgetSwitcher::Slot()[ BuildList<FMemoryTextureRow>(Textures.ToSharedRef()) ]
				+ SWidgetSwitcher::Slot()[ BuildList<FMemoryTextureRow>(RenderTargets.ToSharedRef()) ]
				+ SWidgetSwitcher::Slot()[ BuildList<FMemoryMeshRow>(Meshes.ToSharedRef()) ]
			]
		]
	];

	SelectTab(CurrentTab);
}

TSharedRef<SWidget> SAnalyzerView::BuildSummary()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(12, 10))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(this, &SAnalyzerView::GetHeadlineText).AutoWrapText(true)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))[ SAssignNew(RoleBarBox, SVerticalBox) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))[ SAssignNew(StreamingBarBox, SVerticalBox) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 12, 0, 0))[ SAssignNew(GpuBarBox, SVerticalBox) ]
		];
}

void SAnalyzerView::Scan()
{
	FScopedSlowTask SlowTask(0.0f, LOCTEXT("Measuring", "Measuring loaded memory..."));
	SlowTask.MakeDialog();

	Report = FMemoryReport::Compute();
	bHasReport = true;

	RefreshLists();
	RebuildBars();
}

void SAnalyzerView::OnSearchChanged(const FText& NewText)
{
	SearchText = NewText.ToString();
	RefreshLists();
}

void SAnalyzerView::RefreshLists()
{
	const FString& Filter = SearchText;

	auto FillTextures = [&Filter](TSharedPtr<TMemoryList<FMemoryTextureRow>> List, const TArray<FMemoryTextureRow>& Rows)
	{
		List->Items.Reset();
		for (const FMemoryTextureRow& Row : Rows)
		{
			if (Filter.IsEmpty() || Row.Name.Contains(Filter))
			{
				List->Items.Add(MakeShared<FMemoryTextureRow>(Row));
			}
		}
		List->ApplySort();
	};
	FillTextures(Textures, Report.Textures);
	FillTextures(RenderTargets, Report.RenderTargets);

	Meshes->Items.Reset();
	for (const FMemoryMeshRow& Row : Report.Meshes)
	{
		if (Filter.IsEmpty() || Row.Name.Contains(Filter))
		{
			Meshes->Items.Add(MakeShared<FMemoryMeshRow>(Row));
		}
	}
	Meshes->ApplySort();
}

void SAnalyzerView::RebuildBars()
{
	// --- Texture composition by role (a Project-Size-style stacked bar). -----
	if (RoleBarBox.IsValid())
	{
		RoleBarBox->ClearChildren();
		if (Report.TextureFullTotal > 0)
		{
			TArray<TPair<FLinearColor, float>> Segments;
			TSharedRef<SWrapBox> Legend = SNew(SWrapBox).UseAllottedSize(true);
			for (const FMemoryRoleTotal& Role : Report.TextureRoles)
			{
				const FLinearColor Color = RoleColor(Role.Role);
				const float Fraction = static_cast<float>(double(Role.FullBytes) / double(Report.TextureFullTotal));
				Segments.Add({ Color, Fraction });

				Legend->AddSlot().Padding(FMargin(0, 0, 16, 4))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 6, 0))
					[ SNew(SBox).WidthOverride(9.0f).HeightOverride(9.0f)[ SNew(SColorBlock).Color(Color) ] ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ SNew(STextBlock).Text(FText::Format(LOCTEXT("RoleLeg", "{0} · {1}"), FText::FromString(Role.Role), FText::AsMemory(Role.FullBytes))) ]
				];
			}

			RoleBarBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("RoleTitle", "Textures by role")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
			RoleBarBox->AddSlot().AutoHeight().Padding(FMargin(0, 6, 0, 0))[ MakeBar(Segments) ];
			RoleBarBox->AddSlot().AutoHeight().Padding(FMargin(0, 8, 0, 0))[ Legend ];
		}
	}

	// --- Streaming pool meter. -----------------------------------------------
	if (StreamingBarBox.IsValid())
	{
		StreamingBarBox->ClearChildren();
		if (Report.StreamingPoolBudget > 0)
		{
			const float Used = FMath::Clamp(static_cast<float>(double(Report.StreamingPoolUsed) / double(Report.StreamingPoolBudget)), 0.0f, 1.0f);
			const bool bOver = Report.StreamingOverBudget > 0;
			const FLinearColor UsedColor = bOver ? FLinearColor(FColor(0xEF, 0x4A, 0x3F)) : FLinearColor(FColor(0x17, 0xB9, 0xA6));

			StreamingBarBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("StreamMeter", "Streaming pool — {0} used / {1} budget{2}"),
					FText::AsMemory(Report.StreamingPoolUsed), FText::AsMemory(Report.StreamingPoolBudget),
					bOver ? FText::Format(LOCTEXT("StreamOver", "  ·  {0} over"), FText::AsMemory(Report.StreamingOverBudget)) : FText::GetEmpty()))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
			StreamingBarBox->AddSlot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				MakeBar({ { UsedColor, Used }, { FLinearColor(FColor(0x33, 0x38, 0x3F)), 1.0f - Used } })
			];
		}
	}

	// --- GPU memory (tracked composition over dedicated VRAM). ----------------
	if (GpuBarBox.IsValid())
	{
		GpuBarBox->ClearChildren();
		if (Report.DedicatedVideoMemory > 0)
		{
			const double Total = double(Report.DedicatedVideoMemory);
			const float Tex = static_cast<float>(double(Report.TextureFullTotal) / Total);
			const float Mesh = static_cast<float>(double(Report.MeshTotal) / Total);
			const float Rt = static_cast<float>(double(Report.RenderTargetTotal) / Total);
			const float Rest = FMath::Max(0.0f, 1.0f - Tex - Mesh - Rt);

			GpuBarBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("GpuMeter", "GPU memory — {0} tracked of {1} dedicated"),
					FText::AsMemory(Report.GrandTotal), FText::AsMemory(Report.DedicatedVideoMemory)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
			GpuBarBox->AddSlot().AutoHeight().Padding(FMargin(0, 6, 0, 0))
			[
				MakeBar({
					{ FLinearColor(FColor(0x4A, 0xA3, 0xED)), Tex },
					{ FLinearColor(FColor(0xA9, 0x7B, 0xF0)), Mesh },
					{ FLinearColor(FColor(0x2E, 0xCC, 0x71)), Rt },
					{ FLinearColor(FColor(0x2A, 0x2E, 0x33)), Rest } })
			];
		}
	}
}

TSharedRef<SWidget> SAnalyzerView::MakeTabButton(EAnalyzerTab Tab, const FText& Label)
{
	return SNew(SButton)
		.ContentPadding(FMargin(12, 5))
		.ButtonColorAndOpacity_Lambda([this, Tab]()
		{
			return CurrentTab == Tab ? FStyleColors::Primary : FSlateColor(FLinearColor::White);
		})
		.OnClicked_Lambda([this, Tab]() { SelectTab(Tab); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Label)
		];
}

void SAnalyzerView::SelectTab(EAnalyzerTab Tab)
{
	CurrentTab = Tab;
	if (ListSwitcher.IsValid())
	{
		ListSwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	}
}

FText SAnalyzerView::GetHeadlineText() const
{
	if (!bHasReport)
	{
		return LOCTEXT("NoScan", "Press Scan to measure what the loaded content costs in memory.");
	}
	return FText::Format(
		LOCTEXT("Headline", "Tracked {0}   ·   Textures {1}   ·   Meshes {2}   ·   Render Targets {3}"),
		FText::AsMemory(Report.GrandTotal),
		FText::AsMemory(Report.TextureFullTotal),
		FText::AsMemory(Report.MeshTotal),
		FText::AsMemory(Report.RenderTargetTotal));
}

#undef LOCTEXT_NAMESPACE
