// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/ICleanupAction.h"

/**
 * Finds assets under /Game that nothing references and hands them to Unreal's
 * own delete dialog for review.
 *
 * The most dangerous thing in the plugin, for one reason: unreferenced is not
 * the same as unused. An asset loaded by soft path, by config entry, or by name
 * from a Blueprint has no reference on disk and will be listed here. That is why
 * this never deletes silently — the review dialog is the feature, not a formality.
 */
class FDeleteUnusedAssetsAction : public ICleanupAction
{
public:
	virtual FName GetId() const override { return TEXT("Cleanup_DeleteUnusedAssets"); }
	virtual FText GetTitle() const override;
	virtual FText GetDescription() const override;
	virtual FText GetButtonLabel() const override;
	virtual bool IsDestructive() const override { return true; }

	/** Unreal's delete dialog already lists every asset and re-checks references. */
	virtual bool NeedsConfirmation() const override { return false; }

	virtual FText Execute() const override;
};
