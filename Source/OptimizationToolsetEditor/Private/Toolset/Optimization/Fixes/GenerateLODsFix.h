// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Builds an auto-reduced LOD chain for the mesh behind a finding. Resolves Fix_GenerateLODs. */
class FGenerateLODsFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_GenerateLODs"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
