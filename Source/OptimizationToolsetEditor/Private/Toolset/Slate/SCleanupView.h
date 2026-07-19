// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SHeaderRow;
template <typename T> class SComboBox;

/** Why an asset is on the cleanup list. */
enum class ECleanupKind : uint8
{
	Unused,
	Duplicate,
};

/** One asset flagged for cleanup, with the info the list shows. */
struct FCleanupEntry
{
	FAssetData Asset;
	ECleanupKind Kind = ECleanupKind::Unused;
	int64 DiskSize = 0;
	FString TypeName;

	/** For a duplicate: the group key and how many share it. */
	FString DuplicateKey;
	int32 DuplicateGroupSize = 0;
};

/** The kind filter above the list. */
enum class ECleanupFilter : uint8
{
	All,
	Duplicate,
	Unused,
};

/**
 * Clean Up page for the new UI: project-wide duplicate and unused assets in one
 * filterable list.
 *
 * Duplicates use a name+type+size heuristic across folders (the re-imported pack
 * case), not a content hash. Unused reuses the plugin's stricter referencer check
 * (all dependency categories, with the level/redirector/config exclusions) rather
 * than the material-only test some tools ship, which over-reports.
 */
class SCleanupView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCleanupView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ---- Scan ---------------------------------------------------------------
	FReply OnScanClicked();
	void BuildEntries();

	// ---- Filtering ----------------------------------------------------------
	void ApplyFilters();
	void SetKindFilter(ECleanupFilter Filter);
	TSharedRef<SWidget> MakeKindButton(ECleanupFilter Filter, const FText& Label);

	// ---- List ---------------------------------------------------------------
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FCleanupEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> GenerateCell(const FName& ColumnId, TSharedPtr<FCleanupEntry> Entry);

	// ---- Type combo ---------------------------------------------------------
	TSharedRef<SWidget> MakeTypeComboEntry(TSharedPtr<FString> Item);
	void OnTypeSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	FText GetTypeComboLabel() const;

	FText GetSummaryText() const;

	bool bHasScanned = false;

	TArray<TSharedPtr<FCleanupEntry>> AllEntries;
	TArray<TSharedPtr<FCleanupEntry>> VisibleEntries;
	TSharedPtr<SListView<TSharedPtr<FCleanupEntry>>> ListView;

	ECleanupFilter KindFilter = ECleanupFilter::All;

	/** "All" plus each asset type present, for the type dropdown. */
	FString TypeFilter = TEXT("All");
	TArray<TSharedPtr<FString>> TypeOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeCombo;
};
