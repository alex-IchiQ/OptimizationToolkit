// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Optimization/IOptimizationFix.h"

/** Compresses a normal map as a normal map. Resolves Fix_NormalmapCompression. */
class FNormalmapCompressionFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_NormalmapCompression"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};

/** Stops a data texture being decoded as colour. Resolves Fix_DisableTextureSRGB. */
class FDisableTextureSRGBFix : public IOptimizationFix
{
public:
	virtual FName GetId() const override { return TEXT("Fix_DisableTextureSRGB"); }
	virtual FText GetLabel() const override;
	virtual bool IsSupported() const override;
	virtual bool Apply(const FFinding& Finding) const override;
};
