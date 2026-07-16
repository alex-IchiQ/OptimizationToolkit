// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** On-disk footprint of every package whose main asset is of one class. */
struct FProjectSizeEntry
{
	FName ClassName;
	int64 TotalBytes = 0;
	int32 PackageCount = 0;
};

/**
 * Where the project's disk footprint actually goes, grouped by asset class.
 *
 * Read-only and unrelated to the level scan: this measures package files under
 * /Game rather than anything placed in a map. It is deliberately not an
 * ICleanupAction — actions report a one-line summary, this needs a breakdown.
 */
struct FProjectSizeReport
{
	/** Sorted largest first. */
	TArray<FProjectSizeEntry> Entries;

	int64 TotalBytes = 0;
	int32 PackageCount = 0;
	double ComputeSeconds = 0.0;

	/** True when the asset registry was still scanning and the numbers would lie. */
	bool bRegistryIncomplete = false;

	/** Walks the asset registry under /Game and measures each package on disk. */
	static FProjectSizeReport Compute();
};
