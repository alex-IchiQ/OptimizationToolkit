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
			// Material shader stats: FMaterialStatsUtils lives here. The engine's own
			// GetRepresentativeInstructionCounts() is not exported (its neighbours in
			// the same class are), so FMaterialPass walks the representative shader
			// types itself using the exported half of that API.
			"MaterialEditor",
			"RHI",
			"AssetRegistry",
			"AssetTools",
			"EngineSettings",
			"AssetManagerEditor",
			"PropertyEditor",
			"Settings",
		});
	}
}
