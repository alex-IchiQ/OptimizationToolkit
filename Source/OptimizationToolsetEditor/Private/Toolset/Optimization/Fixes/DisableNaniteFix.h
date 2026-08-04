// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Turns Nanite off for a static mesh when a finding requests it. Resolves Fix_DisableNanite. */
class FDisableNaniteFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_DisableNanite"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
