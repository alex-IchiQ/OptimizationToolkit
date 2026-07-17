// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "OptimizationToolsetSettings.generated.h"

/** Project-wide thresholds used by Optimization Toolset analyze passes. */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Optimization Toolset"))
class OPTIMIZATIONTOOLSETEDITOR_API UOptimizationToolsetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * Include newly discovered loaded sub-levels in the Dashboard scan scope.
	 *
	 * On by default: a streamed map's problems are in its sub-levels, and a scan
	 * that quietly ignored them would report a clean bill of health for a level
	 * that never had any actors of its own. Turn it off to review one level at a
	 * time. Each level can then be included or excluded from the Dashboard without
	 * changing this project default. Unloaded sub-levels are never scanned.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Scan")
	bool bIncludeSubLevels = true;

	/**
	 * Triangle count at or below which an enabled Nanite mesh is suggested for review.
	 * Set to zero to disable this check. Kept deliberately low because Nanite may
	 * still be worthwhile for heavily instanced assets.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="0", ClampMax="1000000", UIMin="0", UIMax="20000", DisplayName="Nanite Low-Poly Threshold"))
	int32 NaniteMinimumTriangles = 2000;

	/** Triangle count above which a non-Nanite static mesh is suggested for Nanite. */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="1000", ClampMax="10000000", UIMin="1000", UIMax="1000000"))
	int32 NaniteCandidateTriangles = 20000;

	/** Triangle count treated as a critical cost on a single non-Nanite mesh. */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="10000", ClampMax="100000000", UIMin="10000", UIMax="5000000"))
	int32 ExcessiveTriangles = 500000;

	/**
	 * Texels per metre of surface a texture may deliver before it is called oversized.
	 * 2048/m is already beyond typical AAA density (1024/m); the default is
	 * deliberately generous so the check only fires on the indefensible.
	 * Only used where the level's texture streaming data has been built.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Textures", meta=(ClampMin="128", ClampMax="16384", UIMin="512", UIMax="8192", DisplayName="Texture Density Budget (texels/m)"))
	int32 TextureDensityBudget = 2048;

	/**
	 * Fallback size limit, used only when streaming data can't say how large a
	 * texture appears in the world. Judges the texture alone, so it can't tell an
	 * 8k skybox from an 8k bolt.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Textures", meta=(ClampMin="256", ClampMax="16384", UIMin="256", UIMax="8192"))
	int32 OversizedTextureSize = 4096;

	/** Number of material slots allowed on a mesh before it is flagged for review. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="32"))
	int32 MaterialSlotBudget = 8;

	/** Texture samplers a material may use before it is flagged. Most platforms hard-limit at 16. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="1", ClampMax="16", UIMin="4", UIMax="16"))
	int32 MaterialSamplerBudget = 12;

	/** Shader instructions a material may reach before it is flagged for review. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="50", ClampMax="10000", UIMin="100", UIMax="2000"))
	int32 MaterialInstructionBudget = 300;

	/** Number of movable lights allowed in the current level before per-light findings appear. */
	UPROPERTY(EditAnywhere, Config, Category="Lighting", meta=(ClampMin="0", ClampMax="512", UIMin="0", UIMax="128"))
	int32 MovableLightBudget = 24;

	/** Lightmap resolution a single static mesh component may use before review. */
	UPROPERTY(EditAnywhere, Config, Category="Lighting", meta=(ClampMin="32", ClampMax="4096", UIMin="64", UIMax="2048"))
	int32 LightmapResolutionBudget = 512;

	/** Minimum compatible repeated actors required for an ISM/HISM recommendation. */
	UPROPERTY(EditAnywhere, Config, Category="Instancing", meta=(ClampMin="2", ClampMax="10000", UIMin="2", UIMax="100"))
	int32 InstancingCandidateCount = 10;

	/** Disk size a Blueprint's hard-reference chain may reach before it is flagged. */
	UPROPERTY(EditAnywhere, Config, Category="Blueprints", meta=(ClampMin="1", ClampMax="100000", UIMin="8", UIMax="1024", DisplayName="Dependency Chain Size (MB)"))
	int32 DependencyChainSizeMB = 64;
};
