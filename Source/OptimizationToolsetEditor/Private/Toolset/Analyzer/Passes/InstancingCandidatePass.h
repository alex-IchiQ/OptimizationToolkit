// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/** Finds conservative groups of repeated static-mesh actors suitable for ISM/HISM review. */
class FInstancingCandidatePass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_InstancingCandidates"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
