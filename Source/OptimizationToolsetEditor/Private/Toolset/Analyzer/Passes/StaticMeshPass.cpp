// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Analyzer/Passes/StaticMeshPass.h"
#include "Toolset/ToolsetCompat.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "StaticMeshPass"

void FStaticMeshPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TSet<const UStaticMesh*> NaniteMaterialIncompatibilities;

#if OPTIMIZATION_HAS_NANITE
	// Match the engine warning against each component's effective materials, not
	// only the mesh defaults: a placed actor can introduce an incompatible
	// translucent override. One finding per mesh is enough because the requested
	// fix disables Nanite on that asset for every use.
	for (AActor* Actor : Context.Actors)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
		for (UStaticMeshComponent* Component : Components)
		{
			UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
			if (!Mesh || !Mesh->IsNaniteEnabled() || Component->IsDisallowNanite()
				|| NaniteMaterialIncompatibilities.Contains(Mesh))
			{
				continue;
			}

			const int32 MaterialCount = Component->GetNumMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
				if (!Material)
				{
					continue;
				}

				const EBlendMode BlendMode = Material->GetBlendMode();
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					continue;
				}

				NaniteMaterialIncompatibilities.Add(Mesh);
				FFinding F(TEXT("Mesh.NaniteUnsupportedMaterial"), ESeverity::Major, ECategory::Meshes, EFindingScope::Asset,
					LOCTEXT("NaniteUnsupportedMaterialTitle", "Nanite mesh uses an unsupported material"),
					FText::Format(LOCTEXT("NaniteUnsupportedMaterialSubject", "{0} — {1}"),
						FText::FromString(Mesh->GetName()), FText::FromString(Material->GetName())));
				F.WhyItMatters = LOCTEXT("NaniteUnsupportedMaterialWhy", "Nanite currently accepts only Opaque or Masked materials. Other blend modes force fallback rendering and repeatedly emit LogStaticMesh warnings.");
				F.HowToFix = LOCTEXT("NaniteUnsupportedMaterialFix", "Disable Nanite on the mesh asset, or replace the material with an Opaque or Masked alternative.");
				F.TargetActor = Actor;
				F.TargetAsset = Mesh;
				F.FixId = TEXT("Fix_DisableNanite");
				Out.Findings.Add(MoveTemp(F));
				break;
			}
		}
	}
#endif

	// Every problem this pass reports (collision, LODs, Nanite, triangles) is a
	// property of the mesh *asset*, not of the placed actor. De-dupe up front so
	// a mesh shared by hundreds of actors yields one finding, not hundreds.
	TSet<const UStaticMesh*> Reported;

	for (AStaticMeshActor* Actor : Context.StaticMeshActors)
	{
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;

		// --- No mesh at all: the actor costs a transform and renders nothing.
		//
		// Per actor, not per asset: there is no asset to de-dupe by, and each one
		// is its own mistake. Deliberately limited to AStaticMeshActor rather than
		// any component with a null mesh — a Blueprint's StaticMeshComponent is
		// routinely left empty and filled in at runtime, so flagging those would
		// report working code as broken.
		if (!Mesh)
		{
			FFinding F(TEXT("Mesh.EmptyMesh"), ESeverity::Minor, ECategory::Meshes, EFindingScope::Actor,
				LOCTEXT("EmptyMeshTitle", "Static mesh actor has no mesh assigned"), FText::FromString(Actor->GetActorNameOrLabel()));
			F.WhyItMatters = LOCTEXT("EmptyMeshWhy", "The actor still loads, ticks its transform and takes up a slot in the level, but draws nothing.");
			F.HowToFix = LOCTEXT("EmptyMeshFix", "Assign the intended mesh, or delete the actor if it is left over from an earlier setup.");
			F.TargetActor = Actor;
			F.FixId = TEXT("Fix_DeleteEmptyMeshActor");
			Out.Findings.Add(MoveTemp(F));
			continue;
		}

		if (Reported.Contains(Mesh))
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
				FFinding F(TEXT("Mesh.ComplexCollision"), ESeverity::Minor, ECategory::Collision, EFindingScope::Asset,
					LOCTEXT("ComplexCollisionTitle", "Complex (per-poly) collision used as simple"), Subject);
				F.WhyItMatters = LOCTEXT("ComplexCollisionWhy", "Per-poly collision is far more expensive to query than a primitive hull.");
				F.HowToFix = LOCTEXT("ComplexCollisionFix", "Add a simple collision primitive and switch the trace flag to Default.");
				F.TargetActor = Actor;
				F.TargetAsset = Mesh;
				F.FixId = TEXT("Fix_SimpleCollision");
				Out.Findings.Add(MoveTemp(F));
			}
		}

		// --- Excessive triangles without Nanite: a hard performance cliff.
		if (!bNanite && NumTris >= T.ExcessiveTriangles)
		{
			FFinding F(TEXT("Mesh.ExcessiveTriangles"), ESeverity::Critical, ECategory::Meshes, EFindingScope::Asset,
				LOCTEXT("ExcessiveTrisTitle", "Excessive triangles on a non-Nanite mesh"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("ExcessiveTrisWhy", "{0} triangles rendered without Nanite hammers the GPU and draw-call budget."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("ExcessiveTrisFix", "Enable Nanite, or add aggressive LODs / decimate the source mesh.");
			F.TargetActor = Actor;
			F.TargetAsset = Mesh;
			F.FixId = TEXT("Fix_EnableNanite");
			Out.Findings.Add(MoveTemp(F));
		}
#if OPTIMIZATION_HAS_NANITE
		// --- Nanite candidate: heavy enough to benefit, but not turned on.
		else if (!bNanite && NumTris >= T.NaniteCandidateTriangles)
		{
			FFinding F(TEXT("Mesh.NaniteCandidate"), ESeverity::Major, ECategory::Meshes, EFindingScope::Asset,
				LOCTEXT("NaniteCandidateTitle", "Nanite candidate not enabled"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("NaniteCandidateWhy", "At {0} triangles this mesh would render cheaper and self-LOD with Nanite."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("NaniteCandidateFix", "Enable Nanite on the static mesh (one-click fix available).");
			F.TargetActor = Actor;
			F.TargetAsset = Mesh;
			F.FixId = TEXT("Fix_EnableNanite");
			Out.Findings.Add(MoveTemp(F));
		}

		// --- Nanite overhead on tiny geometry can outweigh the work it removes.
		// This stays a Minor review item: very high instance counts or a deliberate
		// all-Nanite content pipeline can still make the setting reasonable.
		if (bNanite && !NaniteMaterialIncompatibilities.Contains(Mesh)
			&& T.NaniteMinimumTriangles > 0 && NumTris <= T.NaniteMinimumTriangles)
		{
			FFinding F(TEXT("Mesh.LowPolyNanite"), ESeverity::Minor, ECategory::Meshes, EFindingScope::Asset,
				LOCTEXT("LowPolyNaniteTitle", "Nanite enabled on a low-poly mesh"), Subject);
			F.WhyItMatters = FText::Format(
				LOCTEXT("LowPolyNaniteWhy", "This mesh has only {0} triangles, so Nanite's cluster data, build time, and streaming overhead may outweigh its geometry savings."),
				FText::AsNumber(NumTris));
			F.HowToFix = LOCTEXT("LowPolyNaniteFix", "Disable Nanite unless high instance counts or a deliberate Nanite-only pipeline justify keeping it enabled.");
			F.TargetActor = Actor;
			F.TargetAsset = Mesh;
			F.FixId = TEXT("Fix_DisableNanite");
			Out.Findings.Add(MoveTemp(F));
		}
#endif

		// --- Missing LODs on a non-Nanite mesh: no distance falloff at all.
		if (!bNanite && Mesh->GetNumLODs() <= 1 && NumTris >= T.NaniteCandidateTriangles)
		{
			FFinding F(TEXT("Mesh.MissingLODs"), ESeverity::Major, ECategory::Meshes, EFindingScope::Asset,
				LOCTEXT("MissingLODTitle", "No LODs on a heavy non-Nanite mesh"), Subject);
			F.WhyItMatters = LOCTEXT("MissingLODWhy", "Without LODs the full triangle count is drawn at every distance.");
			F.HowToFix = LOCTEXT("MissingLODFix", "Enable Nanite, or generate an LOD chain (auto-LOD fix available).");
			F.TargetActor = Actor;
			F.TargetAsset = Mesh;
			F.FixId = TEXT("Fix_GenerateLODs");
			Out.Findings.Add(MoveTemp(F));
		}
	}
}

#undef LOCTEXT_NAMESPACE
