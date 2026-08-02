// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One loaded texture (or render target) in the memory report. */
struct FMemoryTextureRow
{
	FString Name;
	FString Path;
	int32 SizeX = 0;
	int32 SizeY = 0;
	FString Format;
	int32 NumMips = 0;
	int32 ResidentMips = 0;
	int64 FullBytes = 0;		// every mip
	int64 ResidentBytes = 0;	// what is actually in memory now
	FString Role;				// Diffuse / Normal / UI / Lightmap / Other
	FString StreamState;		// Stream N/M · NeverStream · Virtual
};

/** One loaded mesh in the memory report. */
struct FMemoryMeshRow
{
	FString Name;
	FString Path;
	FString Type;		// Nanite / Static / Skeletal
	int64 Bytes = 0;
	int32 Triangles = 0;
	int32 LODs = 0;
};

/** One texture role's rolled-up memory, for the summary line. */
struct FMemoryRoleTotal
{
	FString Role;
	int64 FullBytes = 0;
	int32 Count = 0;
};

/**
 * What the loaded content is actually costing in memory right now — the GPU-side
 * view the disk-size Dashboard can't give.
 *
 * Walks loaded textures, render targets and meshes, sums their resource sizes,
 * and reads the RHI's own texture-memory and streaming-pool figures so the
 * plugin's tracked total can be compared against what the driver reports.
 */
struct FMemoryReport
{
	// RHI / driver figures (-1 or 0 when the RHI can't report them).
	int64 DedicatedVideoMemory = 0;
	int64 TexturePoolSize = 0;
	int64 StreamingPoolBudget = 0;
	int64 StreamingPoolUsed = 0;
	int64 StreamingOverBudget = 0;

	// Tracked totals.
	int64 TextureFullTotal = 0;
	int64 TextureResidentTotal = 0;
	int64 RenderTargetTotal = 0;
	int64 MeshTotal = 0;
	int64 GrandTotal = 0;	// full texture + render target + mesh

	TArray<FMemoryRoleTotal> TextureRoles;	// largest first
	TArray<FMemoryTextureRow> Textures;		// largest first, trimmed to TopN
	TArray<FMemoryTextureRow> RenderTargets;
	TArray<FMemoryMeshRow> Meshes;

	double ComputeSeconds = 0.0;

	/** Walks loaded content and reads RHI stats. TopN caps each list length. */
	static FMemoryReport Compute(int32 TopN = 200);
};
