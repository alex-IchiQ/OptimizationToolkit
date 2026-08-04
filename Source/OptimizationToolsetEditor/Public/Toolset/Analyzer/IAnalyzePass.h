// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Toolset/Analyzer/LevelScanContext.h"

/**
 * One analysis pass: a self-contained, read-only check over the level.
 *
 * Passes are the unit of growth for Analyze — add a check by adding a class,
 * without touching the analyzer or the window. Engine-version differences are
 * gated with the OPTIMIZATION_* macros *inside* a pass, so only the code valid
 * for the current engine ever compiles.
 */
class IAnalyzePass
{
public:
	virtual ~IAnalyzePass() = default;

	/** Stable id, mostly for logging / de-dupe. */
	virtual FName GetId() const = 0;

	/** Inspect the pre-gathered level and append findings. Must not modify anything. */
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const = 0;
};
