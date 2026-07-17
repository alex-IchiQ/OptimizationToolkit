// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/LightingPass.h"

#include "Engine/Light.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "LightingPass"

namespace
{
	/**
	 * Lightmap resolution actually in effect for a component: its override if it has
	 * one, otherwise whatever the mesh asset defaults to.
	 */
	int32 EffectiveLightmapResolution(const UStaticMeshComponent& Component, const UStaticMesh& Mesh)
	{
		return Component.bOverrideLightMapRes ? Component.OverriddenLightMapRes : Mesh.GetLightMapResolution();
	}
}

void FLightingPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	// --- Lightmap resolution -------------------------------------------------
	//
	// De-duped by mesh *and* resolution rather than by actor: 200 copies of one
	// rock at 512 are one decision, but the same rock overridden to 2048 on a
	// single hero actor is a different one, and collapsing by asset alone would
	// hide it.
	TSet<TPair<const UStaticMesh*, int32>> ReportedLightmaps;

	for (AStaticMeshActor* Actor : Context.StaticMeshActors)
	{
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}

		// Only static components bake a lightmap at all — a movable one is lit
		// dynamically and its resolution costs nothing.
		if (Component->Mobility != EComponentMobility::Static)
		{
			continue;
		}

		const int32 Resolution = EffectiveLightmapResolution(*Component, *Mesh);
		if (Resolution <= T.LightmapResolutionBudget)
		{
			continue;
		}

		const TPair<const UStaticMesh*, int32> Key(Mesh, Resolution);
		if (ReportedLightmaps.Contains(Key))
		{
			continue;
		}
		ReportedLightmaps.Add(Key);

		// A lightmap is Resolution^2 texels of baked, streamed, memory-resident
		// texture, so the cost is quadratic in this number.
		const ESeverity Severity = Resolution > T.LightmapResolutionBudget * 2
			? ESeverity::Major : ESeverity::Minor;

		FFinding F(TEXT("Lighting.LightmapResolution"), Severity, ECategory::Lighting,
			LOCTEXT("LightmapResTitle", "High lightmap resolution"),
			FText::Format(LOCTEXT("LightmapResSubject", "{0} ({1}x{1})"),
				FText::FromString(Mesh->GetName()), FText::AsNumber(Resolution)));
		F.WhyItMatters = FText::Format(
			LOCTEXT("LightmapResWhy", "A {1}x{1} lightmap is four times the texels of {2}x{2}, and it is baked, stored and streamed for every instance. The budget is {0}."),
			FText::AsNumber(T.LightmapResolutionBudget), FText::AsNumber(Resolution), FText::AsNumber(Resolution / 2));
		F.HowToFix = LOCTEXT("LightmapResFix", "Lower the resolution on the component (or the mesh's Light Map Resolution) unless the surface is large enough to need the detail — big floors and walls legitimately do.");
		F.TargetActor = Actor;
		Out.Findings.Add(MoveTemp(F));
	}

	// --- Movable light budget ------------------------------------------------
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
