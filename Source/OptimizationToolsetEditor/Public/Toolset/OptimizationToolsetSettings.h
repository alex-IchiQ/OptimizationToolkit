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

	/** Triangle count above which a non-Nanite static mesh is suggested for Nanite. */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="1000", ClampMax="10000000", UIMin="1000", UIMax="1000000"))
	int32 NaniteCandidateTriangles = 20000;

	/** Triangle count treated as a critical cost on a single non-Nanite mesh. */
	UPROPERTY(EditAnywhere, Config, Category="Meshes", meta=(ClampMin="10000", ClampMax="100000000", UIMin="10000", UIMax="5000000"))
	int32 ExcessiveTriangles = 500000;

	/** Maximum effective texture dimension before an oversized-texture finding is emitted. */
	UPROPERTY(EditAnywhere, Config, Category="Textures", meta=(ClampMin="256", ClampMax="16384", UIMin="256", UIMax="8192"))
	int32 OversizedTextureSize = 4096;

	/** Number of material slots allowed on a mesh before it is flagged for review. */
	UPROPERTY(EditAnywhere, Config, Category="Materials", meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="32"))
	int32 MaterialSlotBudget = 8;

	/** Number of movable lights allowed in the current level before per-light findings appear. */
	UPROPERTY(EditAnywhere, Config, Category="Lighting", meta=(ClampMin="0", ClampMax="512", UIMin="0", UIMax="128"))
	int32 MovableLightBudget = 24;

	/** Minimum compatible repeated actors required for an ISM/HISM recommendation. */
	UPROPERTY(EditAnywhere, Config, Category="Instancing", meta=(ClampMin="2", ClampMax="10000", UIMin="2", UIMax="100"))
	int32 InstancingCandidateCount = 10;
};
