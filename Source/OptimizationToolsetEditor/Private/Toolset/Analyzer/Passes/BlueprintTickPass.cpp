// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Analyzer/Passes/BlueprintTickPass.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

#define LOCTEXT_NAMESPACE "BlueprintTickPass"

namespace
{
	/** Instances of one Blueprint class that tick every frame while being static. */
	struct FTickingClass
	{
		FString BlueprintName;
		TWeakObjectPtr<AActor> FirstInstance;
		int32 InstanceCount = 0;
	};
}

void FBlueprintTickPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	TMap<const UClass*, FTickingClass> ByClass;

	for (AActor* Actor : Context.Actors)
	{
		if (!Actor)
		{
			continue;
		}

		const UClass* Class = Actor->GetClass();

		// Native actors tick for reasons their C++ decides; only a Blueprint's
		// class defaults are something a user can flip in the editor.
		if (!Class || Class->ClassGeneratedBy == nullptr)
		{
			continue;
		}

		// Ticking is only suspicious when it cannot be doing anything: an actor
		// that already asked for an interval has made a considered choice, and a
		// movable one plausibly has per-frame work.
		const FTickFunction& Tick = Actor->PrimaryActorTick;
		if (!Tick.bCanEverTick || !Tick.bStartWithTickEnabled || Tick.TickInterval > 0.0f)
		{
			continue;
		}

		const USceneComponent* Root = Actor->GetRootComponent();
		if (!Root || Root->Mobility != EComponentMobility::Static)
		{
			continue;
		}

		FTickingClass& Entry = ByClass.FindOrAdd(Class);
		if (Entry.InstanceCount == 0)
		{
			Entry.BlueprintName = Class->ClassGeneratedBy->GetName();
			Entry.FirstInstance = Actor;
		}
		++Entry.InstanceCount;
	}

	for (const TPair<const UClass*, FTickingClass>& Pair : ByClass)
	{
		const FTickingClass& Entry = Pair.Value;

		// One stray ticking prop is a rounding error; a hundred of them is a
		// frame budget.
		const ESeverity Severity = Entry.InstanceCount >= 10 ? ESeverity::Major : ESeverity::Minor;

		FFinding F(TEXT("Blueprint.StaticActorTicksEveryFrame"), Severity, ECategory::Blueprints, EFindingScope::Asset,
			LOCTEXT("Title", "Static Blueprint actor ticks every frame"),
			FText::Format(LOCTEXT("Subject", "{0} ({1} placed)"),
				FText::FromString(Entry.BlueprintName), FText::AsNumber(Entry.InstanceCount)));
		F.WhyItMatters = FText::Format(
			LOCTEXT("Why", "{0} static instances run their tick every frame, paying CPU for something that cannot move."),
			FText::AsNumber(Entry.InstanceCount));
		F.HowToFix = LOCTEXT("Fix", "Open the Blueprint, and in Class Defaults turn off Start with Tick Enabled or set a Tick Interval. This is a class setting: it affects every instance in every level.");
		F.TargetActor = Entry.FirstInstance;
		F.TargetAsset = Pair.Key->ClassGeneratedBy;
		Out.Findings.Add(MoveTemp(F));
	}
}

#undef LOCTEXT_NAMESPACE
