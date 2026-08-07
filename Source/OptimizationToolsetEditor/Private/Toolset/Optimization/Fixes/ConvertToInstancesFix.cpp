// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/ConvertToInstancesFix.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "ConvertToInstancesFix"

namespace
{
	bool ComponentsMatch(const AStaticMeshActor& Candidate, const AStaticMeshActor& TemplateActor)
	{
		const UStaticMeshComponent* Component = Candidate.GetStaticMeshComponent();
		const UStaticMeshComponent* Template = TemplateActor.GetStaticMeshComponent();
		if (!Component || !Template || Candidate.GetClass() != AStaticMeshActor::StaticClass()
			|| Candidate.GetWorld() != TemplateActor.GetWorld()
			|| Candidate.GetLevel() != TemplateActor.GetLevel()
			|| Candidate.GetFolderPath() != TemplateActor.GetFolderPath()
			|| Candidate.IsHidden() != TemplateActor.IsHidden()
			|| Candidate.IsHiddenEd() != TemplateActor.IsHiddenEd()
			|| Candidate.GetAttachParentActor() != nullptr
			|| !Component->GetAttachChildren().IsEmpty()
			|| !Candidate.Tags.IsEmpty() || !Component->ComponentTags.IsEmpty()
			|| Component->Mobility != EComponentMobility::Static
			|| Component->GetStaticMesh() != Template->GetStaticMesh()
			|| Component->GetCollisionProfileName() != Template->GetCollisionProfileName()
			|| Component->GetCollisionEnabled() != Template->GetCollisionEnabled()
			|| Component->GetCollisionObjectType() != Template->GetCollisionObjectType()
			|| Component->GetCollisionResponseToChannels() != Template->GetCollisionResponseToChannels()
			|| Component->CastShadow != Template->CastShadow
			|| Component->IsVisible() != Template->IsVisible()
			|| Component->bHiddenInGame != Template->bHiddenInGame
			|| Component->bReceivesDecals != Template->bReceivesDecals
			|| Component->bUseAsOccluder != Template->bUseAsOccluder
			|| Component->bRenderCustomDepth != Template->bRenderCustomDepth
			|| Component->CustomDepthStencilValue != Template->CustomDepthStencilValue
			|| Component->MinDrawDistance != Template->MinDrawDistance
			|| Component->LDMaxDrawDistance != Template->LDMaxDrawDistance
			|| Component->LightingChannels.bChannel0 != Template->LightingChannels.bChannel0
			|| Component->LightingChannels.bChannel1 != Template->LightingChannels.bChannel1
			|| Component->LightingChannels.bChannel2 != Template->LightingChannels.bChannel2
			|| Component->GetNumMaterials() != Template->GetNumMaterials())
		{
			return false;
		}

		for (int32 MaterialIndex = 0; MaterialIndex < Template->GetNumMaterials(); ++MaterialIndex)
		{
			if (Component->GetMaterial(MaterialIndex) != Template->GetMaterial(MaterialIndex))
			{
				return false;
			}
		}
		return true;
	}
}

FText FConvertToInstancesFix::GetLabel() const
{
	return LOCTEXT("Label", "Convert to instances");
}

bool FConvertToInstancesFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FConvertToInstancesFix::Apply(const FFinding& Finding) const
{
	// The group came from the pass, but the level may have moved on since the
	// scan: re-resolve the actors and re-check they still share one mesh.
	TArray<AStaticMeshActor*> Actors;
	Actors.Reserve(Finding.RelatedActors.Num());
	for (const TWeakObjectPtr<AActor>& ActorPtr : Finding.RelatedActors)
	{
		if (AStaticMeshActor* Actor = Cast<AStaticMeshActor>(ActorPtr.Get()))
		{
			Actors.Add(Actor);
		}
	}

	if (Actors.Num() < 2)
	{
		return false;	// nothing left to batch
	}

	const AStaticMeshActor* TemplateActor = Actors[0];
	const UStaticMeshComponent* Template = TemplateActor->GetStaticMeshComponent();
	UStaticMesh* Mesh = Template ? Template->GetStaticMesh() : nullptr;
	UWorld* World = TemplateActor->GetWorld();
	ULevel* SourceLevel = TemplateActor->GetLevel();
	if (!Mesh || !World || !SourceLevel || !GUnrealEd)
	{
		return false;
	}

	for (const AStaticMeshActor* Actor : Actors)
	{
		if (!ComponentsMatch(*Actor, *TemplateActor) || !GUnrealEd->CanDeleteActor(Actor))
		{
			return false;	// the group drifted or cannot be replaced safely
		}
	}

	FScopedTransaction Transaction(LOCTEXT("Tx", "Convert to instances"));

	// Anchor the holder at the group's centre so instance transforms stay small
	// and the actor lands somewhere sensible in the viewport.
	FVector Centroid = FVector::ZeroVector;
	TArray<FTransform> InstanceTransforms;
	InstanceTransforms.Reserve(Actors.Num());
	for (const AStaticMeshActor* Actor : Actors)
	{
		Centroid += Actor->GetActorLocation();
		InstanceTransforms.Add(Actor->GetActorTransform());
	}
	Centroid /= static_cast<double>(Actors.Num());

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transactional;
	SpawnParams.OverrideLevel = SourceLevel;
	AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Centroid), SpawnParams);
	if (!Holder)
	{
		Transaction.Cancel();
		return false;
	}
	Holder->SetActorLabel(FString::Printf(TEXT("HISM_%s"), *Mesh->GetName()));
	Holder->SetFolderPath(TemplateActor->GetFolderPath());
	Holder->SetActorHiddenInGame(TemplateActor->IsHidden());
	Holder->SetIsTemporarilyHiddenInEditor(TemplateActor->IsHiddenEd());

	UHierarchicalInstancedStaticMeshComponent* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			Holder, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("InstancedMesh"), RF_Transactional);
	
	if (!Instances)
	{
		World->EditorDestroyActor(Holder, /*bShouldModifyLevel*/ true);
		Transaction.Cancel();
		return false;
	}

	Instances->SetStaticMesh(Mesh);
	Instances->SetMobility(EComponentMobility::Static);

	// Carry over what the pass required to match, so the batch renders and
	// collides exactly like the actors it replaces.
	const int32 MaterialCount = Template->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		Instances->SetMaterial(MaterialIndex, Template->GetMaterial(MaterialIndex));
	}
	
	Instances->SetCollisionProfileName(Template->GetCollisionProfileName());
	Instances->SetCollisionEnabled(Template->GetCollisionEnabled());
	Instances->SetCollisionObjectType(Template->GetCollisionObjectType());
	Instances->SetCollisionResponseToChannels(Template->GetCollisionResponseToChannels());
	Instances->CastShadow = Template->CastShadow;
	Instances->SetVisibility(Template->IsVisible());
	Instances->SetHiddenInGame(Template->bHiddenInGame);
	Instances->SetReceivesDecals(Template->bReceivesDecals);
	Instances->bUseAsOccluder = Template->bUseAsOccluder;
	Instances->SetRenderCustomDepth(Template->bRenderCustomDepth);
	Instances->SetCustomDepthStencilValue(Template->CustomDepthStencilValue);
	Instances->MinDrawDistance = Template->MinDrawDistance;
	Instances->SetCullDistance(Template->LDMaxDrawDistance);
	Instances->SetLightingChannels(Template->LightingChannels.bChannel0, Template->LightingChannels.bChannel1, Template->LightingChannels.bChannel2);

	Holder->SetRootComponent(Instances);
	Holder->AddInstanceComponent(Instances);
	Instances->OnComponentCreated();
	Instances->RegisterComponent();

	Instances->AddInstances(InstanceTransforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);

	// Destroying the selected actors while they are selected leaves the editor
	// holding stale selection state.
	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	int32 DestroyedCount = 0;
	for (int32 ActorIndex = Actors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		if (World->EditorDestroyActor(Actors[ActorIndex], /*bShouldModifyLevel*/ true))
		{
			++DestroyedCount;
		}
		else
		{
			// Keep the original and remove its duplicate instance. Reverse order
			// keeps the remaining instance indices stable.
			Instances->RemoveInstance(ActorIndex);
		}
	}

	if (DestroyedCount == 0)
	{
		World->EditorDestroyActor(Holder, /*bShouldModifyLevel*/ true);
		Transaction.Cancel();
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
