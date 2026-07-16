// Copyright Optimization Toolset. All Rights Reserved.

using UnrealBuildTool;

public class OptimizationToolsetEditor : ModuleRules
{
	public OptimizationToolsetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"UnrealEd",
			"ToolMenus",
			"LevelEditor",
			"EditorFramework",
			"EditorSubsystem",
			"StaticMeshEditor",
			"WorkspaceMenuStructure",
			"ApplicationCore",
			"RenderCore",
			"AssetRegistry",
			"AssetTools",
		});
	}
}
