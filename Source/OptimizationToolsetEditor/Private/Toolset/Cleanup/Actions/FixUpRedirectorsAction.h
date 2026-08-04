// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Cleanup/ICleanupAction.h"

/** Re-points references at moved/renamed assets and removes the leftover redirectors. */
class FFixUpRedirectorsAction : public ICleanupAction
{
public:
	virtual FName GetId() const override { return TEXT("Cleanup_FixUpRedirectors"); }
	virtual FText GetTitle() const override;
	virtual FText GetDescription() const override;
	virtual FText GetButtonLabel() const override;
	virtual bool IsDestructive() const override { return true; }
	virtual FText Execute() const override;
};
