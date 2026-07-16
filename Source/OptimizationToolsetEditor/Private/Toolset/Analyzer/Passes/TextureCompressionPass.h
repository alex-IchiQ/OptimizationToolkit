// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/**
 * Texture compression and colour space, judged by what the texture actually does.
 *
 * The engine already fixes the mismatches it can see: set a texture to Normalmap
 * compression and it turns sRGB off for you. What it cannot know is a texture's
 * *role* — a normal map left on Default compression, or a roughness map with
 * sRGB on, both look like ordinary colour textures to it.
 *
 * So ask the materials instead: whatever feeds the Normal input is a normal map,
 * whatever feeds Roughness/Metallic/AO/Specular is data. That is a fact about the
 * project, not a guess from a filename suffix.
 */
class FTextureCompressionPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_TextureCompression"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
