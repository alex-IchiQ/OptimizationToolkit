// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

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

	// Shipped defaults: the single source of truth for the property initializers,
	// ResetToDefaults(), and the Dashboard's per-row reset buttons.
	static constexpr bool  Default_bIncludeSubLevels        = true;
	static constexpr int32 Default_NaniteMinimumTriangles   = 2000;
	static constexpr int32 Default_NaniteCandidateTriangles = 20000;
	static constexpr int32 Default_ExcessiveTriangles       = 500000;
	static constexpr int32 Default_TextureDensityBudget     = 2048;
	static constexpr int32 Default_OversizedTextureSize     = 4096;
	static constexpr int32 Default_MaterialSlotBudget       = 8;
	static constexpr int32 Default_MaterialSamplerBudget    = 12;
	static constexpr int32 Default_MaterialInstructionBudget= 300;
	static constexpr int32 Default_MovableLightBudget       = 24;
	static constexpr int32 Default_LightmapResolutionBudget = 512;
	static constexpr int32 Default_InstancingCandidateCount = 10;
	static constexpr int32 Default_DependencyChainSizeMB    = 64;

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
	bool bIncludeSubLevels = Default_bIncludeSubLevels;

	/**
	 * Triangle count at or below which an enabled Nanite mesh is suggested for review.
	 * Set to zero to disable this check. Kept deliberately low because Nanite may
	 * still be worthwhile for heavily instanced assets.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="0", ClampMax="1000000", UIMin="0", UIMax="20000", DisplayName="Nanite Low-Poly Threshold"))
	int32 NaniteMinimumTriangles = Default_NaniteMinimumTriangles;

	/** Triangle count above which a non-Nanite static mesh is suggested for Nanite. */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="1000", ClampMax="10000000", UIMin="1000", UIMax="1000000"))
	int32 NaniteCandidateTriangles = Default_NaniteCandidateTriangles;

	/** Triangle count treated as a critical cost on a single non-Nanite mesh. */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="10000", ClampMax="100000000", UIMin="10000", UIMax="5000000"))
	int32 ExcessiveTriangles = Default_ExcessiveTriangles;

	/**
	 * Texels per metre of surface a texture may deliver before it is called oversized.
	 * 2048/m is already beyond typical AAA density (1024/m); the default is
	 * deliberately generous so the check only fires on the indefensible.
	 * Only used where the level's texture streaming data has been built.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Textures", meta=(ClampMin="128", ClampMax="16384", UIMin="512", UIMax="8192", DisplayName="Texture Density Budget (texels/m)"))
	int32 TextureDensityBudget = Default_TextureDensityBudget;

	/**
	 * Fallback size limit, used only when streaming data can't say how large a
	 * texture appears in the world. Judges the texture alone, so it can't tell an
	 * 8k skybox from an 8k bolt.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Textures", meta=(ClampMin="256", ClampMax="16384", UIMin="256", UIMax="8192"))
	int32 OversizedTextureSize = Default_OversizedTextureSize;

	/** Number of material slots allowed on a mesh before it is flagged for review. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="32"))
	int32 MaterialSlotBudget = Default_MaterialSlotBudget;

	/** Texture samplers a material may use before it is flagged. Most platforms hard-limit at 16. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="1", ClampMax="16", UIMin="4", UIMax="16"))
	int32 MaterialSamplerBudget = Default_MaterialSamplerBudget;

	/** Shader instructions a material may reach before it is flagged for review. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="50", ClampMax="10000", UIMin="100", UIMax="2000"))
	int32 MaterialInstructionBudget = Default_MaterialInstructionBudget;

	/** Number of movable lights allowed in each loaded level before per-light findings appear. */
	UPROPERTY(EditAnywhere, Config, Category="Lighting", meta=(ClampMin="0", ClampMax="512", UIMin="0", UIMax="128"))
	int32 MovableLightBudget = Default_MovableLightBudget;

	/** Lightmap resolution a single static mesh component may use before review. */
	UPROPERTY(EditAnywhere, Config, Category="Lighting", meta=(ClampMin="32", ClampMax="4096", UIMin="64", UIMax="2048"))
	int32 LightmapResolutionBudget = Default_LightmapResolutionBudget;

	/** Minimum compatible repeated actors required for an ISM/HISM recommendation. */
	UPROPERTY(EditAnywhere, Config, Category="Instancing", meta=(ClampMin="2", ClampMax="10000", UIMin="2", UIMax="100"))
	int32 InstancingCandidateCount = Default_InstancingCandidateCount;

	/** Disk size a Blueprint's hard-reference chain may reach before it is flagged. */
	UPROPERTY(EditAnywhere, Config, Category="Blueprints", meta=(ClampMin="1", ClampMax="100000", UIMin="8", UIMax="1024", DisplayName="Dependency Chain Size (MB)"))
	int32 DependencyChainSizeMB = Default_DependencyChainSizeMB;

	/** Restore every threshold to its shipped default and persist to config. */
	void ResetToDefaults()
	{
		bIncludeSubLevels = Default_bIncludeSubLevels;
		NaniteMinimumTriangles = Default_NaniteMinimumTriangles;
		NaniteCandidateTriangles = Default_NaniteCandidateTriangles;
		ExcessiveTriangles = Default_ExcessiveTriangles;
		TextureDensityBudget = Default_TextureDensityBudget;
		OversizedTextureSize = Default_OversizedTextureSize;
		MaterialSlotBudget = Default_MaterialSlotBudget;
		MaterialSamplerBudget = Default_MaterialSamplerBudget;
		MaterialInstructionBudget = Default_MaterialInstructionBudget;
		MovableLightBudget = Default_MovableLightBudget;
		LightmapResolutionBudget = Default_LightmapResolutionBudget;
		InstancingCandidateCount = Default_InstancingCandidateCount;
		DependencyChainSizeMB = Default_DependencyChainSizeMB;
		TryUpdateDefaultConfigFile();
	}
};
