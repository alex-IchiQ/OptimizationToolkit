// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Optimization/Fixes/TextureSettingsFixes.h"

#include "Editor.h"
#include "Engine/Texture2D.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "TextureSettingsFixes"

namespace
{
	/** The texture a finding is about, or null. */
	UTexture2D* TextureFromFinding(const FFinding& Finding)
	{
		return Cast<UTexture2D>(Finding.TargetAsset.Get());
	}
}

// ---------------------------------------------------------------------------
// Normal map compression
// ---------------------------------------------------------------------------
FText FNormalmapCompressionFix::GetLabel() const
{
	return LOCTEXT("NormalLabel", "Fix compression");
}

bool FNormalmapCompressionFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FNormalmapCompressionFix::Apply(const FFinding& Finding) const
{
	UTexture2D* Texture = TextureFromFinding(Finding);
	if (!Texture || Texture->CompressionSettings == TC_Normalmap)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("NormalTx", "Set normal map compression"));
	Texture->Modify();
	Texture->CompressionSettings = TC_Normalmap;

	// The engine forces sRGB off for Normalmap compression anyway; setting it
	// here keeps the asset consistent rather than relying on that to happen.
	Texture->SRGB = false;

	Texture->PostEditChange();		// recompresses the texture
	Texture->MarkPackageDirty();
	return true;
}

// ---------------------------------------------------------------------------
// Data texture colour space
// ---------------------------------------------------------------------------
FText FDisableTextureSRGBFix::GetLabel() const
{
	return LOCTEXT("SRGBLabel", "Disable sRGB");
}

bool FDisableTextureSRGBFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FDisableTextureSRGBFix::Apply(const FFinding& Finding) const
{
	UTexture2D* Texture = TextureFromFinding(Finding);
	if (!Texture || !Texture->SRGB)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("SRGBTx", "Disable texture sRGB"));
	Texture->Modify();
	Texture->SRGB = false;
	Texture->PostEditChange();		// recompresses the texture
	Texture->MarkPackageDirty();
	return true;
}

// ---------------------------------------------------------------------------
// Enable streaming
// ---------------------------------------------------------------------------
FText FEnableStreamingFix::GetLabel() const
{
	return LOCTEXT("StreamLabel", "Enable streaming");
}

bool FEnableStreamingFix::IsSupported() const
{
	return GEditor != nullptr;
}

bool FEnableStreamingFix::Apply(const FFinding& Finding) const
{
	UTexture2D* Texture = TextureFromFinding(Finding);
	if (!Texture || !Texture->NeverStream)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("StreamTx", "Enable texture streaming"));
	Texture->Modify();
	Texture->NeverStream = false;
	Texture->PostEditChange();		// rebuilds the texture's streaming state
	Texture->MarkPackageDirty();
	return true;
}

#undef LOCTEXT_NAMESPACE
