// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * A section that is designed but not built yet: it states what it will do and
 * lists the planned actions, rather than showing an empty panel or nothing at all.
 *
 * Only Reports still uses this.
 */
class SPlaceholderPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlaceholderPanel) {}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Body)
		SLATE_ARGUMENT(TArray<FText>, PlannedActions)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
