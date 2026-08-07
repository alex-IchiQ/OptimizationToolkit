// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#include "Toolset/Slate/SProfileView.h"
#include "Toolset/Slate/OptimizeStyle.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EditorViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "LevelEditorViewport.h"
#include "UnrealClient.h"	// GStatProcessingViewportClient
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SProfileView"

namespace
{
	/**
	 * Command strings verified against UE 5.7 source, not guessed: a wrong string
	 * fails silently, the worst failure for a button whose whole job is to change
	 * what you see. Origins: r.Nanite.Visualize (NaniteVisualizationData.h),
	 * r.Lumen.Visualize.ViewMode (LumenVisualizationData.h), r.Shadow.Virtual.Visualize
	 * (VirtualShadowMapVisualizationData.h), with mode names from each AddVisualizationMode().
	 */
	TArray<FProfileAction> MakeStatActions()
	{
		return {
			{ LOCTEXT("StatFPS", "FPS"),           TEXT("stat fps") },
			{ LOCTEXT("StatUnit", "Unit"),         TEXT("stat unit") },
			{ LOCTEXT("StatGPU", "GPU"),           TEXT("stat gpu") },
			{ LOCTEXT("StatRHI", "RHI"),           TEXT("stat rhi") },
			{ LOCTEXT("StatScene", "Scene Rendering"), TEXT("stat scenerendering") },
			{ LOCTEXT("StatInitViews", "Init Views"),  TEXT("stat initviews") },
			{ LOCTEXT("StatStreaming", "Streaming"),   TEXT("stat streaming") },
			{ LOCTEXT("StatMemory", "Memory"),         TEXT("stat memory") },
			{ LOCTEXT("StatGPUProfile", "GPU Profiler"), TEXT("profilegpu") },
			{ LOCTEXT("StatNone", "Clear stats"),      TEXT("stat none") },
		};
	}

	TArray<FProfileAction> MakeComplexityActions()
	{
		return {
			{ LOCTEXT("VmLightComplexity", "Light Complexity"), FString(), VMI_LightComplexity,
				LOCTEXT("VmLightComplexityTip", "Lighting cost per pixel: how many lights affect each surface.") },
			{ LOCTEXT("VmShaderComplexity", "Shader Complexity"), FString(), VMI_ShaderComplexity },
			{ LOCTEXT("VmQuadOverdraw", "Quad Overdraw"), FString(), VMI_QuadOverdraw,
				LOCTEXT("VmQuadOverdrawTip", "Wasted rasterization from dense or thin geometry.") },
			{ LOCTEXT("VmLightmapDensity", "Lightmap Density"), FString(), VMI_LightmapDensity },
			{ LOCTEXT("VmStationaryOverlap", "Stationary Light Overlap"), FString(), VMI_StationaryLightOverlap,
				LOCTEXT("VmStationaryOverlapTip", "Stationary lights past the overlap limit fall back to dynamic shadows.") },
			{ LOCTEXT("VmLit", "Reset to Lit"), FString(), VMI_Lit },
		};
	}

	TArray<FProfileAction> MakeNaniteActions()
	{
		return {
			{ LOCTEXT("NanOverview", "Overview"),   TEXT("r.Nanite.Visualize Overview"),   VMI_VisualizeNanite },
			{ LOCTEXT("NanTriangles", "Triangles"), TEXT("r.Nanite.Visualize Triangles"),  VMI_VisualizeNanite },
			{ LOCTEXT("NanClusters", "Clusters"),   TEXT("r.Nanite.Visualize Clusters"),   VMI_VisualizeNanite },
			{ LOCTEXT("NanInstances", "Instances"), TEXT("r.Nanite.Visualize Instances"),  VMI_VisualizeNanite },
			{ LOCTEXT("NanOverdraw", "Overdraw"),   TEXT("r.Nanite.Visualize Overdraw"),   VMI_VisualizeNanite },
			{ LOCTEXT("NanMask", "Nanite Mask"),    TEXT("r.Nanite.Visualize Mask"),       VMI_VisualizeNanite,
				LOCTEXT("NanMaskTip", "Which pixels Nanite rasterized, and which fell back to the classic path.") },
			{ LOCTEXT("NanRaster", "Raster Mode"),  TEXT("r.Nanite.Visualize RasterMode"), VMI_VisualizeNanite },
			{ LOCTEXT("NanPicking", "Picking"),     TEXT("r.Nanite.Visualize Picking"),    VMI_VisualizeNanite,
				LOCTEXT("NanPickingTip", "Inspect the Nanite primitive under the crosshair (r.Nanite.Picking.Crosshair).") },
		};
	}

	TArray<FProfileAction> MakeLumenActions()
	{
		return {
			{ LOCTEXT("LumOverview", "Overview"),        TEXT("r.Lumen.Visualize.ViewMode Overview"),            VMI_VisualizeLumen },
			{ LOCTEXT("LumPerf", "Performance"),         TEXT("r.Lumen.Visualize.ViewMode PerformanceOverview"), VMI_VisualizeLumen },
			{ LOCTEXT("LumScene", "Lumen Scene"),        TEXT("r.Lumen.Visualize.ViewMode LumenScene"),          VMI_VisualizeLumen },
			{ LOCTEXT("LumReflect", "Reflection View"),  TEXT("r.Lumen.Visualize.ViewMode ReflectionView"),      VMI_VisualizeLumen },
			{ LOCTEXT("LumSurface", "Surface Cache"),    TEXT("r.Lumen.Visualize.ViewMode SurfaceCache"),        VMI_VisualizeLumen,
				LOCTEXT("LumSurfaceTip", "Pink is missing surface cache coverage; yellow is culled meshes.") },
			{ LOCTEXT("LumNormals", "Geometry Normals"), TEXT("r.Lumen.Visualize.ViewMode GeometryNormals"),     VMI_VisualizeLumen },
		};
	}

	TArray<FProfileAction> MakeShadowActions()
	{
		return {
			{ LOCTEXT("VsmMask", "VSM: Shadow Mask"),  TEXT("r.Shadow.Virtual.Visualize mask"),  VMI_VisualizeVirtualShadowMap },
			{ LOCTEXT("VsmMip", "VSM: Clipmap/Mip"),   TEXT("r.Shadow.Virtual.Visualize mip"),   VMI_VisualizeVirtualShadowMap },
			{ LOCTEXT("VsmPage", "VSM: Virtual Page"), TEXT("r.Shadow.Virtual.Visualize vpage"), VMI_VisualizeVirtualShadowMap },
			{ LOCTEXT("VsmCache", "VSM: Cached Page"), TEXT("r.Shadow.Virtual.Visualize cache"), VMI_VisualizeVirtualShadowMap,
				LOCTEXT("VsmCacheTip", "Uncached pages are re-rendered every frame — this is where VSM cost hides.") },
		};
	}
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void SProfileView::ApplyViewMode(EViewModeIndex ViewMode)
{
	if (ViewMode == VMI_Unknown || !GEditor)
	{
		return;
	}

	// The viewport the user is actually looking at, falling back to every level
	// viewport so a click straight from this page (nothing focused) still lands.
	if (GCurrentLevelEditingViewportClient)
	{
		GCurrentLevelEditingViewportClient->SetViewMode(ViewMode);
		GCurrentLevelEditingViewportClient->Invalidate();
		return;
	}
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client)
		{
			Client->SetViewMode(ViewMode);
			Client->Invalidate();
		}
	}
}

void SProfileView::RunAction(const FProfileAction& Action)
{
	if (GEditor)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();

		// Nanite's visualization cvar force-enables its show flag independent of the
		// view mode (its Update() sets bForceShowFlag when the cvar is set), so it
		// keeps drawing after you switch away — even to Lit — until it is explicitly
		// turned off. Clear it when switching view modes; a Nanite action re-sets it next.
		if (Action.ViewMode != VMI_Unknown)
		{
			GEditor->Exec(World, TEXT("r.Nanite.Visualize off"));
		}

		// Then the action's own command picks the channel the view mode will display.
		if (!Action.Command.IsEmpty())
		{
			// A stat only registers in a viewport's EnabledStats (what the highlight
			// reads back) if that viewport is the stat processor when the toggle
			// fires. A bare Exec from a button leaves that pointing nowhere useful,
			// so aim it at the viewport the user is looking at first.
			if (Action.Command.StartsWith(TEXT("stat ")) && GCurrentLevelEditingViewportClient)
			{
				GStatProcessingViewportClient = GCurrentLevelEditingViewportClient;
			}
			GEditor->Exec(World, *Action.Command);
		}
	}
	ApplyViewMode(Action.ViewMode);
}

bool SProfileView::IsActionActive(const FProfileAction& Action)
{
	if (!GEditor)
	{
		return false;
	}

	// Stat toggles: active while that stat overlay is on. A stat run through Exec
	// may land on whichever level viewport is active, so check them all rather than
	// only the focused one. "stat none" clears, so it is a verb, not a state.
	//
	// Note: only engine stats (fps, unit) land in EnabledStats; stat *groups* (gpu,
	// rhi, scenerendering…) go through a different system and won't light up here.
	if (Action.Command.StartsWith(TEXT("stat ")))
	{
		const FString Name = Action.Command.RightChop(5).TrimStartAndEnd();
		if (Name == TEXT("none"))
		{
			return false;
		}
		for (const FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
		{
			if (Client && Client->IsStatEnabled(Name))
			{
				return true;
			}
		}
		return false;
	}

	if (Action.ViewMode != VMI_Unknown)
	{
		const FEditorViewportClient* Viewport = GCurrentLevelEditingViewportClient;
		if (!Viewport)
		{
			return false;
		}
		// Reset to Lit is the way *out* of a visualization, not a state to advertise.
		if (Action.ViewMode == VMI_Lit || Viewport->GetViewMode() != Action.ViewMode)
		{
			return false;
		}

		// A channel action (r.X.Visualize Mode) also has to match the live cvar value,
		// so only the one selected channel lights up, not every button in the group.
		FString CvarName;
		FString Mode;
		if (Action.Command.StartsWith(TEXT("r.")) && Action.Command.Split(TEXT(" "), &CvarName, &Mode))
		{
			const IConsoleVariable* Cvar = IConsoleManager::Get().FindConsoleVariable(*CvarName);
			return Cvar && Cvar->GetString().Equals(Mode.TrimStartAndEnd(), ESearchCase::IgnoreCase);
		}
		return true;	// a pure view mode, and it matched
	}

	return false;	// profilegpu, custom Run: fire-and-forget, no state
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void SProfileView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(12, 10))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 12))
			[
				MakeSection(
					LOCTEXT("SecStats", "Stat stacks"),
					LOCTEXT("SecStatsHint", "Console stat overlays for the active viewport or PIE session."),
					MakeStatActions(), 5)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 12))
			[
				MakeSection(
					LOCTEXT("SecComplexity", "Complexity view modes"),
					LOCTEXT("SecComplexityHint", "Where the frame is being spent, per pixel. Reset to Lit when you're done."),
					MakeComplexityActions(), 3)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 12))
			[
				MakeSection(
					LOCTEXT("SecNanite", "Nanite"),
					LOCTEXT("SecNaniteHint", "Cluster complexity and instance density. Needs Nanite meshes in view."),
					MakeNaniteActions(), 4)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 12))
			[
				MakeSection(
					LOCTEXT("SecLumen", "Lumen"),
					LOCTEXT("SecLumenHint", "Global illumination and reflection diagnostics. Needs Lumen enabled."),
					MakeLumenActions(), 3)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 12))
			[
				MakeSection(
					LOCTEXT("SecShadows", "Virtual shadow maps"),
					LOCTEXT("SecShadowsHint", "Needs Shadow Map Method set to Virtual Shadow Maps."),
					MakeShadowActions(), 4)
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildCustomCommandCard()
			]
		]
	];
}

TSharedRef<SWidget> SProfileView::MakeSection(const FText& Title, const FText& Hint, const TArray<FProfileAction>& Actions, int32 Columns)
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(3));
	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		Grid->AddSlot(Index % Columns, Index / Columns)
		[
			MakeActionButton(Actions[Index])
		];
	}

	return SNew(SBorder)
		.BorderImage(FOptimizeStyle::Brush("Opt.Card"))
		.Padding(FMargin(16, 14))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Heading").Text(Title)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3, 0, 12))
			[
				SNew(STextBlock).TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle").Text(Hint).AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Grid
			]
		];
}

TSharedRef<SWidget> SProfileView::MakeActionButton(const FProfileAction& Action)
{
	// Falls back to the raw command as the tooltip — for a console-literate audience
	// that names the cvar rather than hiding it.
	const FText Tooltip = Action.Tooltip.IsEmpty() ? FText::FromString(Action.Command) : Action.Tooltip;

	return SNew(SButton)
		.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Button.Secondary")
		.HAlign(HAlign_Center)
		.ToolTipText(Tooltip)
		.OnClicked_Lambda([Action]() { RunAction(Action); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Body")
			.Text(Action.Label)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			// Active actions read in accent so it's visible at a glance what the
			// viewport is currently showing.
			.ColorAndOpacity_Lambda([Action]()
			{
				return IsActionActive(Action) ? FSlateColor(FOptimizeStyle::Accent) : FSlateColor(FOptimizeStyle::TextPrimary);
			})
		];
}

TSharedRef<SWidget> SProfileView::BuildCustomCommandCard()
{
	return SNew(SBorder)
		.BorderImage(FOptimizeStyle::Brush("Opt.Card"))
		.Padding(FMargin(16, 14))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Heading")
				.Text(LOCTEXT("SecCustom", "Console command"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3, 0, 12))
			[
				SNew(STextBlock)
				.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.Subtle")
				.Text(LOCTEXT("SecCustomHint", "Anything the buttons above don't cover, run against the editor world."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("CustomHint", "e.g. r.ScreenPercentage 50"))
					.OnTextChanged_Lambda([this](const FText& NewText) { CustomCommand = NewText.ToString(); })
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
					{
						CustomCommand = NewText.ToString();
						if (CommitType == ETextCommit::OnEnter)
						{
							RunAction({ FText::GetEmpty(), CustomCommand });
						}
					})
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
				[
					SNew(SButton)
					.ButtonStyle(&FOptimizeStyle::Get(), "Opt.Button.Primary")
					.OnClicked_Lambda([this]() { RunAction({ FText::GetEmpty(), CustomCommand }); return FReply::Handled(); })
					[
						SNew(STextBlock)
						.TextStyle(&FOptimizeStyle::Get(), "Opt.Text.NavLabel")
						.ColorAndOpacity(FSlateColor(FOptimizeStyle::OnAccent))
						.Text(LOCTEXT("RunCmd", "Run"))
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
