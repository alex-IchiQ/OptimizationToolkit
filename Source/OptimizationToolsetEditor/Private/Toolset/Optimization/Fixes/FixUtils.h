// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

class UStaticMesh;

/** Resolves the static mesh asset behind a finding's target actor, or null. */
inline UStaticMesh* MeshFromFinding(const FFinding& Finding)
{
	AActor* Actor = Finding.TargetActor.Get();
	if (!Actor)
	{
		return nullptr;
	}
	UStaticMeshComponent* Comp = Actor->FindComponentByClass<UStaticMeshComponent>();
	return Comp ? Comp->GetStaticMesh() : nullptr;
}
