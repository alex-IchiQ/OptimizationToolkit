// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Styling/SlateStyle.h"

/**
 * Central Slate style set for the toolset UI.
 *
 * Owns the colour palette (surfaces, accent, severity colours), rounded-card
 * brushes and text styles so every panel pulls from one place and the window
 * reads as a single designed system rather than stock editor widgets.
 */
class FToolsetStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	/** Accent / surface palette (public so panels can tint inline where needed). */
	static const FLinearColor Accent;
	static const FLinearColor AccentDim;
	static const FLinearColor SurfaceBase;		// window background
	static const FLinearColor SurfacePanel;		// sidebar / header
	static const FLinearColor SurfaceCard;		// content cards
	static const FLinearColor SurfaceCardHover;
	static const FLinearColor TextPrimary;
	static const FLinearColor TextSecondary;

	static const FLinearColor SeverityCritical;
	static const FLinearColor SeverityMajor;
	static const FLinearColor SeverityMinor;
	static const FLinearColor SeverityGood;

	/** Maps a severity to its display colour and label. */
	static FLinearColor ColorForSeverity(ESeverity Severity);
	static FText LabelForSeverity(ESeverity Severity);
	static FText LabelForCategory(ECategory Category);

private:
	static TSharedRef<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
