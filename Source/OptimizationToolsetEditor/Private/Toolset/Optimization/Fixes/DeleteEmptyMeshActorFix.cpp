// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/DeleteEmptyMeshActorFix.h"

#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DeleteEmptyMeshActorFix"

FText FDeleteEmptyMeshActorFix::GetLabel() const
{
	return LOCTEXT("Label", "Delete Actor");
}

bool FDeleteEmptyMeshActorFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FDeleteEmptyMeshActorFix::Apply(const FFinding& Finding) const
{
	AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Finding.TargetActor.Get());
	const UStaticMeshComponent* Component = Actor ? Actor->GetStaticMeshComponent() : nullptr;
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;

	// The level may have changed since the scan. Never delete an actor that has
	// since been assigned a mesh, or whose target is no longer valid.
	if (!GEditor || !Actor || !Component || Component->GetStaticMesh() || !World)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("Transaction", "Delete Empty Static Mesh Actor"));

	// Avoid leaving the editor selection pointing at an actor being destroyed.
	GEditor->SelectActor(Actor, /*bInSelected*/ false, /*bNotify*/ false);
	return World->EditorDestroyActor(Actor, /*bShouldModifyLevel*/ true);
}

#undef LOCTEXT_NAMESPACE
