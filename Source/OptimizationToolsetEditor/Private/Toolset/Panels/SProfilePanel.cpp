// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SProfilePanel.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SProfilePanel"

using namespace ToolsetUI;

namespace
{
	/**
	 * Command strings are verified against the UE 5.7 source, not guessed — a wrong
	 * one fails silently, which is the worst possible failure for a button whose
	 * whole job is to change what you see. Where they came from:
	 *   r.Nanite.Visualize / r.Nanite.VisualizeOverview  NaniteVisualizationData.h
	 *   r.Lumen.Visualize.ViewMode                       LumenVisualizationData.h
	 *   r.Shadow.Virtual.Visualize                       VirtualShadowMapVisualizationData.h
	 * and the mode names from each one's AddVisualizationMode() table.
	 *
	 * The view modes matter as much as the cvars. Setting a channel only picks
	 * *which* thing is drawn; the viewport still has to be in the matching
	 * visualization view mode to draw it at all. Nanite is the exception — its
	 * Update() sets bForceShowFlag when the cvar is set from the console — but we
	 * set the view mode anyway so every button behaves the same way.
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

	/** Pure view modes: no channel to pick, the mode *is* the visualization. */
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
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void SProfilePanel::ApplyViewMode(EViewModeIndex ViewMode)
{
	if (ViewMode == VMI_Unknown || !GEditor)
	{
		return;
	}

	// The viewport the user is actually looking at. Falling back to every level
	// viewport rather than doing nothing: with no viewport focused (clicking
	// straight from this panel is exactly that case) a silent no-op would read as
	// a broken button.
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

void SProfilePanel::RunAction(const FProfileAction& Action)
{
	// Command first: it picks the channel the view mode is about to display.
	if (!Action.Command.IsEmpty() && GEditor)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		GEditor->Exec(World, *Action.Command);
	}

	ApplyViewMode(Action.ViewMode);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void SProfilePanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(FMargin(22, 20))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 14))
			[
				MakeSection(
					LOCTEXT("SecStats", "Stat stacks"),
					LOCTEXT("SecStatsHint", "Console stat overlays for the active viewport or PIE session."),
					MakeStatActions(), 5)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 14))
			[
				MakeSection(
					LOCTEXT("SecComplexity", "Complexity view modes"),
					LOCTEXT("SecComplexityHint", "Where the frame is being spent, per pixel. Reset to Lit when you're done."),
					MakeComplexityActions(), 3)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 14))
			[
				MakeSection(
					LOCTEXT("SecNanite", "Nanite"),
					LOCTEXT("SecNaniteHint", "Cluster complexity and instance density. Needs Nanite meshes in view."),
					MakeNaniteActions(), 4)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 14))
			[
				MakeSection(
					LOCTEXT("SecLumen", "Lumen"),
					LOCTEXT("SecLumenHint", "Global illumination and reflection diagnostics. Needs Lumen enabled in project settings."),
					MakeLumenActions(), 3)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 14))
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

TSharedRef<SWidget> SProfilePanel::MakeSection(const FText& Title, const FText& Hint, const TArray<FProfileAction>& Actions, int32 Columns)
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4));
	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		Grid->AddSlot(Index % Columns, Index / Columns)
		[
			MakeActionButton(Actions[Index])
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(18, 16))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(Title)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 4, 0, 12))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").AutoWrapText(true).Text(Hint)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Grid
			]
		];
}

TSharedRef<SWidget> SProfilePanel::MakeActionButton(const FProfileAction& Action)
{
	// Falls back to showing the raw command: for a tool aimed at people who know
	// the console, naming the cvar is documentation, not clutter.
	const FText Tooltip = Action.Tooltip.IsEmpty()
		? FText::FromString(Action.Command)
		: Action.Tooltip;

	return SNew(SButton)
		.ButtonStyle(&S(), "Toolset.Button.Ghost")
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(10, 12))
		.ToolTipText(Tooltip)
		.OnClicked_Lambda([Action]()
		{
			RunAction(Action);
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.TextStyle(&S(), "Toolset.Text.NavLabel")
			.Text(Action.Label)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SProfilePanel::BuildCustomCommandCard()
{
	return SNew(SBorder)
		.BorderImage(Brush("Toolset.Card"))
		.Padding(FMargin(18, 16))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Heading").Text(LOCTEXT("SecCustom", "Console command"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 4, 0, 12))
			[
				SNew(STextBlock).TextStyle(&S(), "Toolset.Text.Subtle").AutoWrapText(true)
				.Text(LOCTEXT("SecCustomHint", "Anything the buttons above don't cover, run against the editor world."))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("CustomHint", "e.g. r.ScreenPercentage 50"))
					.OnTextChanged_Lambda([this](const FText& NewText) { CustomCommand = NewText.ToString(); })
					// Enter runs it, so the button is a hint rather than the only way.
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
					.ButtonStyle(&S(), "Toolset.Button.Primary")
					.OnClicked_Lambda([this]()
					{
						RunAction({ FText::GetEmpty(), CustomCommand });
						return FReply::Handled();
					})
					[
						SNew(STextBlock).TextStyle(&S(), "Toolset.Text.NavLabel")
						.ColorAndOpacity(FSlateColor(FToolsetStyle::OnAccent))
						.Text(LOCTEXT("RunCmd", "Run"))
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
