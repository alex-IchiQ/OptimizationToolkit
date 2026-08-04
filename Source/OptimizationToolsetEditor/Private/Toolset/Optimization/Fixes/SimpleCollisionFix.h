// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Adds a box primitive when needed and stops using render triangles as simple collision. */
class FSimpleCollisionFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_SimpleCollision"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
