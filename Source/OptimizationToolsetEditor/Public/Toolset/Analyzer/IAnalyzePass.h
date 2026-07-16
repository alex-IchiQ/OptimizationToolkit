// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"

class UWorld;

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

	/** Inspect the world and append findings. Must not modify anything. */
	virtual void Run(UWorld* World, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const = 0;
};
