// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/ReviewLightMobilityFix.h"

#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ReviewLightMobilityFix"

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
	AActor* Owner = Finding.TargetActor.Get();
	ULightComponent* LightComponent = Cast<ULightComponent>(Finding.TargetComponent.Get());
	if (!Owner || !LightComponent || LightComponent->GetOwner() != Owner || LightComponent->Mobility != EComponentMobility::Movable)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ReviewLightMobilityTx", "Set Light Stationary"));
	Owner->Modify();
	LightComponent->Modify();
	LightComponent->SetMobility(EComponentMobility::Stationary);
	Owner->PostEditChange();
	Owner->MarkPackageDirty();
	return true;
}

#undef LOCTEXT_NAMESPACE
