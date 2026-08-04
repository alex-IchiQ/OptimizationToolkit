// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"

class FToolsetModel;

/** Routes a finding's primary action without coupling its card to editor APIs. */
class FFindingNavigator
{
public:
	static bool CanNavigate(const FFinding& Finding);
	static FText GetActionLabel(const FFinding& Finding);
	static void Navigate(const FFinding& Finding, const TSharedPtr<FToolsetModel>& Model);

	/** Opens Rendering settings and optionally filters to the setting for a finding type. */
	static void OpenProjectRenderingSettings(FName FindingTypeId = NAME_None);
};
