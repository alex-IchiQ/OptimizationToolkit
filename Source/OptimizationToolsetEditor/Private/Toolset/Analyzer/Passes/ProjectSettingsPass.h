// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Analyzer/IAnalyzePass.h"

/**
 * Project-wide rendering settings that cost performance everywhere.
 *
 * The only pass that ignores the level entirely: these are project settings, not
 * placed actors. It lives in Analyze anyway because it produces the same thing
 * every other pass does — a finding with a severity, a reason and a next step —
 * and a user hunting for lost frames should not have to look in two places.
 *
 * None of these carry a FixId: writing project settings means editing
 * DefaultEngine.ini and usually restarting the editor, which is not something to
 * do behind a one-click "fix" button.
 */
class FProjectSettingsPass : public IAnalyzePass
{
public:
	virtual FName GetId() const override { return TEXT("Pass_ProjectSettings"); }
	virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& Thresholds, FScanResult& Out) const override;
};
