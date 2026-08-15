// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * Editor module entry point.
 *
 * Owns lifetime of the Slate style, registers the dockable "Optimization
 * Toolkit" tab and adds an entry to the Level Editor toolbar + Window menu.
 */
class FOptimizationToolsetEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void OpenToolsetTab();

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnToolsetTab(const FSpawnTabArgs& Args);
};
