// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "OptimizationToolsetEditorModule.h"
#include "Toolset/ToolsetRegistry.h"
#include "Toolset/Slate/OptimizeStyle.h"
#include "Toolset/Slate/SOptimizeShell.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "OptimizationToolset"

static const FName ToolsetTabName("OptimizationToolkit");

void FOptimizationToolsetEditorModule::StartupModule()
{
	FOptimizeStyle::Initialize();
	FToolsetRegistry::Get().RegisterDefaults();

	// Register the dockable tab.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ToolsetTabName, FOnSpawnTab::CreateRaw(this, &FOptimizationToolsetEditorModule::SpawnToolsetTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Ultimate Optimization Toolkit"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Analyze, optimize and profile the current level."))
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
	}

	FOptimizeStyle::Shutdown();
}

void FOptimizationToolsetEditorModule::OpenToolsetTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ToolsetTabName);
}

TSharedRef<SDockTab> FOptimizationToolsetEditorModule::SpawnToolsetTab(const FSpawnTabArgs& Args)
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
			"OpenOptimizationToolkit",
			LOCTEXT("MenuEntry", "Ultimate Optimization Toolkit"),
			LOCTEXT("MenuEntryTip", "Open Ultimate Optimization Toolkit."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"),
			FUIAction(FExecuteAction::CreateRaw(this, &FOptimizationToolsetEditorModule::OpenToolsetTab)));
	}

	// Level Editor toolbar button.
	if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User"))
	{
		FToolMenuSection& Section = Toolbar->FindOrAddSection("OptimizationToolkit");
		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			"OpenOptimizationToolset",
			FUIAction(FExecuteAction::CreateRaw(this, &FOptimizationToolsetEditorModule::OpenToolsetTab)),
			LOCTEXT("ToolbarLabel", "Optimize"),
			LOCTEXT("ToolbarTip", "Open Ultimate Optimization Toolkit."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
		Section.AddEntry(Entry);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOptimizationToolsetEditorModule, OptimizationToolsetEditor)
