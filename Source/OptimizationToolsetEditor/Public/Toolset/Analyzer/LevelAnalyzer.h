// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"

class AActor;

/**
 * Drives a read-only analysis of the active editor level by running every
 * registered IAnalyzePass. The passes do the actual inspection; this class
 * just gathers the world, iterates the registry, and sorts the results.
 */
class OPTIMIZATIONTOOLSETEDITOR_API FLevelAnalyzer
{
public:
	/** Runs every registered pass against the current editor world, skipping selected level packages. */
	static FScanResult AnalyzeCurrentLevel(const TSet<FName>& ExcludedLevelPackages);

	/** Focuses the given actor in the active viewport (select + frame). */
	static void FocusActor(TWeakObjectPtr<AActor> Actor);
};
