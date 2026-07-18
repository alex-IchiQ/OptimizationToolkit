// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/** Static-mesh hygiene: triangles, Nanite/material compatibility, LODs, and collision. */
class FStaticMeshPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_StaticMesh"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
