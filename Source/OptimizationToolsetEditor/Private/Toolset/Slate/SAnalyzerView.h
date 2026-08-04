// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/MemoryReport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class SVerticalBox;
class SWidgetSwitcher;

/** A sortable column: how to render a cell, and an optional numeric sort key. */
template <typename Item>
struct TMemoryColumn
{
	FName Id;
	FText Header;
	float Fill = 1.0f;
	EHorizontalAlignment Align = HAlign_Left;
	TFunction<FText(const Item&)> Get;
	TFunction<double(const Item&)> SortValue;	// unset: sort by the displayed text
};

/**
 * One list's data, columns, widget and sort state — sorting lives with the list
 * so a header click can re-sort in place without the view juggling per-list state.
 */
template <typename Item>
struct TMemoryList
{
	TArray<TSharedPtr<Item>> Items;
	TArray<TMemoryColumn<Item>> Columns;
	TSharedPtr<SListView<TSharedPtr<Item>>> List;
	FName SortColumn;
	EColumnSortMode::Type SortMode = EColumnSortMode::Descending;

	void ApplySort()
	{
		if (const TMemoryColumn<Item>* Column = Columns.FindByPredicate(
			[this](const TMemoryColumn<Item>& C) { return C.Id == SortColumn; }))
		{
			const bool bAscending = SortMode != EColumnSortMode::Descending;
			Items.Sort([Column, bAscending](const TSharedPtr<Item>& A, const TSharedPtr<Item>& B)
			{
				if (!A.IsValid() || !B.IsValid())
				{
					return A.IsValid();
				}
				if (Column->SortValue)
				{
					const double DA = Column->SortValue(*A);
					const double DB = Column->SortValue(*B);
					return bAscending ? DA < DB : DA > DB;
				}
				const int32 Cmp = Column->Get(*A).CompareTo(Column->Get(*B));
				return bAscending ? Cmp < 0 : Cmp > 0;
			});
		}
		if (List.IsValid())
		{
			List->RequestListRefresh();
		}
	}
};

/** Which list the tab strip is showing. */
enum class EAnalyzerTab : uint8
{
	Textures,
	RenderTargets,
	Meshes,
};

/**
 * Analyzer page for the new UI: what the loaded content costs in memory.
 *
 * A summary up top — texture composition as a stacked bar, plus streaming-pool
 * and GPU-memory meters — over a tab strip switching between the Textures, Render
 * Targets and Meshes lists, each with click-to-sort columns.
 */
class SAnalyzerView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnalyzerView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Recomputes the memory report and refreshes. Driven by the shell's Scan. */
	void Scan();

private:
	TSharedRef<SWidget> BuildSummary();
	void RebuildBars();

	TSharedRef<SWidget> MakeTabButton(EAnalyzerTab Tab, const FText& Label);
	void SelectTab(EAnalyzerTab Tab);

	/** Refills each list from the report, filtered by the search text, then sorts. */
	void RefreshLists();
	void OnSearchChanged(const FText& NewText);

	FText GetHeadlineText() const;

	FMemoryReport Report;
	bool bHasReport = false;
	FString SearchText;

	EAnalyzerTab CurrentTab = EAnalyzerTab::Textures;
	TSharedPtr<SWidgetSwitcher> ListSwitcher;

	// Bar containers, rebuilt on each scan.
	TSharedPtr<SVerticalBox> RoleBarBox;
	TSharedPtr<SVerticalBox> StreamingBarBox;
	TSharedPtr<SVerticalBox> GpuBarBox;

	TSharedPtr<TMemoryList<FMemoryTextureRow>> Textures;
	TSharedPtr<TMemoryList<FMemoryTextureRow>> RenderTargets;
	TSharedPtr<TMemoryList<FMemoryMeshRow>> Meshes;
};
