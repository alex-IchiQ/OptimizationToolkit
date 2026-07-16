// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/AnalyzePasses.h"
#include "Toolset/ToolsetCompat.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "AnalyzePasses"

void FStaticMeshPass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	// De-dupe by mesh asset: many actors can share one mesh; we only want to
	// report an asset-level problem (LODs, Nanite, tris) once.
	TSet<const UStaticMesh*> Reported;

	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		++Out.ActorsScanned;

		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}

		const FText Subject = FText::FromString(Mesh->GetName());
		const int32 NumTris = Mesh->GetNumTriangles(0);

#if OPTIMIZATION_HAS_NANITE
		const bool bNanite = Mesh->IsNaniteEnabled();
#else
		const bool bNanite = false;
#endif

		// --- Collision: per-poly on an otherwise cheap prop is a silent CPU cost.
		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			if (Body->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple)
			{
				FFinding F(ESeverity::Minor, ECategory::Collision,
					LOCTEXT("ComplexCollisionTitle", "Complex (per-poly) collision used as simple"), Subject);
				F.WhyItMatters = LOCTEXT("ComplexCollisionWhy", "Per-poly collision is far more expensive to query than a primitive hull.");
				F.HowToFix = LOCTEXT("ComplexCollisionFix", "Add a simple collision primitive and switch the trace flag to Default.");
				F.TargetActor = Actor;
				F.FixId = TEXT("Fix_SimpleCollision");
				Out.Findings.Add(MoveTemp(F));
			}
		}

		if (Reported.Contains(Mesh))
		{
			continue;
		}
		Reported.Add(Mesh);

		// --- Excessive triangles without Nanite: a hard performance cliff.
		if (!bNanite && NumTris >= T.ExcessiveTriangles)
		{
			FFinding F(ESeverity::Critical, ECategory::Meshes,
				LOCTEXT("ExcessiveTrisTitle", "Excessive triangles on a non-Nanite mesh"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("ExcessiveTrisWhy", "{0} triangles rendered without Nanite hammers the GPU and draw-call budget."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("ExcessiveTrisFix", "Enable Nanite, or add aggressive LODs / decimate the source mesh.");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_EnableNanite");
			Out.Findings.Add(MoveTemp(F));
		}
#if OPTIMIZATION_HAS_NANITE
		// --- Nanite candidate: heavy enough to benefit, but not turned on.
		else if (!bNanite && NumTris >= T.NaniteCandidateTriangles)
		{
			FFinding F(ESeverity::Major, ECategory::Meshes,
				LOCTEXT("NaniteCandidateTitle", "Nanite candidate not enabled"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("NaniteCandidateWhy", "At {0} triangles this mesh would render cheaper and self-LOD with Nanite."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("NaniteCandidateFix", "Enable Nanite on the static mesh (one-click fix available).");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_EnableNanite");
			Out.Findings.Add(MoveTemp(F));
		}
#endif

		// --- Missing LODs on a non-Nanite mesh: no distance falloff at all.
		if (!bNanite && Mesh->GetNumLODs() <= 1 && NumTris >= T.NaniteCandidateTriangles)
		{
			FFinding F(ESeverity::Major, ECategory::Meshes,
				LOCTEXT("MissingLODTitle", "No LODs on a heavy non-Nanite mesh"), Subject);
			F.WhyItMatters = LOCTEXT("MissingLODWhy", "Without LODs the full triangle count is drawn at every distance.");
			F.HowToFix = LOCTEXT("MissingLODFix", "Enable Nanite, or generate an LOD chain (auto-LOD fix available).");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_GenerateLODs");
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

void FLightingPass::Run(UWorld* World, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	int32 MovableLights = 0;
	AActor* FirstMovable = nullptr;

	for (TActorIterator<ALight> It(World); It; ++It)
	{
		ALight* Light = *It;
		++Out.ActorsScanned;

		ULightComponent* LC = Light->GetLightComponent();
		if (LC && LC->Mobility == EComponentMobility::Movable)
		{
			++MovableLights;
			if (!FirstMovable)
			{
				FirstMovable = Light;
			}
		}
	}

	if (MovableLights > T.MovableLightBudget)
	{
		FFinding F(ESeverity::Major, ECategory::Lighting, LOCTEXT("TooManyMovableTitle", "High number of movable (dynamic) lights"),
			FText::Format(LOCTEXT("MovableLightSubject", "{0} movable lights"), FText::AsNumber(MovableLights)));
		F.WhyItMatters = LOCTEXT("TooManyMovableWhy", "Every movable light adds dynamic shadow and lighting cost each frame.");
		F.HowToFix = LOCTEXT("TooManyMovableFix", "Set static geometry lights to Static/Stationary where dynamics aren't needed.");
		F.TargetActor = FirstMovable;
		F.FixId = TEXT("Fix_ReviewLightMobility");
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
