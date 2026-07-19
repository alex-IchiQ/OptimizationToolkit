// Copyright Optimization Toolset. All Rights Reserved.

#include "OptimizationToolsetEditorModule.h"
#include "Toolset/ToolsetStyle.h"
#include "Toolset/SToolsetWindow.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/Slate/SOptimizeShell.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "OptimizationToolset"

static const FName ToolsetTabName("OptimizationToolset");
static const FName OptimizeViewTabName("OptimizationToolsetOptimizeView");

void FOptimizationToolsetEditorModule::StartupModule()
{
	FToolsetStyle::Initialize();
	FToolsetRegistry::Get().RegisterDefaults();

	// Register the dockable tab.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ToolsetTabName, FOnSpawnTab::CreateRaw(this, &FOptimizationToolsetEditorModule::SpawnToolsetTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Optimization Toolset"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Analyze, optimize and profile the current level."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));

	// Parallel tab for the new-UI Optimize view, kept separate so both can run.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OptimizeViewTabName, FOnSpawnTab::CreateRaw(this, &FOptimizationToolsetEditorModule::SpawnOptimizeViewTab))
		.SetDisplayName(LOCTEXT("OptimizeViewTabTitle", "Optimize (New UI)"))
		.SetTooltipText(LOCTEXT("OptimizeViewTabTooltip", "Work-in-progress Optimize page: level tree, affected assets, batch fix."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));

	// Defer menu registration until ToolMenus is ready.
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOptimizationToolsetEditorModule::RegisterMenus));
}

void FOptimizationToolsetEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToolsetTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OptimizeViewTabName);
	}

	FToolsetStyle::Shutdown();
}

void FOptimizationToolsetEditorModule::OpenToolsetTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ToolsetTabName);
}

void FOptimizationToolsetEditorModule::OpenOptimizeViewTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(OptimizeViewTabName);
}

TSharedRef<SDockTab> FOptimizationToolsetEditorModule::SpawnToolsetTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SToolsetWindow)
		];
}

TSharedRef<SDockTab> FOptimizationToolsetEditorModule::SpawnOptimizeViewTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SOptimizeShell)
		];
}

void FOptimizationToolsetEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Window menu entry.
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntry(
			"OpenOptimizationToolset",
			LOCTEXT("MenuEntry", "Optimization Toolset"),
			LOCTEXT("MenuEntryTip", "Open the Optimization / Profiling Toolset."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"),
			FUIAction(FExecuteAction::CreateRaw(this, &FOptimizationToolsetEditorModule::OpenToolsetTab)));

		Section.AddMenuEntry(
			"OpenOptimizationToolsetOptimizeView",
			LOCTEXT("MenuEntryNew", "Optimize (New UI)"),
			LOCTEXT("MenuEntryNewTip", "Open the work-in-progress Optimize page."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"),
			FUIAction(FExecuteAction::CreateRaw(this, &FOptimizationToolsetEditorModule::OpenOptimizeViewTab)));
	}

	// Level Editor toolbar button.
	if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User"))
	{
		FToolMenuSection& Section = Toolbar->FindOrAddSection("OptimizationToolset");
		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			"OpenOptimizationToolset",
			FUIAction(FExecuteAction::CreateRaw(this, &FOptimizationToolsetEditorModule::OpenToolsetTab)),
			LOCTEXT("ToolbarLabel", "Optimize"),
			LOCTEXT("ToolbarTip", "Open the Optimization / Profiling Toolset."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
		Section.AddEntry(Entry);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOptimizationToolsetEditorModule, OptimizationToolsetEditor)
