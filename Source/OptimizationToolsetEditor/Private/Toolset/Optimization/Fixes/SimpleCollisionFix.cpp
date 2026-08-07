// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/SimpleCollisionFix.h"
#include "Toolset/Optimization/Fixes/FixUtils.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshEditorSubsystem.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SimpleCollisionFix"

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
	UStaticMesh* Mesh = OptimizationFixUtils::ResolveStaticMesh(Finding);
	UBodySetup* BodySetup = Mesh ? Mesh->GetBodySetup() : nullptr;
	UStaticMeshEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
	if (!Mesh || !BodySetup || !Subsystem || BodySetup->CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple)
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
		const int32 PrimitiveIndex = Subsystem->AddSimpleCollisionsWithNotification(Mesh, EScriptCollisionShapeType::Box, /*bApplyChanges*/ false);
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

#undef LOCTEXT_NAMESPACE
