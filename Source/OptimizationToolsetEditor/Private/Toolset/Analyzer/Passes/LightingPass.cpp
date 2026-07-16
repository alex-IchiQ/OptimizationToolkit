// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/LightingPass.h"

#include "Engine/Light.h"
#include "Components/LightComponent.h"

#define LOCTEXT_NAMESPACE "LightingPass"

void FLightingPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TArray<ALight*> MovableLights;

	for (ALight* Light : Context.Lights)
	{
		ULightComponent* LC = Light->GetLightComponent();
		if (LC && LC->Mobility == EComponentMobility::Movable)
		{
			MovableLights.Add(Light);
		}
	}

	if (MovableLights.Num() <= T.MovableLightBudget)
	{
		return;
	}

	const FText BudgetContext = FText::Format(
		LOCTEXT("MovableLightBudgetContext", "The level contains {0} movable lights; the configured budget is {1}."),
		FText::AsNumber(MovableLights.Num()), FText::AsNumber(T.MovableLightBudget));

	// Emit addressable findings so Focus and Apply operate on the light the user
	// is reviewing instead of an arbitrary actor from an aggregate warning.
	for (ALight* Light : MovableLights)
	{
		FFinding F(TEXT("Lighting.MovableLightOverBudget"), ESeverity::Major, ECategory::Lighting,
			LOCTEXT("MovableLightTitle", "Movable light contributes to an exceeded budget"),
			FText::FromString(Light->GetActorLabel()));
		F.WhyItMatters = FText::Format(
			LOCTEXT("MovableLightWhy", "{0} Every movable light adds dynamic shadow and lighting cost each frame."),
			BudgetContext);
		F.HowToFix = LOCTEXT("MovableLightFix", "If this light does not move at runtime, change it to Stationary.");
		F.TargetActor = Light;
		F.FixId = TEXT("Fix_ReviewLightMobility");
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
