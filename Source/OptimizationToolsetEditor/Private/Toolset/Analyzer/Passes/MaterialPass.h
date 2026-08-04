// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/** Material and section-layout hygiene for mesh components used in the level. */
class FMaterialPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_Materials"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
