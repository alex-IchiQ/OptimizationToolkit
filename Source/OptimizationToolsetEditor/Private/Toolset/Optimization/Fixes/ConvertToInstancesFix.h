// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/**
 * Replaces a group of repeated static-mesh actors with one actor carrying a
 * hierarchical instanced mesh component. Resolves Fix_ConvertToInstances.
 *
 * Operates on FFinding::RelatedActors — the group the instancing pass already
 * vetted — and re-checks compatibility before touching anything, since the
 * level can change between the scan and the click.
 */
class FConvertToInstancesFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_ConvertToInstances"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
