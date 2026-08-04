// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Buckets raw asset classes into the handful of groups a dev actually thinks in.
 *
 * A project has dozens of asset classes (Texture2D, TextureCube, SoundWave,
 * MetaSoundSource, MaterialInstanceConstant...). Charting them raw produces a
 * long tail that has to be truncated, which hides part of the project in the one
 * view whose whole job is showing all of it. Grouping keeps the legend short and
 * lets Other honestly absorb the rest.
 */
enum class EAssetCategory : uint8
{
	Textures,
	StaticMeshes,
	SkeletalMeshes,
	Materials,
	Animations,
	Audio,
	Blueprints,
	Levels,
	Other,
	Count
};

/** On-disk footprint of every package belonging to one category. */
struct FProjectSizeEntry
{
	EAssetCategory Category = EAssetCategory::Other;
	int64 TotalBytes = 0;
	int32 PackageCount = 0;
};

/**
 * Where the project's disk footprint actually goes, grouped by asset category.
 *
 * Read-only and unrelated to the level scan: this measures package files under
 * /Game rather than anything placed in a map. It is deliberately not an
 * ICleanupAction — actions report a one-line summary, this needs a breakdown.
 */
struct FProjectSizeReport
{
	/** One entry per category that has any content, sorted largest first. */
	TArray<FProjectSizeEntry> Entries;

	int64 TotalBytes = 0;
	int32 PackageCount = 0;
	double ComputeSeconds = 0.0;

	/** True when the asset registry was still scanning and the numbers would lie. */
	bool bRegistryIncomplete = false;

	/** Walks the asset registry under /Game and measures each package on disk. */
	static FProjectSizeReport Compute();

	/** Display name for a category, e.g. "Skeletal meshes". */
	static FText LabelForCategory(EAssetCategory Category);

	/** Buckets an asset class name into its category (Other for anything unlisted). */
	static EAssetCategory CategoryForClass(FName ClassName);
};
