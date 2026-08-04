// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Slate/OptimizeStyle.h"
#include "Toolset/ToolsetTypes.h"
#include "Toolset/Cleanup/ProjectSizeReport.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "OptimizeStyle"

// ---- Palette ----------------------------------------------------------------
// Authored as sRGB hex, converted to linear (Slate brushes take linear colour).
// Flat, near-black Palatial surfaces in the mascot's teal scheme.
const FLinearColor FOptimizeStyle::Accent        = FLinearColor(FColor(0x17, 0xB9, 0xA6)); // teal
const FLinearColor FOptimizeStyle::AccentBright  = FLinearColor(FColor(0x2A, 0xD6, 0xC2));
const FLinearColor FOptimizeStyle::AccentDim     = FLinearColor(FColor(0x0E, 0x6E, 0x63));
const FLinearColor FOptimizeStyle::OnAccent      = FLinearColor(FColor(0x0C, 0x1A, 0x18));
const FLinearColor FOptimizeStyle::Window        = FLinearColor(FColor(0x12, 0x14, 0x16));
const FLinearColor FOptimizeStyle::Panel         = FLinearColor(FColor(0x18, 0x1B, 0x1E));
const FLinearColor FOptimizeStyle::Card          = FLinearColor(FColor(0x21, 0x25, 0x29));
const FLinearColor FOptimizeStyle::CardHover     = FLinearColor(FColor(0x2A, 0x2F, 0x35));
const FLinearColor FOptimizeStyle::Line          = FLinearColor(FColor(0x2E, 0x33, 0x3A));
const FLinearColor FOptimizeStyle::TextPrimary   = FLinearColor(FColor(0xE6, 0xE8, 0xEB));
const FLinearColor FOptimizeStyle::TextDim       = FLinearColor(FColor(0x8B, 0x90, 0x96));
const FLinearColor FOptimizeStyle::SeverityCritical = FLinearColor(FColor(0xEF, 0x4A, 0x3F));
const FLinearColor FOptimizeStyle::SeverityMajor    = FLinearColor(FColor(0xF5, 0xA7, 0x23));
const FLinearColor FOptimizeStyle::SeverityMinor    = FLinearColor(FColor(0x4A, 0xA3, 0xED));
const FLinearColor FOptimizeStyle::SeverityGood     = FLinearColor(FColor(0x2E, 0xCC, 0x71));

TSharedPtr<FSlateStyleSet> FOptimizeStyle::StyleInstance = nullptr;

void FOptimizeStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FOptimizeStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

const ISlateStyle& FOptimizeStyle::Get()
{
	return *StyleInstance;
}

FName FOptimizeStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("OptimizeUIStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FOptimizeStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	const float Radius = 4.0f;	// Palatial's corner radius, everywhere.

	// --- Surfaces ------------------------------------------------------------
	Style->Set("Opt.Window", new FSlateColorBrush(Window));
	Style->Set("Opt.Panel",  new FSlateColorBrush(Panel));
	Style->Set("Opt.Card",       new FSlateRoundedBoxBrush(Card, Radius));
	Style->Set("Opt.Card.Hover", new FSlateRoundedBoxBrush(CardHover, Radius));
	// A tile nested inside a card reads one step darker than its parent.
	Style->Set("Opt.Tile",       new FSlateRoundedBoxBrush(Window, Radius));
	Style->Set("Opt.Divider",    new FSlateColorBrush(Line));

	// Tint-ready white fills: BorderBackgroundColor multiplies a brush's own tint,
	// so anything coloured at runtime (severity dots, category swatches, bars)
	// must start from pure white.
	Style->Set("Opt.Fill",         new FSlateColorBrush(FLinearColor::White));
	Style->Set("Opt.Fill.Rounded", new FSlateRoundedBoxBrush(FLinearColor::White, 2.0f));

	// --- Table rows ----------------------------------------------------------
	// Transparent by default (the card/tile behind shows through), a faint teal
	// wash on hover, a stronger one when selected — Palatial's flat selected row,
	// recoloured from olive to our teal.
	{
		FTableRowStyle Row = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
		const FSlateBrush Hover    = FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.05f), Radius);
		const FSlateBrush Selected = FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.16f), Radius);
		Row.SetEvenRowBackgroundBrush(FSlateNoResource());
		Row.SetOddRowBackgroundBrush(FSlateNoResource());
		Row.SetEvenRowBackgroundHoveredBrush(Hover);
		Row.SetOddRowBackgroundHoveredBrush(Hover);
		Row.SetActiveBrush(Selected);
		Row.SetActiveHoveredBrush(Selected);
		Row.SetInactiveBrush(Selected);
		Row.SetInactiveHoveredBrush(Selected);
		Row.SetTextColor(FSlateColor(TextPrimary));
		Row.SetSelectedTextColor(FSlateColor(FLinearColor::White));
		Style->Set("Opt.TableRow", Row);
	}

	// The tree's own rounded backdrop, so it doesn't punch a hard-cornered hole.
	{
		FTableViewStyle TreeView = FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("TreeView");
		TreeView.SetBackgroundBrush(FSlateRoundedBoxBrush(Window, Radius));
		Style->Set("Opt.TreeView", TreeView);
	}

	// --- Buttons -------------------------------------------------------------
	// Primary: solid teal, brighter on hover, deep on press (Palatial's DefaultButtonStyle shape).
	{
		FButtonStyle Primary;
		Primary.SetNormal (FSlateRoundedBoxBrush(Accent, Radius));
		Primary.SetHovered(FSlateRoundedBoxBrush(AccentBright, Radius));
		Primary.SetPressed(FSlateRoundedBoxBrush(AccentDim, Radius));
		Primary.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), Radius));
		Primary.SetNormalPadding(FMargin(12, 7));
		Primary.SetPressedPadding(FMargin(12, 7));
		Style->Set("Opt.Button.Primary", Primary);
	}

	// Secondary: a raised control fill with a hairline edge so the button reads
	// clearly even when it sits on a same-toned card; teal on hover (Palatial's
	// ClickedButtonStyle shape).
	{
		const FLinearColor Control      = FLinearColor(FColor(0x34, 0x3A, 0x42));
		const FLinearColor ControlEdge   = FLinearColor(FColor(0x47, 0x4E, 0x57));
		const FLinearColor ControlHover  = FLinearColor(FColor(0x3D, 0x44, 0x4D));
		FButtonStyle Secondary;
		Secondary.SetNormal (FSlateRoundedBoxBrush(Control, Radius, ControlEdge, 1.0f));
		Secondary.SetHovered(FSlateRoundedBoxBrush(ControlHover, Radius, FLinearColor(Accent.R, Accent.G, Accent.B, 0.7f), 1.0f));
		Secondary.SetPressed(FSlateRoundedBoxBrush(Card, Radius, ControlEdge, 1.0f));
		Secondary.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), Radius));
		Secondary.SetNormalPadding(FMargin(12, 6));
		Secondary.SetPressedPadding(FMargin(12, 6));
		Style->Set("Opt.Button.Secondary", Secondary);
	}

	// Ghost: transparent, faint hover — for nav rows and inline icon buttons.
	{
		FButtonStyle Ghost;
		Ghost.SetNormal (FSlateNoResource());
		Ghost.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.06f), Radius));
		Ghost.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.03f), Radius));
		Ghost.SetNormalPadding(FMargin(10, 7));
		Ghost.SetPressedPadding(FMargin(10, 7));
		Style->Set("Opt.Button.Ghost", Ghost);
	}

	// Compact icon button: no padding of its own, so it fits inside a narrow fixed
	// column (the row magnifiers) without the icon spilling past the column edge.
	{
		FButtonStyle Icon;
		Icon.SetNormal (FSlateNoResource());
		Icon.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.08f), Radius));
		Icon.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.04f), Radius));
		Icon.SetNormalPadding(FMargin(0));
		Icon.SetPressedPadding(FMargin(0));
		Style->Set("Opt.Button.Icon", Icon);
	}

	// Numeric entry (threshold spin boxes on the Dashboard settings card): a raised
	// control fill with a hairline edge, accent when active, accent drag-fill.
	{
		const FLinearColor Control     = FLinearColor(FColor(0x2A, 0x2F, 0x35));
		const FLinearColor ControlEdge = FLinearColor(FColor(0x47, 0x4E, 0x57));
		FSpinBoxStyle Spin;
		Spin.SetBackgroundBrush(FSlateRoundedBoxBrush(Control, Radius, ControlEdge, 1.0f));
		Spin.SetActiveBackgroundBrush(FSlateRoundedBoxBrush(Control, Radius, FLinearColor(Accent.R, Accent.G, Accent.B, 0.8f), 1.0f));
		Spin.SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(CardHover, Radius, FLinearColor(Accent.R, Accent.G, Accent.B, 0.5f), 1.0f));
		Spin.SetActiveFillBrush(FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.30f), Radius));
		Spin.SetHoveredFillBrush(FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.22f), Radius));
		// Visible even when idle, so the row shows the value's fill by default.
		Spin.SetInactiveFillBrush(FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.14f), Radius));
		Spin.SetArrowsImage(FSlateNoResource());
		Spin.SetForegroundColor(FSlateColor(TextPrimary));
		Spin.SetTextPadding(FMargin(8, 4));
		Style->Set("Opt.SpinBox", Spin);
	}

	// Nav item selected background (a teal wash behind the active section).
	Style->Set("Opt.Nav.Selected", new FSlateRoundedBoxBrush(FLinearColor(Accent.R, Accent.G, Accent.B, 0.16f), Radius));

	// Nav row button: no padding of its own (the inner highlight border owns it),
	// transparent normal, a faint hover for rows that aren't the active section.
	{
		FButtonStyle Nav;
		Nav.SetNormal (FSlateNoResource());
		Nav.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.05f), Radius));
		Nav.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.03f), Radius));
		Nav.SetNormalPadding(FMargin(0));
		Nav.SetPressedPadding(FMargin(0));
		Style->Set("Opt.Nav.Button", Nav);
	}

	// --- Text styles ---------------------------------------------------------
	const FString Reg = TEXT("Regular");
	const FString Bold = TEXT("Bold");

	Style->Set("Opt.Text.Title", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Bold, 16))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Opt.Text.Heading", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Bold, 12))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Opt.Text.Body", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Reg, 10))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Opt.Text.Subtle", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Reg, 9))
		.SetColorAndOpacity(FSlateColor(TextDim)));

	// Big dashboard numbers.
	Style->Set("Opt.Text.Metric", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Bold, 26))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	// Between Heading and Metric: five stat tiles share one row.
	Style->Set("Opt.Text.StatValue", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Bold, 16))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("Opt.Text.NavLabel", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle(*Bold, 11))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	// --- Plugin resources ----------------------------------------------------
	// SVGs are authored white so their tint follows the current nav state.
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OptimizationToolset")))
	{
		const FString Resources = Plugin->GetBaseDir() / TEXT("Resources");
		const FVector2D NavIconSize(18.0f, 18.0f);
		Style->Set("Opt.Icon.Dashboard", new FSlateVectorImageBrush(Resources / TEXT("dashboard.svg"), NavIconSize));
		Style->Set("Opt.Icon.Analyze",   new FSlateVectorImageBrush(Resources / TEXT("analyze.svg"), NavIconSize));
		Style->Set("Opt.Icon.Optimize",  new FSlateVectorImageBrush(Resources / TEXT("optimize.svg"), NavIconSize));
		Style->Set("Opt.Icon.Profile",   new FSlateVectorImageBrush(Resources / TEXT("profile.svg"), NavIconSize));
		Style->Set("Opt.Icon.Cleanup",   new FSlateVectorImageBrush(Resources / TEXT("cleanup.svg"), NavIconSize));
		Style->Set("Opt.Icon.Scan", new FSlateVectorImageBrush(Resources / TEXT("scan.svg"), FVector2D(26.0f, 26.0f)));
		Style->Set("Opt.Mascot", new FSlateImageBrush(Resources / TEXT("vera.png"), FVector2D(184.0f, 500.0f)));
	}

	return Style;
}

FLinearColor FOptimizeStyle::ColorForSeverity(ESeverity Severity)
{
	switch (Severity)
	{
	case ESeverity::Critical: return SeverityCritical;
	case ESeverity::Major:    return SeverityMajor;
	case ESeverity::Minor:    return SeverityMinor;
	default:                  return SeverityGood;
	}
}

FText FOptimizeStyle::LabelForSeverity(ESeverity Severity)
{
	switch (Severity)
	{
	case ESeverity::Critical: return LOCTEXT("SevCritical", "Critical");
	case ESeverity::Major:    return LOCTEXT("SevMajor", "Major");
	case ESeverity::Minor:    return LOCTEXT("SevMinor", "Minor");
	default:                  return LOCTEXT("SevGood", "OK");
	}
}

FLinearColor FOptimizeStyle::ColorForAssetCategory(EAssetCategory Category)
{
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

#undef LOCTEXT_NAMESPACE
