// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/**
 * Blueprint actors that tick every frame while being unable to move.
 *
 * Reports per Blueprint class, not per placed actor: tick lives on the class
 * defaults (PrimaryActorTick is EditDefaultsOnly), so a hundred placed copies
 * are one decision made once, not a hundred problems.
 *
 * Deliberately has no FixId. Turning tick off means rewriting the Blueprint's
 * class defaults, which changes every instance in every level of the project —
 * far too wide a blast radius for a button in a panel that scanned one level,
 * and it would run under "Apply all" alongside harmless mesh tweaks.
 */
class FBlueprintTickPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_BlueprintTick"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
