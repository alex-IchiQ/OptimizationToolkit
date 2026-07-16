// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/ICleanupAction.h"

/** Saves every modified map and content package without prompting per asset. */
class FSaveDirtyPackagesAction : public ICleanupAction
{
public:
	virtual FName GetId() const override { return TEXT("Cleanup_SaveDirtyPackages"); }
	virtual FText GetTitle() const override;
	virtual FText GetDescription() const override;
	virtual FText GetButtonLabel() const override;
	virtual FText Execute() const override;
};
