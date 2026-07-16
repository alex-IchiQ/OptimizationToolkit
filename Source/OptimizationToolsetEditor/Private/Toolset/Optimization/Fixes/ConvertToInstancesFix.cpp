// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Optimization/Fixes/ConvertToInstancesFix.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ConvertToInstancesFix"

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

	UStaticMeshComponent* Template = Actors[0]->GetStaticMeshComponent();
	UStaticMesh* Mesh = Template ? Template->GetStaticMesh() : nullptr;
	UWorld* World = Actors[0]->GetWorld();
	if (!Mesh || !World)
	{
		return false;
	}

	for (const AStaticMeshActor* Actor : Actors)
	{
		const UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		if (!Component || Component->GetStaticMesh() != Mesh)
		{
			return false;	// the group drifted; refuse rather than merge the wrong things
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("Tx", "Convert to instances"));

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
	AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Centroid), SpawnParams);
	if (!Holder)
	{
		return false;
	}
	Holder->SetActorLabel(FString::Printf(TEXT("ISM_%s"), *Mesh->GetName()));

	UHierarchicalInstancedStaticMeshComponent* Instances =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(
			Holder, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("InstancedMesh"), RF_Transactional);

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
	Instances->CastShadow = Template->CastShadow;

	Holder->SetRootComponent(Instances);
	Instances->OnComponentCreated();
	Instances->RegisterComponent();
	Holder->AddInstanceComponent(Instances);

	Instances->AddInstances(InstanceTransforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);

	// Destroying the selected actors while they are selected leaves the editor
	// holding stale selection state.
	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	for (AStaticMeshActor* Actor : Actors)
	{
		World->EditorDestroyActor(Actor, /*bShouldModifyLevel*/ true);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
