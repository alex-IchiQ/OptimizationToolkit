// Copyright Optimization Toolset. All Rights Reserved.

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

/** Builds an auto-reduced LOD chain for the mesh behind a finding. Resolves Fix_GenerateLODs. */
class FGenerateLODsFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_GenerateLODs"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};

/** Adds a box primitive when needed and stops using render triangles as simple collision. */
class FSimpleCollisionFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_SimpleCollision"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};

/** Changes the movable light behind a finding to Stationary after user review. */
class FReviewLightMobilityFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_ReviewLightMobility"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
