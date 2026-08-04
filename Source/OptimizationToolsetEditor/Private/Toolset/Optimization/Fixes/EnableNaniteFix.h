// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Turns Nanite on for the static mesh behind a finding. Resolves Fix_EnableNanite. */
class FEnableNaniteFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_EnableNanite"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
