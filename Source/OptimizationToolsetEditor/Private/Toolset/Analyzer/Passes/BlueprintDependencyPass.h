// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/**
 * What a Blueprint actually costs to load, rather than what it weighs itself.
 *
 * A Blueprint is usually a few kilobytes. Everything it hard-references comes in
 * with it, so a trivial-looking pickup can drag a 300 MB texture set into memory
 * the moment it is loaded. The Blueprint's own size never hints at this, and the
 * Size Map only tells you once you already suspect a particular asset — this runs
 * the same walk across every Blueprint in the level and reports the offenders.
 *
 * Follows hard package references only: that is exactly what a soft reference
 * would avoid, which is the fix being pointed at.
 */
class FBlueprintDependencyPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_BlueprintDependencies"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
