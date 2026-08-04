// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

enum class ESeverity : uint8;
enum class EAssetCategory : uint8;

/**
 * Slate style for the redesigned toolset UI (the Slate/ views).
 *
 * A fresh, deliberately flat style set modelled on Palatial's design language —
 * dark solid panels, small-radius rounded boxes for cards/tiles/buttons, quiet
 * hairlines instead of outlined "islands" — but painted in the mascot's own
 * colours (teal accent, charcoal surfaces). This is the single style set for
 * the plugin: controls, icons, mascot and semantic colours all live here.
 * Brushes are prefixed "Opt.".
 */
class FOptimizeStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	/** Convenience: a brush from this set. */
	static const FSlateBrush* Brush(const FName& Name) { return Get().GetBrush(Name); }

	// Palette (public so views can tint inline where needed).
	static const FLinearColor Accent;
	static const FLinearColor AccentBright;	// hover
	static const FLinearColor AccentDim;	// pressed / deep
	static const FLinearColor OnAccent;		// text/icon drawn on the accent fill
	static const FLinearColor Window;		// window backdrop
	static const FLinearColor Panel;		// nav rail / section surface
	static const FLinearColor Card;			// cards / tiles
	static const FLinearColor CardHover;
	static const FLinearColor Line;			// hairline / divider
	static const FLinearColor TextPrimary;
	static const FLinearColor TextDim;
	static const FLinearColor SeverityCritical;
	static const FLinearColor SeverityMajor;
	static const FLinearColor SeverityMinor;
	static const FLinearColor SeverityGood;

	static FLinearColor ColorForSeverity(ESeverity Severity);
	static FText LabelForSeverity(ESeverity Severity);
	static FLinearColor ColorForAssetCategory(EAssetCategory Category);

private:
	static TSharedRef<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
