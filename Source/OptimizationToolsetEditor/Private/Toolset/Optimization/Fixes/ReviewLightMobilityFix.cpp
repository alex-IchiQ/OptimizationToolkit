// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Optimization/Fixes/ReviewLightMobilityFix.h"

#include "Editor.h"
#include "Engine/Light.h"
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
