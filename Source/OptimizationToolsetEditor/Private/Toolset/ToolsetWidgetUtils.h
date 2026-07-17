// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "Toolset/ToolsetStyle.h"

/**
 * Terse accessors for the style set, shared by every panel.
 *
 * Slate declarative code is dense enough without FToolsetStyle::Get().GetBrush()
 * spelled out on each of a hundred lines. Header-only and inline: this is
 * shorthand, not a layer.
 */
namespace ToolsetUI
{
	inline const ISlateStyle& S()
	{
		return FToolsetStyle::Get();
	}

	inline const FSlateBrush* Brush(const FName& Name)
	{
		return FToolsetStyle::Get().GetBrush(Name);
	}
}
