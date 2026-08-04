// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Changes the movable light behind a finding to Stationary after user review. */
class FReviewLightMobilityFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_ReviewLightMobility"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
