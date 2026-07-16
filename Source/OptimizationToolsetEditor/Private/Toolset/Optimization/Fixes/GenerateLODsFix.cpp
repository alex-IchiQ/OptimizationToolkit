// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Optimization/Fixes/GenerateLODsFix.h"
#include "Toolset/Optimization/Fixes/FixUtils.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshEditorSubsystem.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "GenerateLODsFix"

FText FGenerateLODsFix::GetLabel() const
{
	return LOCTEXT("GenLODsLabel", "Generate LODs");
}

bool FGenerateLODsFix::IsSupported() const
{
	// Needs the editor (mesh reduction runs editor-side). Always true in-editor.
	return GEditor != nullptr;
}

bool FGenerateLODsFix::Apply(const FFinding& Finding) const
{
	UStaticMesh* Mesh = MeshFromFinding(Finding);
	if (!Mesh)
	{
		return false;
	}

	UStaticMeshEditorSubsystem* Subsystem =
		GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("GenLODsTx", "Generate LODs"));
	Mesh->Modify();

	// A sensible default chain: LOD0 full, then aggressive auto-reduction.
	FStaticMeshReductionOptions Options;
	Options.bAutoComputeLODScreenSize = true;

	auto AddLod = [&Options](float PercentTriangles)
	{
		FStaticMeshReductionSettings Settings;
		Settings.PercentTriangles = PercentTriangles;
		Options.ReductionSettings.Add(Settings);
	};
	AddLod(1.00f);	// LOD0 — untouched
	AddLod(0.50f);	// LOD1
	AddLod(0.25f);	// LOD2
	AddLod(0.10f);	// LOD3

	Subsystem->SetLodsWithNotification(Mesh, Options, /*bApplyChanges*/ true);
	Mesh->MarkPackageDirty();
	return true;
}

#undef LOCTEXT_NAMESPACE
