// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/** Texture hygiene for assets actually referenced by primitive components in the level. */
class FTexturePass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_Textures"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
