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
	Project
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

	/** Stable id for the auto-fix that resolves this finding, or NAME_None. */
	FName FixId = NAME_None;

	FFinding() = default;

	FFinding(ESeverity InSeverity, ECategory InCategory, FText InTitle, FText InSubject)
		: Severity(InSeverity)
		, Category(InCategory)
		, Title(MoveTemp(InTitle))
		, Subject(MoveTemp(InSubject))
	{
	}
};

/** Tunable limits shared by analyze passes. Defaults suit typical game levels;
 *  a settings panel can override them later. */
struct FAnalyzeThresholds
{
	int32 NaniteCandidateTriangles = 20000;	// tris above which Nanite is worth it
	int32 ExcessiveTriangles = 500000;		// single mesh that is simply too heavy
	int32 OversizedTextureSize = 4096;		// px on longest side
	int32 MaterialSlotBudget = 8;			// mesh sections/material slots before review
	int32 MovableLightBudget = 24;			// dynamic lights per level before we warn
	int32 InstancingCandidateCount = 10;	// compatible repeated actors worth batching
};

/** Aggregate result of a full analyze pass. */
struct FScanResult
{
	TArray<FFinding> Findings;
	double ScanSeconds = 0.0;
	int32 ActorsScanned = 0;

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

	/** 0-100 health score: starts at 100, weighted penalty per finding. */
	int32 HealthScore() const
	{
		const int32 Penalty =
			CountBySeverity(ESeverity::Critical) * 12 +
			CountBySeverity(ESeverity::Major) * 5 +
			CountBySeverity(ESeverity::Minor) * 1;
		return FMath::Clamp(100 - Penalty, 0, 100);
	}
};
