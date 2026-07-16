// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Optimization/OptimizationFixes.h"
#include "Toolset/ToolsetCompat.h"

#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshEditorSubsystem.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "OptimizationFixes"

namespace
{
	/** Resolves the static mesh asset behind a finding's target actor, or null. */
	UStaticMesh* MeshFromFinding(const FFinding& Finding)
	{
		AActor* Actor = Finding.TargetActor.Get();
		if (!Actor)
		{
			return nullptr;
		}
		UStaticMeshComponent* Comp = Actor->FindComponentByClass<UStaticMeshComponent>();
		return Comp ? Comp->GetStaticMesh() : nullptr;
	}
}

FText FEnableNaniteFix::GetLabel() const
{
	return LOCTEXT("EnableNaniteLabel", "Enable Nanite");
}

bool FEnableNaniteFix::IsSupported() const
{
#if OPTIMIZATION_HAS_NANITE
	return true;
#else
	return false;	// pre-5.0 engines have no Nanite
#endif
}

bool FEnableNaniteFix::Apply(const FFinding& Finding) const
{
#if OPTIMIZATION_HAS_NANITE
	UStaticMesh* Mesh = MeshFromFinding(Finding);
	if (!Mesh || Mesh->IsNaniteEnabled())
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("EnableNaniteTx", "Enable Nanite"));
	Mesh->Modify();
	Mesh->NaniteSettings.bEnabled = true;
	Mesh->PostEditChange();		// rebuilds the mesh with Nanite data
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

// ---------------------------------------------------------------------------
// Generate LODs
// ---------------------------------------------------------------------------
FText FGenerateLODsFix::GetLabel() const
{
	return LOCTEXT("GenLODsLabel", "Generate LODs");
}

bool FGenerateLODsFix::IsSupported() const
{
	// Needs the editor (mesh reduction runs editor-side). Always true in-editor.
	return GEditor != nullptr;
}

bool FGenerateLODsFix::Apply(const FFinding& Finding) const
{
	UStaticMesh* Mesh = MeshFromFinding(Finding);
	if (!Mesh)
	{
		return false;
	}

	UStaticMeshEditorSubsystem* Subsystem =
		GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("GenLODsTx", "Generate LODs"));
	Mesh->Modify();

	// A sensible default chain: LOD0 full, then aggressive auto-reduction.
	FStaticMeshReductionOptions Options;
	Options.bAutoComputeLODScreenSize = true;

	auto AddLod = [&Options](float PercentTriangles)
	{
		FStaticMeshReductionSettings Settings;
		Settings.PercentTriangles = PercentTriangles;
		Options.ReductionSettings.Add(Settings);
	};
	AddLod(1.00f);	// LOD0 — untouched
	AddLod(0.50f);	// LOD1
	AddLod(0.25f);	// LOD2
	AddLod(0.10f);	// LOD3

	Subsystem->SetLodsWithNotification(Mesh, Options, /*bApplyChanges*/ true);
	Mesh->MarkPackageDirty();
	return true;
}

// ---------------------------------------------------------------------------
// Simple collision
// ---------------------------------------------------------------------------
FText FSimpleCollisionFix::GetLabel() const
{
	return LOCTEXT("SimpleCollisionLabel", "Use Simple Collision");
}

bool FSimpleCollisionFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FSimpleCollisionFix::Apply(const FFinding& Finding) const
{
	UStaticMesh* Mesh = MeshFromFinding(Finding);
	UBodySetup* BodySetup = Mesh ? Mesh->GetBodySetup() : nullptr;
	UStaticMeshEditorSubsystem* Subsystem =
		GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
	if (!Mesh || !BodySetup || !Subsystem ||
		BodySetup->CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple)
	{
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SimpleCollisionTx", "Use Simple Collision"));
	Mesh->Modify();
	BodySetup->Modify();

	// Preserve existing authored primitives. If none exist, add a generated box
	// so switching away from per-poly collision cannot leave the mesh non-colliding.
	if (Subsystem->GetSimpleCollisionCount(Mesh) == 0)
	{
		const int32 PrimitiveIndex = Subsystem->AddSimpleCollisionsWithNotification(
			Mesh, EScriptCollisionShapeType::Box, /*bApplyChanges*/ false);
		if (PrimitiveIndex == INDEX_NONE)
		{
			Transaction.Cancel();
			return false;
		}
	}

	BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return true;
}

// ---------------------------------------------------------------------------
// Light mobility
// ---------------------------------------------------------------------------
FText FReviewLightMobilityFix::GetLabel() const
{
	return LOCTEXT("ReviewLightMobilityLabel", "Set Stationary");
}

bool FReviewLightMobilityFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FReviewLightMobilityFix::Apply(const FFinding& Finding) const
{
	ALight* Light = Cast<ALight>(Finding.TargetActor.Get());
	ULightComponent* LightComponent = Light ? Light->GetLightComponent() : nullptr;
	if (!Light || !LightComponent || LightComponent->Mobility != EComponentMobility::Movable)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ReviewLightMobilityTx", "Set Light Stationary"));
	Light->Modify();
	LightComponent->Modify();
	LightComponent->SetMobility(EComponentMobility::Stationary);
	Light->PostEditChange();
	Light->MarkPackageDirty();
	return true;
}

#undef LOCTEXT_NAMESPACE
