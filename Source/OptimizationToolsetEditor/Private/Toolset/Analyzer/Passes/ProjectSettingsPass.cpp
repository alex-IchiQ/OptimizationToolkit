// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Analyzer/Passes/ProjectSettingsPass.h"

#include "Engine/RendererSettings.h"

#define LOCTEXT_NAMESPACE "ProjectSettingsPass"

void FProjectSettingsPass::Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const
{
	const URendererSettings* Settings = GetDefault<URendererSettings>();
	if (!Settings)
	{
		return;
	}

	const FText Subject = LOCTEXT("Subject", "Project Settings > Rendering");

	// --- Occlusion culling off means every mesh in the level is submitted, no
	// matter what is actually visible. Nothing else in the plugin can win back
	// what this costs.
	if (!Settings->bOcclusionCulling)
	{
		FFinding F(TEXT("Project.OcclusionCullingDisabled"), ESeverity::Critical, ECategory::Project, EFindingScope::Project,
			LOCTEXT("OcclusionTitle", "Occlusion culling is disabled"), Subject);
		F.WhyItMatters = LOCTEXT("OcclusionWhy", "Every mesh is rendered whether or not anything can see it, so the whole level is paid for in every frame.");
		F.HowToFix = LOCTEXT("OcclusionFix", "Enable Occlusion Culling in Project Settings > Rendering > Culling, unless the project deliberately handles visibility itself.");
		Out.Findings.Add(MoveTemp(F));
	}

	// --- Every permutation of every shader gets compiled and shipped, most of
	// them for features the project never uses. This one lives on the separate
	// "Rendering Overrides" settings object, not on URendererSettings.
	const URendererOverrideSettings* Overrides = GetDefault<URendererOverrideSettings>();
	if (Overrides && Overrides->bSupportAllShaderPermutations)
	{
		FFinding F(TEXT("Project.AllShaderPermutations"), ESeverity::Major, ECategory::Project, EFindingScope::Project,
			LOCTEXT("PermutationsTitle", "Support All Shader Permutations is enabled"),
			LOCTEXT("OverridesSubject", "Project Settings > Rendering Overrides"));
		F.WhyItMatters = LOCTEXT("PermutationsWhy", "Shaders are compiled for features the project never uses, inflating cook time, package size and the PSO count behind hitches.");
		F.HowToFix = LOCTEXT("PermutationsFix", "Disable Support All Shader Permutations in Project Settings > Rendering > Optimizations; it exists for engine development, not shipping projects.");
		Out.Findings.Add(MoveTemp(F));
	}

	// --- Lumen computes GI at runtime, so lightmap support only adds cost.
	if (Settings->bAllowStaticLighting && Settings->DynamicGlobalIllumination == EDynamicGlobalIlluminationMethod::Lumen)
	{
		FFinding F(TEXT("Project.StaticLightingWithLumen"), ESeverity::Major, ECategory::Project, EFindingScope::Project,
			LOCTEXT("StaticLightingTitle", "Static lighting is allowed while Lumen provides GI"), Subject);
		F.WhyItMatters = LOCTEXT("StaticLightingWhy", "Lumen computes global illumination at runtime, so lightmap UVs, lightmap memory and the extra shader permutations they require are paid for and never used.");
		F.HowToFix = LOCTEXT("StaticLightingFix", "If nothing in the project bakes lighting, disable Allow Static Lighting in Project Settings > Rendering. Verify no level relies on baked lightmaps first.");
		Out.Findings.Add(MoveTemp(F));
	}

	// --- Ray tracing costs even where it is not visibly used.
	if (Settings->bEnableRayTracing)
	{
		FFinding F(TEXT("Project.RayTracingEnabled"), ESeverity::Minor, ECategory::Project, EFindingScope::Project,
			LOCTEXT("RayTracingTitle", "Hardware ray tracing is enabled"), Subject);
		F.WhyItMatters = LOCTEXT("RayTracingWhy", "Ray tracing maintains an acceleration structure every frame and narrows the hardware the project can ship to.");
		F.HowToFix = LOCTEXT("RayTracingFix", "Keep it only if the project actually uses ray traced effects; otherwise disable it in Project Settings > Rendering > Hardware Ray Tracing.");
		Out.Findings.Add(MoveTemp(F));

		// Ray tracing needs skinned geometry in the acceleration structure, and
		// the skin cache is what puts it there.
		if (!Settings->bSupportSkinCacheShaders)
		{
			FFinding G(TEXT("Project.RayTracingWithoutSkinCache"), ESeverity::Major, ECategory::Project, EFindingScope::Project,
				LOCTEXT("SkinCacheTitle", "Ray tracing is enabled without skin cache shaders"), Subject);
			G.WhyItMatters = LOCTEXT("SkinCacheWhy", "Without the skin cache, skeletal meshes are missing from the ray tracing acceleration structure, so they are absent from reflections and shadows.");
			G.HowToFix = LOCTEXT("SkinCacheFix", "Enable Support Compute Skin Cache in Project Settings > Rendering > Optimizations, or disable ray tracing.");
			Out.Findings.Add(MoveTemp(G));
		}
	}
}

#undef LOCTEXT_NAMESPACE
