// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class AStaticMeshActor;
class ALight;

/**
 * One shared walk of the level, handed to every pass.
 *
 * Passes used to each run their own TActorIterator, so a scan walked the whole
 * world once per pass. FLevelAnalyzer now gathers the actors a single time and
 * pre-buckets the common types; passes just iterate the cached arrays.
 *
 * Add a bucket here when a new pass would otherwise need its own world walk.
 */
struct FLevelScanContext
{
	UWorld* World = nullptr;

	/** Every actor in the level, in iteration order. */
	TArray<AActor*> Actors;

	/** Pre-filtered subsets of Actors, for the types passes ask for most. */
	TArray<AStaticMeshActor*> StaticMeshActors;
	TArray<ALight*> Lights;
};
