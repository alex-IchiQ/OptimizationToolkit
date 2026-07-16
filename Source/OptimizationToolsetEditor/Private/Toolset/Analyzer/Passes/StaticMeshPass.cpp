// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/StaticMeshPass.h"
#include "Toolset/ToolsetCompat.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "StaticMeshPass"

void FStaticMeshPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	// Every problem this pass reports (collision, LODs, Nanite, triangles) is a
	// property of the mesh *asset*, not of the placed actor. De-dupe up front so
	// a mesh shared by hundreds of actors yields one finding, not hundreds.
	TSet<const UStaticMesh*> Reported;

	for (AStaticMeshActor* Actor : Context.StaticMeshActors)
	{
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!Mesh || Reported.Contains(Mesh))
		{
			continue;
		}
		Reported.Add(Mesh);

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
				FFinding F(TEXT("Mesh.ComplexCollision"), ESeverity::Minor, ECategory::Collision,
					LOCTEXT("ComplexCollisionTitle", "Complex (per-poly) collision used as simple"), Subject);
				F.WhyItMatters = LOCTEXT("ComplexCollisionWhy", "Per-poly collision is far more expensive to query than a primitive hull.");
				F.HowToFix = LOCTEXT("ComplexCollisionFix", "Add a simple collision primitive and switch the trace flag to Default.");
				F.TargetActor = Actor;
				F.FixId = TEXT("Fix_SimpleCollision");
				Out.Findings.Add(MoveTemp(F));
			}
		}

		// --- Excessive triangles without Nanite: a hard performance cliff.
		if (!bNanite && NumTris >= T.ExcessiveTriangles)
		{
			FFinding F(TEXT("Mesh.ExcessiveTriangles"), ESeverity::Critical, ECategory::Meshes,
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
			FFinding F(TEXT("Mesh.NaniteCandidate"), ESeverity::Major, ECategory::Meshes,
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
			FFinding F(TEXT("Mesh.MissingLODs"), ESeverity::Major, ECategory::Meshes,
				LOCTEXT("MissingLODTitle", "No LODs on a heavy non-Nanite mesh"), Subject);
			F.WhyItMatters = LOCTEXT("MissingLODWhy", "Without LODs the full triangle count is drawn at every distance.");
			F.HowToFix = LOCTEXT("MissingLODFix", "Enable Nanite, or generate an LOD chain (auto-LOD fix available).");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_GenerateLODs");
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

#undef LOCTEXT_NAMESPACE
