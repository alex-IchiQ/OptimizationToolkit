// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * Editor module entry point.
 *
 * Owns lifetime of the Slate style, registers the dockable "Optimization
 * Toolset" tab and adds an entry to the Level Editor toolbar + Window menu.
 */
class FOptimizationToolsetEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Focuses (or spawns) the toolset tab. */
	void OpenToolsetTab();

	/** Focuses (or spawns) the new-UI Optimize tab, developed in parallel. */
	void OpenOptimizeViewTab();

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnToolsetTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnOptimizeViewTab(const FSpawnTabArgs& Args);
};
