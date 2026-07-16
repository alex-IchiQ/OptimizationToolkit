// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/** Static-mesh hygiene: excessive triangles, Nanite candidates, missing LODs, per-poly collision. */
class FStaticMeshPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_StaticMesh"); }
	virtual void Run(UWorld* World, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};

/** Texture hygiene for assets actually referenced by primitive components in the level. */
class FTexturePass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_Textures"); }
	virtual void Run(UWorld* World, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};

/** Material and section-layout hygiene for mesh components used in the level. */
class FMaterialPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_Materials"); }
	virtual void Run(UWorld* World, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};

/** Lighting hygiene: too many movable (dynamic) lights. */
class FLightingPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_Lighting"); }
	virtual void Run(UWorld* World, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};

/** Finds conservative groups of repeated static-mesh actors suitable for ISM/HISM review. */
class FInstancingCandidatePass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_InstancingCandidates"); }
	virtual void Run(UWorld* World, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
