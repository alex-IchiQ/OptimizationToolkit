// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"
#include "Toolset/Optimization/IOptimizationFix.h"
#include "Toolset/Cleanup/ICleanupAction.h"

/**
 * Central registry of analyze passes, optimization fixes and cleanup actions,
 * populated once at module startup. The window and analyzer iterate it; features
 * never reference each other. Adding a feature = add its class + one line in
 * RegisterDefaults().
 */
class FToolsetRegistry
{
public:
	static FToolsetRegistry& Get();

	/** Clears then registers the built-in passes and fixes. */
	void RegisterDefaults();

	void AddPass(TUniquePtr<IAnalyzePass> Pass);
	void AddFix(TUniquePtr<IOptimizationFix> Fix);
	void AddAction(TUniquePtr<ICleanupAction> Action);

	const TArray<TUniquePtr<IAnalyzePass>>& GetPasses() const { return Passes; }
	const TArray<TUniquePtr<IOptimizationFix>>& GetFixes() const { return Fixes; }
	const TArray<TUniquePtr<ICleanupAction>>& GetActions() const { return Actions; }

	/** First fix whose GetId() matches, or nullptr. Non-owning. */
	IOptimizationFix* FindFix(FName FixId) const;

private:
	TArray<TUniquePtr<IAnalyzePass>> Passes;
	TArray<TUniquePtr<IOptimizationFix>> Fixes;
	TArray<TUniquePtr<ICleanupAction>> Actions;
};
