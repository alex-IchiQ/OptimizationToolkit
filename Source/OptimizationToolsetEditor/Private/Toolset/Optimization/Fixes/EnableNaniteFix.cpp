// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/EnableNaniteFix.h"
#include "Toolset/Optimization/Fixes/FixUtils.h"
#include "Toolset/ToolsetCompat.h"

#include "Engine/StaticMesh.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EnableNaniteFix"

FText FEnableNaniteFix::GetLabel() const
{
	return LOCTEXT("EnableNaniteLabel", "Enable Nanite");
}

bool FEnableNaniteFix::IsSupported() const
{
#if OPTIMIZATION_HAS_NANITE
	return true;
#else
	return false;	// pre-5.0 engines have no Nanite
#endif
}

bool FEnableNaniteFix::Apply(const FFinding& Finding) const
{
#if OPTIMIZATION_HAS_NANITE
	UStaticMesh* Mesh = OptimizationFixUtils::ResolveStaticMesh(Finding);
	if (!Mesh || Mesh->IsNaniteEnabled())
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("EnableNaniteTx", "Enable Nanite"));
	Mesh->Modify();
#if OPTIMIZATION_UE_VERSION_AT_LEAST(5, 7)
	FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
	Settings.bEnabled = true;
	Mesh->SetNaniteSettings(Settings);
#else
	Mesh->NaniteSettings.bEnabled = true;
#endif
	Mesh->PostEditChange();		// rebuilds the mesh with Nanite data
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE
