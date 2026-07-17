// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"

/**
 * Everything the panels know about the level, and the only thing they share.
 *
 * The registry keeps *features* from knowing about each other or the UI; this
 * keeps the *panels* from knowing about each other. Before it existed, the scan
 * result and the filters were fields on SToolsetWindow, so every panel that
 * needed them had to be built by that window — which is how one widget grew to
 * six panels and 1800 lines. A panel now takes the model and nothing else.
 *
 * Views react through OnChanged() rather than being poked by whoever caused the
 * change: a fix applied in Optimize moves the Dashboard's numbers, the nav's
 * badges and the Analyze list, and none of those should have to be listed at the
 * call site.
 *
 * Not a UObject and not exported: it is plain state, and MODULE_API on a class
 * holding move-only members is the dllexport trap FToolsetRegistry already hit.
 */
class FToolsetModel
{
public:
	/** Fires when the scan result or any filter changed — i.e. redraw. */
	FSimpleMulticastDelegate& OnChanged() { return ChangedEvent; }

	// ---- Scan ---------------------------------------------------------------
	/** Re-analyzes the active level and rebuilds the derived lists. */
	void RunScan();

	bool HasScanned() const { return bHasScanned; }
	const FScanResult& GetLastScan() const { return LastScan; }

	/**
	 * The scan before this one, so the Dashboard can show what moved. A fix that
	 * removes 40,000 triangles is invisible in an absolute number the user never
	 * wrote down; the delta is the proof the tool did something.
	 */
	bool HasPreviousStats() const { return bHasPreviousStats; }
	const FLevelStats& GetPreviousStats() const { return PreviousStats; }

	// ---- Findings -----------------------------------------------------------
	/** Every finding of the last scan, shared so views can hold a row cheaply. */
	const TArray<TSharedPtr<FFinding>>& GetAllFindings() const { return AllFindings; }

	/** The subset with a registered, supported fix. Same objects as above. */
	const TArray<TSharedPtr<FFinding>>& GetFixableFindings() const { return FixableFindings; }

	/** All findings passing severity + search + category. Analyze's list. */
	TArray<TSharedPtr<FFinding>> GetVisibleFindings() const;

	/**
	 * Fixable findings passing the category narrowing only: the severity/search
	 * toolbar belongs to Analyze, so Optimize must not silently inherit it.
	 */
	TArray<TSharedPtr<FFinding>> GetVisibleFixable() const;

	/** How many rows a category would show; drives the nav badges. */
	int32 CountForCategory(ECategory Category, bool bFixableOnly) const;

	// ---- Filters ------------------------------------------------------------
	void SetSearchFilter(const FString& InSearch);
	void ToggleSeverity(ESeverity Severity);
	bool IsSeverityEnabled(ESeverity Severity) const { return EnabledSeverities.Contains(Severity); }

	/** Unset means "everything"; the nav's section header clears it. */
	void SetCategoryFilter(TOptional<ECategory> Category);
	TOptional<ECategory> GetCategoryFilter() const { return CategoryFilter; }

	bool PassesSeverityAndSearch(const FFinding& Finding) const;
	bool PassesCategory(const FFinding& Finding) const;

	// ---- Fixes --------------------------------------------------------------
	static bool HasSupportedFix(const FFinding& Finding);

	/** Applies one fix, then rescans so every view reflects the new truth. */
	void ApplyFix(TSharedPtr<FFinding> Finding);

	/** Applies every currently fixable finding, then rescans once. */
	void ApplyAllFixes();

private:
	/** Rebuilds AllFindings/FixableFindings from LastScan and broadcasts. */
	void RebuildDerivedLists();

	FScanResult LastScan;
	bool bHasScanned = false;

	FLevelStats PreviousStats;
	bool bHasPreviousStats = false;

	TArray<TSharedPtr<FFinding>> AllFindings;
	TArray<TSharedPtr<FFinding>> FixableFindings;

	FString SearchFilter;
	TSet<ESeverity> EnabledSeverities = { ESeverity::Critical, ESeverity::Major, ESeverity::Minor };
	TOptional<ECategory> CategoryFilter;

	FSimpleMulticastDelegate ChangedEvent;
};
