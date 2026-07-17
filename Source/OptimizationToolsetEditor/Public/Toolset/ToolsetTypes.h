// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AActor;

/**
 * Plain (non-reflected) enums: nothing here is exposed to Blueprint or stored
 * in a UPROPERTY, so we skip UENUM. If a settings UObject later needs these,
 * reintroduce UENUM and qualify the names (e.g. EOptimizationSeverity) since a
 * reflected type must be globally unique across the whole editor.
 */

/** How severe a finding is. Drives sorting and the colour stripe in the UI. */
enum class ESeverity : uint8
{
	Critical,	// Ships broken / huge perf cliff. Fix before milestone.
	Major,		// Notable, worth fixing soon.
	Minor,		// Hygiene / polish.
	Good		// Informational "all clear" entry.
};

/** Which optimization domain a finding belongs to. Used by the category filter. */
enum class ECategory : uint8
{
	Meshes,
	Materials,
	Textures,
	Lighting,
	Collision,
	Blueprints,
	Project,
	Count	// iteration bound; never assigned to a finding
};

/**
 * A single issue produced by an analyzer pass.
 *
 * Kept as a plain struct (not a UObject) so passes can create thousands
 * cheaply; the owning actor is held as a weak pointer so findings never
 * keep an actor alive or dangle after a level change.
 */
struct FFinding
{
	/**
	 * Stable, non-localized id of the *kind* of problem, e.g. "Mesh.MissingLODs".
	 * Title is display text and localizable, so it can't identify a type; this can.
	 * Used to group repeats when scoring, and later for report exports.
	 */
	FName TypeId = NAME_None;

	ESeverity Severity = ESeverity::Minor;
	ECategory Category = ECategory::Meshes;

	/** Short one-line title, e.g. "Nanite candidate not enabled". */
	FText Title;

	/** Which asset/actor this is about, e.g. "SM_Rock_04". */
	FText Subject;

	/** Why this matters for performance (one sentence). */
	FText WhyItMatters;

	/** Concrete suggested fix (one sentence). */
	FText HowToFix;

	/** Actor to focus in the viewport when the user clicks the row (optional). */
	TWeakObjectPtr<AActor> TargetActor;

	/**
	 * The asset this finding is really about, when it is an asset rather than a
	 * placed actor (a texture, a material). TargetActor only says where to look
	 * in the viewport; a fix that edits the asset needs the asset itself, and for
	 * something like a texture there is no path from the actor back to it.
	 */
	TWeakObjectPtr<UObject> TargetAsset;

	/**
	 * Every actor this finding covers, when it describes a group rather than a
	 * single offender (instancing candidates, for one). TargetActor is just the
	 * one we frame; a fix that rewrites the group needs all of them, and
	 * re-deriving the grouping inside the fix would duplicate the pass's rules.
	 * Empty for findings about a single actor or an asset.
	 */
	TArray<TWeakObjectPtr<AActor>> RelatedActors;

	/** Stable id for the auto-fix that resolves this finding, or NAME_None. */
	FName FixId = NAME_None;

	/**
	 * Which sub-level the actor lives in, when it isn't the persistent one.
	 *
	 * A scan walks every loaded sub-level, so on a streamed map "SM_Rock_04" alone
	 * doesn't say where to look. Left empty for the persistent level and for
	 * asset-only findings, where naming it would be noise. Set by FLevelAnalyzer
	 * from TargetActor, so a pass never has to remember to fill it in.
	 */
	FText LevelName;

	FFinding() = default;

	FFinding(FName InTypeId, ESeverity InSeverity, ECategory InCategory, FText InTitle, FText InSubject)
		: TypeId(InTypeId)
		, Severity(InSeverity)
		, Category(InCategory)
		, Title(MoveTemp(InTitle))
		, Subject(MoveTemp(InSubject))
	{
	}
};

/**
 * Level-wide counts, gathered during the same walk that feeds the passes.
 *
 * These are not findings — nothing here is a problem on its own. They are the
 * scale of the level, so a user can see what they are working with before any
 * severity judgement, and watch the numbers move as fixes land.
 *
 * Geometry is deliberately static-mesh only: skeletal triangle counts live in
 * render data that a cooked/streamed asset may not have loaded, and a number
 * that silently reads zero is worse than one with a stated scope. The Dashboard
 * tooltips say as much.
 */
struct FLevelStats
{
	/** Every actor in the level. */
	int32 Actors = 0;

	/** Distinct static mesh *assets* placed, however many actors use each. */
	int32 Meshes = 0;

	/** LOD0 triangles across every placed instance, so an ISM counts per instance. */
	int64 Triangles = 0;

	/** Distinct non-engine material interfaces assigned on mesh components. */
	int32 Materials = 0;

	/** Light components, including those on Blueprint actors. */
	int32 Lights = 0;
};

/** Tunable limits shared by analyze passes. Defaults suit typical game levels;
 *  a settings panel can override them later. */
struct FAnalyzeThresholds
{
	int32 NaniteCandidateTriangles = 20000;	// tris above which Nanite is worth it
	int32 ExcessiveTriangles = 500000;		// single mesh that is simply too heavy
	int32 OversizedTextureSize = 4096;		// px on longest side (fallback rule)
	int32 TextureDensityBudget = 2048;		// texels per metre of surface (preferred rule)
	int32 MaterialSlotBudget = 8;			// mesh sections/material slots before review
	int32 MaterialSamplerBudget = 12;		// texture samplers before review (hard limit is 16)
	int32 MaterialInstructionBudget = 300;	// shader instructions before review
	int32 MovableLightBudget = 24;			// dynamic lights per level before we warn
	int32 LightmapResolutionBudget = 512;	// lightmap res on one component before we warn
	int32 InstancingCandidateCount = 10;	// compatible repeated actors worth batching
	int32 DependencyChainSizeMB = 64;		// disk a Blueprint may drag in before we warn
};

/** Aggregate result of a full analyze pass. */
struct FScanResult
{
	TArray<FFinding> Findings;
	double ScanSeconds = 0.0;
	int32 ActorsScanned = 0;

	/** Scale of the level the findings came from. */
	FLevelStats Stats;

	int32 CountBySeverity(ESeverity Severity) const
	{
		int32 Count = 0;
		for (const FFinding& F : Findings)
		{
			if (F.Severity == Severity)
			{
				++Count;
			}
		}
		return Count;
	}

	/** What one finding of this severity subtracts from the score. */
	static int32 SeverityPenalty(ESeverity Severity)
	{
		switch (Severity)
		{
		case ESeverity::Critical: return 12;
		case ESeverity::Major:    return 5;
		case ESeverity::Minor:    return 1;
		default:                  return 0;
		}
	}

	/** The most a single finding *type* may subtract, however often it repeats. */
	static int32 SeverityPenaltyCap(ESeverity Severity)
	{
		switch (Severity)
		{
		case ESeverity::Critical: return 25;
		case ESeverity::Major:    return 15;
		case ESeverity::Minor:    return 6;
		default:                  return 0;
		}
	}

	/**
	 * 0-100 health score: starts at 100, minus a penalty per finding type.
	 *
	 * Penalties are grouped by TypeId and capped per type, because a level with
	 * 200 movable lights has one problem repeated 200 times, not 200 problems.
	 * Without the cap any real level pins to 0 and the number stops meaning
	 * anything. The cap scales with the worst severity seen for that type.
	 */
	int32 HealthScore() const
	{
		struct FTypeTally
		{
			int32 Penalty = 0;
			int32 Cap = 0;
		};

		TMap<FName, FTypeTally> ByType;
		for (const FFinding& F : Findings)
		{
			FTypeTally& Tally = ByType.FindOrAdd(F.TypeId);
			Tally.Penalty += SeverityPenalty(F.Severity);
			Tally.Cap = FMath::Max(Tally.Cap, SeverityPenaltyCap(F.Severity));
		}

		int32 Total = 0;
		for (const TPair<FName, FTypeTally>& Pair : ByType)
		{
			Total += FMath::Min(Pair.Value.Penalty, Pair.Value.Cap);
		}
		return FMath::Clamp(100 - Total, 0, 100);
	}
};
