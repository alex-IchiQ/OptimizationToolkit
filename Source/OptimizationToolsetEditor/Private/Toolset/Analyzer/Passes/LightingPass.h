// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/** Lighting hygiene: too many movable (dynamic) lights. */
class FLightingPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_Lighting"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
