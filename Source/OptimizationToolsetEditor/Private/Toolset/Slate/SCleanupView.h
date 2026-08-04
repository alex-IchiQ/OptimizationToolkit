// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Toolset/Cleanup/ProjectSizeReport.h"	// EAssetCategory
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SHeaderRow;
class ICleanupAction;
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

	/** The exact class (shown in the Type column). */
	FString TypeName;

	/** The broad group the class falls in (drives the group filter). */
	EAssetCategory Category = EAssetCategory::Other;

	/** For a possible duplicate: the heuristic key and how many assets share it. */
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

/** Whether the last project sweep produced a complete, trustworthy result. */
enum class ECleanupScanState : uint8
{
	NotScanned,
	Complete,
	RegistryBusy,
	Cancelled,
};

/**
 * Clean Up page for the new UI: project-wide possible duplicates and unused assets in one
 * filterable list.
 *
 * Possible duplicates use a name+type+size heuristic across folders (the re-imported pack
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

	/** Rescans the project. Driven by the shell's single Scan action. */
	void Scan() { BuildEntries(); }

private:
	// ---- Scan ---------------------------------------------------------------
	void BuildEntries();

	// ---- Filtering ----------------------------------------------------------
	void ApplyFilters();
	void SetKindFilter(ECleanupFilter Filter);
	TSharedRef<SWidget> MakeKindButton(ECleanupFilter Filter, const FText& Label);
	void OnSearchChanged(const FText& NewText);

	// ---- List ---------------------------------------------------------------
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FCleanupEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> GenerateCell(const FName& ColumnId, TSharedPtr<FCleanupEntry> Entry);

	// ---- Selection + delete -------------------------------------------------
	ECheckBoxState GetItemCheck(TSharedPtr<FCleanupEntry> Entry) const;
	void OnItemCheckChanged(ECheckBoxState State, TSharedPtr<FCleanupEntry> Entry);
	ECheckBoxState GetSelectAllState() const;
	void OnSelectAllChanged(ECheckBoxState State);
	FReply OnDeleteClicked();
	bool IsDeleteEnabled() const;
	FText GetSelectionText() const;

	// ---- Group combo --------------------------------------------------------
	TSharedRef<SWidget> MakeGroupComboEntry(TSharedPtr<EAssetCategory> Item);
	void OnGroupSelected(TSharedPtr<EAssetCategory> Item, ESelectInfo::Type SelectInfo);
	FText GetGroupComboLabel() const;
	static FText GroupOptionLabel(const TSharedPtr<EAssetCategory>& Item);

	FText GetSummaryText() const;

	// ---- Project-wide actions (registry-driven) -----------------------------
	TSharedRef<SWidget> BuildActionBar();
	FReply OnRunAction(const ICleanupAction* Action);

	ECleanupScanState ScanState = ECleanupScanState::NotScanned;
	FText LastActionResult;

	TArray<TSharedPtr<FCleanupEntry>> AllEntries;
	TArray<TSharedPtr<FCleanupEntry>> VisibleEntries;
	TSharedPtr<SListView<TSharedPtr<FCleanupEntry>>> ListView;

	ECleanupFilter KindFilter = ECleanupFilter::All;
	FString SearchText;

	/** The ticked rows, for a batch delete. */
	TSet<TSharedPtr<FCleanupEntry>> CheckedEntries;

	/** No value = every group; otherwise narrows to one asset category. */
	TOptional<EAssetCategory> GroupFilter;
	TArray<TSharedPtr<EAssetCategory>> GroupOptions;
	TSharedPtr<SComboBox<TSharedPtr<EAssetCategory>>> GroupCombo;
};
