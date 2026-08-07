// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/DisableNaniteFix.h"
#include "Toolset/Optimization/Fixes/FixUtils.h"
#include "Toolset/ToolsetCompat.h"

#include "Engine/StaticMesh.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DisableNaniteFix"

FText FDisableNaniteFix::GetLabel() const
{
	return LOCTEXT("DisableNaniteLabel", "Disable Nanite");
}

bool FDisableNaniteFix::IsSupported() const
{
#if OPTIMIZATION_HAS_NANITE
	return true;
#else
	return false;
#endif
}

bool FDisableNaniteFix::Apply(const FFinding& Finding) const
{
#if OPTIMIZATION_HAS_NANITE
	UStaticMesh* Mesh = OptimizationFixUtils::ResolveStaticMesh(Finding);
	if (!Mesh || !Mesh->IsNaniteEnabled())
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("DisableNaniteTx", "Disable Nanite"));
	Mesh->Modify();
#if OPTIMIZATION_UE_VERSION_AT_LEAST(5, 7)
	FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
	Settings.bEnabled = false;
	Mesh->SetNaniteSettings(Settings);
#else
	Mesh->NaniteSettings.bEnabled = false;
#endif
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE
