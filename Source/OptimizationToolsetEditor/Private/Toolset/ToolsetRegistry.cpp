// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/ToolsetRegistry.h"

#include "Toolset/Analyzer/Passes/StaticMeshPass.h"
#include "Toolset/Analyzer/Passes/TexturePass.h"
#include "Toolset/Analyzer/Passes/MaterialPass.h"
#include "Toolset/Analyzer/Passes/LightingPass.h"
#include "Toolset/Analyzer/Passes/InstancingCandidatePass.h"
#include "Toolset/Analyzer/Passes/ProjectSettingsPass.h"
#include "Toolset/Analyzer/Passes/BlueprintTickPass.h"
#include "Toolset/Analyzer/Passes/TextureCompressionPass.h"
#include "Toolset/Analyzer/Passes/BlueprintDependencyPass.h"

#include "Toolset/Optimization/Fixes/EnableNaniteFix.h"
#include "Toolset/Optimization/Fixes/DisableNaniteFix.h"
#include "Toolset/Optimization/Fixes/GenerateLODsFix.h"
#include "Toolset/Optimization/Fixes/SimpleCollisionFix.h"
#include "Toolset/Optimization/Fixes/ReviewLightMobilityFix.h"
#include "Toolset/Optimization/Fixes/DeleteEmptyMeshActorFix.h"
#include "Toolset/Optimization/Fixes/ConvertToInstancesFix.h"
#include "Toolset/Optimization/Fixes/TextureSettingsFixes.h"

#include "Toolset/Cleanup/Actions/SaveDirtyPackagesAction.h"
#include "Toolset/Cleanup/Actions/FixUpRedirectorsAction.h"

FToolsetRegistry& FToolsetRegistry::Get()
{
	static FToolsetRegistry Instance;
	return Instance;
}

void FToolsetRegistry::RegisterDefaults()
{
	// Clear first so a live-coding / module reload doesn't stack duplicates.
	Passes.Reset();
	Fixes.Reset();
	Actions.Reset();

	AddPass(MakeUnique<FStaticMeshPass>());
	AddPass(MakeUnique<FTexturePass>());
	AddPass(MakeUnique<FMaterialPass>());
	AddPass(MakeUnique<FLightingPass>());
	AddPass(MakeUnique<FInstancingCandidatePass>());
	AddPass(MakeUnique<FProjectSettingsPass>());
	AddPass(MakeUnique<FBlueprintTickPass>());
	AddPass(MakeUnique<FTextureCompressionPass>());
	AddPass(MakeUnique<FBlueprintDependencyPass>());

	AddFix(MakeUnique<FEnableNaniteFix>());
	AddFix(MakeUnique<FDisableNaniteFix>());
	AddFix(MakeUnique<FGenerateLODsFix>());
	AddFix(MakeUnique<FSimpleCollisionFix>());
	AddFix(MakeUnique<FReviewLightMobilityFix>());
	AddFix(MakeUnique<FDeleteEmptyMeshActorFix>());
	AddFix(MakeUnique<FConvertToInstancesFix>());
	AddFix(MakeUnique<FNormalmapCompressionFix>());
	AddFix(MakeUnique<FDisableTextureSRGBFix>());
	AddFix(MakeUnique<FEnableStreamingFix>());

	AddAction(MakeUnique<FSaveDirtyPackagesAction>());
	AddAction(MakeUnique<FFixUpRedirectorsAction>());
}

void FToolsetRegistry::AddPass(TUniquePtr<IAnalyzePass> Pass)
{
	if (Pass)
	{
		Passes.Add(MoveTemp(Pass));
	}
}

void FToolsetRegistry::AddFix(TUniquePtr<IOptimizationFix> Fix)
{
	if (Fix)
	{
		Fixes.Add(MoveTemp(Fix));
	}
}

void FToolsetRegistry::AddAction(TUniquePtr<ICleanupAction> Action)
{
	if (Action)
	{
		Actions.Add(MoveTemp(Action));
	}
}

IOptimizationFix* FToolsetRegistry::FindFix(FName FixId) const
{
	for (const TUniquePtr<IOptimizationFix>& Fix : Fixes)
	{
		if (Fix && Fix->GetId() == FixId)
		{
			return Fix.Get();
		}
	}
	return nullptr;
}
