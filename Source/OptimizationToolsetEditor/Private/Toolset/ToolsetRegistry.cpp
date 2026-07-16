// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/ToolsetRegistry.h"
#include "Toolset/Analyzer/AnalyzePasses.h"
#include "Toolset/Optimization/OptimizationFixes.h"

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

	AddPass(MakeUnique<FStaticMeshPass>());
	AddPass(MakeUnique<FLightingPass>());

	AddFix(MakeUnique<FEnableNaniteFix>());
	AddFix(MakeUnique<FGenerateLODsFix>());
	AddFix(MakeUnique<FSimpleCollisionFix>());
	AddFix(MakeUnique<FReviewLightMobilityFix>());
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
