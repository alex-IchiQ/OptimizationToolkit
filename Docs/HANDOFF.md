# Optimization / Profiling Toolset — Handoff

Continuation brief for picking this project up in a fresh session. Read this
plus [ARCHITECTURE.md](ARCHITECTURE.md) before making changes.

## What this is

An **editor-only Unreal Engine plugin** sold on Epic's **FAB** marketplace. It
lets a dev **analyze, optimize and profile the current level** from one dockable
Slate panel.

**Market positioning** (from analyzing 5 FAB competitors — HXS Optimizer,
Perfector: Level, MeoPlay Level & Graphic Tools, Palatial, Ultimate
Optimization): nobody combines all three of *severity-based analysis* +
*safe Undo-able auto-fixes* + *profiling* in one place across a wide engine
range at a mid price. That gap is our product:
- Analyze **and** fix (Perfector only advises; HXS fixes but is 5.6-only beta, ~$230–330).
- Wide UE support, fair mid price ($40–70 target), no "beta" label.

## Locked-in decisions

| Decision | Value |
|---|---|
| Target engine | **UE 5.3 – 5.7** (one source tree; per-version features gated by `OPTIMIZATION_*` macros; each FAB build ships one engine version) |
| Core | **C++ editor module + custom Slate** (premium, not Editor Utility Widgets) |
| Dev/test location | `E:\Projects\PluginTest\Plugins\OptimizationToolset` (real UE project used to compile) |
| Scope | Full product in mind (Dashboard/Analyze/Optimize/Profile/Cleanup/Reports); built scaffold-first |

## Current status

**Compiles and runs in UE.** Window opens via the **Optimize** toolbar button or
**Window → Optimization Toolset**.

Working:
- **Dashboard** — severity summary cards + workflow guide.
- **Analyze** *(functional)* — runs registered passes; findings with severity
  (Critical/Major/Minor), category filter, search, and a **Focus** button that
  frames the offending actor. Passes: `FStaticMeshPass` (excessive triangles,
  Nanite candidate, missing LODs, per-poly collision), `FTexturePass`
  (oversized, non-power-of-two, and missing mipmaps), `FMaterialPass` (slot
  count, empty/duplicate assignments, translucency, and two-sided review),
  `FLightingPass` (too many movable lights), `FInstancingCandidatePass`
  (conservative groups of compatible repeated static-mesh actors),
  `FProjectSettingsPass` (rendering settings that cost everywhere; ignores the
  level), and `FBlueprintTickPass` (static Blueprint actors ticking every frame,
  reported per class). The last two carry no FixId on purpose — both would have
  to rewrite something shared (DefaultEngine.ini, a Blueprint's class defaults)
  from a panel that only scanned one level.
- **Optimize** *(functional)* — lists findings that have a supported fix, with
  per-row **Apply** and an **Apply all** button; every fix is transactional
  (Undo/Redo) and the level auto-re-scans afterward. Fixes: `FEnableNaniteFix`,
  `FGenerateLODsFix`, `FSimpleCollisionFix`, `FReviewLightMobilityFix`,
  `FConvertToInstancesFix` (replaces a vetted group of repeated static-mesh
  actors with one HISM actor; reads the group from `FFinding::RelatedActors`),
  `FNormalmapCompressionFix` and `FDisableTextureSRGBFix` (edit the texture named
  by `FFinding::TargetAsset`).
- **Profile** *(functional)* — one-click `stat` command stacks (fps/unit/gpu/
  scenerendering/rhi/initviews/streaming/profilegpu/clear).
- **Settings** *(functional)* — project-wide analyze thresholds under
  **Project Settings → Plugins → Optimization Toolset**.

- **Cleanup** *(functional)* — a **project size** card (on-demand `FProjectSizeReport`:
  measures every package under /Game on disk, grouped by asset class, top 10 with
  bars plus a rolled-up tail), then registry-driven project-wide actions, each a
  card with a Run button and a last-run summary. Destructive ones are tagged
  "NOT UNDOABLE" and confirm first, unless they run a better review themselves
  (`NeedsConfirmation()`). Actions: `FSaveDirtyPackagesAction`,
  `FFixUpRedirectorsAction`, `FDeleteUnusedAssetsAction`.

Placeholder panels (structured, list planned actions, not yet implemented):
- **Reports** — CSV/JSON export, before/after snapshots.

## Roadmap / next steps

Ordered by suggested priority:

1. **Preview before apply** — currently Apply is a leap of faith. Worth a shared
   list-with-checkboxes widget so a user can untick individual actors/assets
   before ISM conversion or a delete. Also: `Fix_ConvertToInstances` is
   structural and currently runs under "Apply all" like any other fix; consider
   flagging structural fixes so bulk apply doesn't restructure a level silently.
2. **More analyze passes** — shader instruction counts (version/platform-aware).
4. **Reports panel** — CSV/JSON export of `FScanResult`, before/after snapshots.
   `FFinding::TypeId` is the stable column to group and diff on.
5. **Ship prep** — `Resources/Icon128.png` is missing (plugin browser + FAB), and
   `.uplugin` has empty `CreatedBy` / `DocsURL` / `SupportURL`.

## Build / run

1. Plugin lives at `<Project>/Plugins/OptimizationToolset`.
2. Build the editor target (or let the editor compile on launch / Live Coding).
3. Open **Optimization Toolset** from the toolbar or Window menu.
4. Optional: adjust thresholds in **Project Settings → Plugins → Optimization
   Toolset**.
5. Smoke test: **Scan Level** → check Analyze findings → **Optimize** → **Apply**
   on a finding → confirm the change (Nanite, LOD1–3, simple collision, or light
   mobility) and that **Ctrl+Z** reverts it.

## Gotchas already hit (don't rediscover these)

- **Slate colours are linear.** Author palette as sRGB hex via
  `FLinearColor(FColor(0x..))` or the UI washes out to grey.
- **dllexport + move-only members.** Don't put `MODULE_API` on a class holding
  `TArray<TUniquePtr<...>>` (MSVC forces the copy ctor → deleted-function error).
  `FToolsetRegistry` is intentionally *not* exported.
- **SVG icons must be white** (`#FFFFFF`) so `SImage.ColorAndOpacity` can tint
  them per state. Source art came in salmon; it was recoloured to white.
- **Include paths that bit us:** `FSlateVectorImageBrush`/`FSlateImageBrush` are in
  `Brushes/SlateImageBrush.h`; `IPlugin` is in `Interfaces/IPluginManager.h` (no
  separate `IPlugin.h`).
- **Risky version-sensitive APIs:** `UStaticMesh::NaniteSettings.bEnabled`
  (direct member; fall back to `SetNaniteSettings()` if a version rejects it) and
  `UStaticMeshEditorSubsystem::SetLodsWithNotification` / `FStaticMeshReductionOptions`
  (module `StaticMeshEditor`; name may differ across versions).
