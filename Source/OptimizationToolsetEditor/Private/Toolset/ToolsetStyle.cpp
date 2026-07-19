// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/ToolsetStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "ToolsetStyle"

// ---- Palette (Islands layout, mascot colour scheme) -------------------------
// Values are authored as sRGB hex and converted to linear: Slate tints/brushes
// take a *linear* FLinearColor, so passing raw sRGB fractions would wash the
// whole UI out to grey. FLinearColor(FColor) applies the sRGB->linear curve.
// Accent = the mascot's signature teal (hair tips / jacket piping / trim);
// surfaces echo her charcoal jacket; the amber severity colours match her eyes.
const FLinearColor FToolsetStyle::Accent          = FLinearColor(FColor(0x17, 0xB9, 0xA6)); // teal
const FLinearColor FToolsetStyle::AccentDim       = FLinearColor(FColor(0x0E, 0x6E, 0x63)); // deep teal
const FLinearColor FToolsetStyle::SurfaceBase     = FLinearColor(FColor(0x13, 0x15, 0x17)); // backdrop
const FLinearColor FToolsetStyle::SurfacePanel    = FLinearColor(FColor(0x23, 0x26, 0x2A)); // island (jacket charcoal)
const FLinearColor FToolsetStyle::SurfaceCard     = FLinearColor(FColor(0x2E, 0x32, 0x38)); // card
const FLinearColor FToolsetStyle::SurfaceCardHover= FLinearColor(FColor(0x37, 0x3C, 0x43)); // card hover
const FLinearColor FToolsetStyle::TextPrimary     = FLinearColor(FColor(0xE6, 0xE8, 0xEB));
const FLinearColor FToolsetStyle::TextSecondary   = FLinearColor(FColor(0x96, 0x9A, 0xA0));
const FLinearColor FToolsetStyle::OnAccent        = FLinearColor(FColor(0x16, 0x17, 0x19)); // on teal

const FLinearColor FToolsetStyle::SeverityCritical= FLinearColor(FColor(0xEF, 0x4A, 0x3F));
const FLinearColor FToolsetStyle::SeverityMajor   = FLinearColor(FColor(0xF5, 0xA7, 0x23));
const FLinearColor FToolsetStyle::SeverityMinor   = FLinearColor(FColor(0x4A, 0xA3, 0xED));
const FLinearColor FToolsetStyle::SeverityGood    = FLinearColor(FColor(0x2E, 0xCC, 0x71));

TSharedPtr<FSlateStyleSet> FToolsetStyle::StyleInstance = nullptr;

void FToolsetStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FToolsetStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

const ISlateStyle& FToolsetStyle::Get()
{
	return *StyleInstance;
}

FName FToolsetStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ToolsetStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FToolsetStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	// --- Surfaces ------------------------------------------------------------
	// Flat backdrop that every island floats on.
	Style->Set("Toolset.Surface.Base",  new FSlateColorBrush(SurfaceBase));
	Style->Set("Toolset.Surface.Panel", new FSlateColorBrush(SurfacePanel));

	// "Island": a rounded, elevated panel with a hairline edge — the defining
	// element of the Islands look. Solid (not translucent) outline avoids a
	// faint seam on the corner arc where it would blend over the dark backdrop.
	Style->Set("Toolset.Island", new FSlateRoundedBoxBrush(SurfacePanel, 10.0f, FLinearColor(FColor(0x33, 0x38, 0x3F)), 1.0f));

	// Rounded cards (radius, solid 1px outline — solid, not translucent, so the
	// corner arc has no faint seam over whatever surface sits behind it).
	Style->Set("Toolset.Card",       new FSlateRoundedBoxBrush(SurfaceCard, 8.0f, FLinearColor(FColor(0x3D, 0x42, 0x4A)), 1.0f));
	Style->Set("Toolset.Card.Hover", new FSlateRoundedBoxBrush(SurfaceCardHover, 8.0f, FLinearColor(FColor(0x47, 0x4D, 0x55)), 1.0f));
	Style->Set("Toolset.Card.Inner", new FSlateRoundedBoxBrush(SurfacePanel, 6.0f));

	// The tree's own backdrop. Stock TreeView draws a hard-cornered dark rectangle,
	// which reads as a hole punched through the island it sits on; this rounds it to
	// match the surrounding radii.
	{
		FTableViewStyle TreeView = FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("TreeView");
		TreeView.SetBackgroundBrush(FSlateRoundedBoxBrush(SurfaceBase, 8.0f));
		Style->Set("Toolset.TreeView", TreeView);
	}

	// Transparent rows, so the rounded backdrop above survives.
	//
	// STableViewBase paints its background across the full geometry — all four
	// corners round. But rows paint their *own* backgrounds on top, and the stock
	// TableView.Row brushes are opaque squares. Rows stack from the top, so they
	// cover the top corners while the empty space below the last row still shows
	// the arc: the tree ends up rounded at the bottom only. Our finding cards draw
	// their own card background anyway, so every row state here is nothing.
	{
		FTableRowStyle Row = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
		Row.SetEvenRowBackgroundBrush(FSlateNoResource());
		Row.SetEvenRowBackgroundHoveredBrush(FSlateNoResource());
		Row.SetOddRowBackgroundBrush(FSlateNoResource());
		Row.SetOddRowBackgroundHoveredBrush(FSlateNoResource());
		Row.SetActiveBrush(FSlateNoResource());
		Row.SetActiveHoveredBrush(FSlateNoResource());
		Row.SetInactiveBrush(FSlateNoResource());
		Row.SetInactiveHoveredBrush(FSlateNoResource());
		Style->Set("Toolset.TableRow", Row);
	}

	// A plain list row: transparent so the rounded backdrop shows, but with a faint
	// hover so each row still feels interactive. For classic settings-style lists
	// (the level scope) where the row itself is the control, not a card.
	{
		FTableRowStyle Row = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
		const FSlateBrush Hover = FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.05f), 6.0f);
		Row.SetEvenRowBackgroundBrush(FSlateNoResource());
		Row.SetOddRowBackgroundBrush(FSlateNoResource());
		Row.SetEvenRowBackgroundHoveredBrush(Hover);
		Row.SetOddRowBackgroundHoveredBrush(Hover);
		Row.SetActiveBrush(FSlateNoResource());
		Row.SetActiveHoveredBrush(Hover);
		Row.SetInactiveBrush(FSlateNoResource());
		Row.SetInactiveHoveredBrush(Hover);
		Style->Set("Toolset.TableRow.List", Row);
	}

	// Pills / chips.
	Style->Set("Toolset.Pill",        new FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.06f), 10.0f));
	Style->Set("Toolset.Pill.Accent", new FSlateRoundedBoxBrush(AccentDim, 10.0f));

	// Tint-ready fills. BorderBackgroundColor *multiplies* a brush's own tint, so
	// a brush carrying any colour of its own can never be tinted to a different
	// one: a dark surface times a bright colour is just a dark colour. These are
	// pure white, so whatever colour a widget tints them with lands as authored.
	// Use these for anything whose colour is decided at runtime (severity
	// stripes, category swatches, the project size bar).
	Style->Set("Toolset.Fill",         new FSlateColorBrush(FLinearColor::White));
	Style->Set("Toolset.Fill.Rounded", new FSlateRoundedBoxBrush(FLinearColor::White, 2.0f));
	Style->Set("Toolset.Fill.Pill",    new FSlateRoundedBoxBrush(FLinearColor::White, 10.0f));

	// Nav item states.
	Style->Set("Toolset.Nav.Selected", new FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.16f), 6.0f));
	Style->Set("Toolset.Nav.Hover",    new FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.05f), 9.0f));
	Style->Set("Toolset.Nav.Active",   new FSlateRoundedBoxBrush(Accent, 9.0f));

	// Borderless nav button: transparent normal, faint hover, for the icon rail.
	{
		FButtonStyle NavButton;
		NavButton.SetNormal (FSlateNoResource());
		NavButton.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.06f), 9.0f));
		NavButton.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.03f), 9.0f));
		NavButton.SetNormalPadding(FMargin(0));
		NavButton.SetPressedPadding(FMargin(0));
		Style->Set("Toolset.Nav.Button", NavButton);
	}

	// Primary button.
	{
		FButtonStyle PrimaryButton;
		PrimaryButton.SetNormal (FSlateRoundedBoxBrush(Accent, 6.0f));
		PrimaryButton.SetHovered(FSlateRoundedBoxBrush(FLinearColor(Accent.R * 1.12f, Accent.G * 1.12f, Accent.B * 1.12f, 1.0f), 6.0f));
		PrimaryButton.SetPressed(FSlateRoundedBoxBrush(AccentDim, 6.0f));
		PrimaryButton.SetNormalPadding(FMargin(14, 7));
		PrimaryButton.SetPressedPadding(FMargin(14, 7));
		Style->Set("Toolset.Button.Primary", PrimaryButton);
	}

	// Scan button: the same accent as Primary, but padding-free so the chunky
	// square block in the sidebar is sized by its SBox rather than by the style
	// (SButton adds NormalPadding *on top of* ContentPadding).
	{
		FButtonStyle ScanButton;
		ScanButton.SetNormal (FSlateRoundedBoxBrush(Accent, 8.0f));
		ScanButton.SetHovered(FSlateRoundedBoxBrush(FLinearColor(Accent.R * 1.12f, Accent.G * 1.12f, Accent.B * 1.12f, 1.0f), 8.0f));
		ScanButton.SetPressed(FSlateRoundedBoxBrush(AccentDim, 8.0f));
		ScanButton.SetNormalPadding(FMargin(0));
		ScanButton.SetPressedPadding(FMargin(0));
		Style->Set("Toolset.Button.Scan", ScanButton);
	}

	// Numeric threshold entry, for the category settings rows. Stock SSpinBox is
	// pale and boxy; this gives it the card-inner surface and the teal fill so it
	// reads as part of the panel rather than a form field dropped onto it.
	{
		FSpinBoxStyle SpinBox;
		SpinBox.SetBackgroundBrush(FSlateRoundedBoxBrush(SurfacePanel, 6.0f, FLinearColor(FColor(0x3D, 0x42, 0x4A)), 1.0f));
		SpinBox.SetActiveBackgroundBrush(FSlateRoundedBoxBrush(SurfacePanel, 6.0f, FLinearColor(Accent.R, Accent.G, Accent.B, 0.6f), 1.0f));
		SpinBox.SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(SurfaceCardHover, 6.0f, FLinearColor(FColor(0x47, 0x4D, 0x55)), 1.0f));
		// The drag-fill portion of the slider, in accent.
		SpinBox.SetActiveFillBrush(FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.28f), 6.0f));
		SpinBox.SetHoveredFillBrush(FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.20f), 6.0f));
		SpinBox.SetInactiveFillBrush(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.0f), 6.0f));
		SpinBox.SetArrowsImage(FSlateNoResource());	// cleaner without the tiny up/down chevrons
		SpinBox.SetForegroundColor(FSlateColor(TextPrimary));
		SpinBox.SetTextPadding(FMargin(8, 4));
		Style->Set("Toolset.SpinBox", SpinBox);
	}

	// Ghost / secondary button.
	{
		FButtonStyle Ghost;
		Ghost.SetNormal (FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.05f), 6.0f, FLinearColor(1, 1, 1, 0.08f), 1.0f));
		Ghost.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.10f), 6.0f, FLinearColor(1, 1, 1, 0.12f), 1.0f));
		Ghost.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.03f), 6.0f));
		Ghost.SetNormalPadding(FMargin(12, 6));
		Ghost.SetPressedPadding(FMargin(12, 6));
		Style->Set("Toolset.Button.Ghost", Ghost);
	}

	// --- Text styles ---------------------------------------------------------
	const FString FontRegular = TEXT("Regular");
	const FString FontBold = TEXT("Bold");

	Style->Set("Toolset.Text.Title", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontBold, 18))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Toolset.Text.Heading", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontBold, 13))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Toolset.Text.Body", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontRegular, 10))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Toolset.Text.Subtle", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontRegular, 9))
		.SetColorAndOpacity(FSlateColor(TextSecondary)));

	Style->Set("Toolset.Text.Metric", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontBold, 30))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	// Between Heading and Metric: five of these sit in one row, so the big Metric
	// size would wrap a polycount and blow the card apart.
	Style->Set("Toolset.Text.StatValue", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontBold, 15))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Toolset.Text.NavLabel", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*FontBold, 11))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	// --- Section icons -------------------------------------------------------
	// SVGs are authored white so SImage.ColorAndOpacity can tint them per state.
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OptimizationToolset")))
	{
		const FString Res = Plugin->GetBaseDir() / TEXT("Resources");
		const FVector2D IconSize(18.0f, 18.0f);
		Style->Set("Toolset.Icon.Dashboard", new FSlateVectorImageBrush(Res / TEXT("dashboard.svg"), IconSize));
		Style->Set("Toolset.Icon.Analyze",   new FSlateVectorImageBrush(Res / TEXT("analyze.svg"),   IconSize));
		Style->Set("Toolset.Icon.Optimize",  new FSlateVectorImageBrush(Res / TEXT("optimize.svg"),  IconSize));
		Style->Set("Toolset.Icon.Profile",   new FSlateVectorImageBrush(Res / TEXT("profile.svg"),   IconSize));
		Style->Set("Toolset.Icon.Cleanup",   new FSlateVectorImageBrush(Res / TEXT("cleanup.svg"),   IconSize));
		Style->Set("Toolset.Icon.Reports",   new FSlateVectorImageBrush(Res / TEXT("reports.svg"),   IconSize));

		// Bigger: this one sits above a label on the scan button, not in a nav row.
		Style->Set("Toolset.Icon.Scan", new FSlateVectorImageBrush(Res / TEXT("scan.svg"), FVector2D(26.0f, 26.0f)));

		// Mascot (cropped portrait 515x1400 -> keep the 0.368 aspect).
		Style->Set("Toolset.Mascot", new FSlateImageBrush(Res / TEXT("vera.png"), FVector2D(184.0f, 500.0f)));
	}

	return Style;
}

FLinearColor FToolsetStyle::ColorForSeverity(ESeverity Severity)
{
	switch (Severity)
	{
	case ESeverity::Critical: return SeverityCritical;
	case ESeverity::Major:    return SeverityMajor;
	case ESeverity::Minor:    return SeverityMinor;
	default:                  return SeverityGood;
	}
}

FText FToolsetStyle::LabelForSeverity(ESeverity Severity)
{
	switch (Severity)
	{
	case ESeverity::Critical: return LOCTEXT("SevCritical", "Critical");
	case ESeverity::Major:    return LOCTEXT("SevMajor", "Major");
	case ESeverity::Minor:    return LOCTEXT("SevMinor", "Minor");
	default:                  return LOCTEXT("SevGood", "OK");
	}
}

FLinearColor FToolsetStyle::ColorForAssetCategory(EAssetCategory Category)
{
	// Nine hues that stay apart on the dark island background. Textures take the
	// mascot teal (they are usually the biggest slice), Animations her shoe
	// orange, Blueprints her amber eyes; Other stays deliberately grey so the
	// leftovers never look like a category worth chasing.
	switch (Category)
	{
	case EAssetCategory::Textures:       return FLinearColor(FColor(0x17, 0xB9, 0xA6));
	case EAssetCategory::StaticMeshes:   return FLinearColor(FColor(0x4A, 0xA3, 0xED));
	case EAssetCategory::SkeletalMeshes: return FLinearColor(FColor(0xA9, 0x7B, 0xF0));
	case EAssetCategory::Materials:      return FLinearColor(FColor(0xE8, 0x61, 0x9D));
	case EAssetCategory::Animations:     return FLinearColor(FColor(0xF0, 0x84, 0x2A));
	case EAssetCategory::Audio:          return FLinearColor(FColor(0x2E, 0xCC, 0x71));
	case EAssetCategory::Blueprints:     return FLinearColor(FColor(0xF5, 0xA7, 0x23));
	case EAssetCategory::Levels:         return FLinearColor(FColor(0x6C, 0x7B, 0xFF));
	default:                             return FLinearColor(FColor(0x7A, 0x82, 0x8C));
	}
}

FText FToolsetStyle::LabelForCategory(ECategory Category)
{
	switch (Category)
	{
	case ECategory::Meshes:     return LOCTEXT("CatMeshes", "Meshes");
	case ECategory::Materials:  return LOCTEXT("CatMaterials", "Materials");
	case ECategory::Textures:   return LOCTEXT("CatTextures", "Textures");
	case ECategory::Lighting:   return LOCTEXT("CatLighting", "Lighting");
	case ECategory::Collision:  return LOCTEXT("CatCollision", "Collision");
	case ECategory::Blueprints: return LOCTEXT("CatBlueprints", "Blueprints");
	default:                    return LOCTEXT("CatProject", "Project");
	}
}

#undef LOCTEXT_NAMESPACE
