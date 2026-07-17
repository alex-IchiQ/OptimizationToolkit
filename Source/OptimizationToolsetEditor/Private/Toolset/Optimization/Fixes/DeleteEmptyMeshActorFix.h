// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Deletes the empty static mesh actor behind a finding. */
class FDeleteEmptyMeshActorFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_DeleteEmptyMeshActor"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
