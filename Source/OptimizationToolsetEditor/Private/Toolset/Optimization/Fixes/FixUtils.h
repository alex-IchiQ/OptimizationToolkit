// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace OptimizationFixUtils
{
	/** Resolves the explicit mesh asset first, then falls back to its actor. */
	inline UStaticMesh* ResolveStaticMesh(const FFinding& Finding)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Finding.TargetAsset.Get()))
		{
			return Mesh;
		}

		const AActor* Actor = Finding.TargetActor.Get();
		if (!Actor)
		{
			return nullptr;
		}
		
		const UStaticMeshComponent* Comp = Actor->FindComponentByClass<UStaticMeshComponent>();
		return Comp ? Comp->GetStaticMesh() : nullptr;
	}
}
