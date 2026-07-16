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
  Nanite candidate, missing LODs, per-poly collision) and `FLightingPass`
  (too many movable lights), plus `FInstancingCandidatePass` (conservative
  groups of compatible repeated static-mesh actors).
- **Optimize** *(functional)* — lists findings that have a supported fix, with
  per-row **Apply** and an **Apply all** button; every fix is transactional
  (Undo/Redo) and the level auto-re-scans afterward. Fixes: `FEnableNaniteFix`,
  `FGenerateLODsFix`, `FSimpleCollisionFix`, `FReviewLightMobilityFix`.
- **Profile** *(functional)* — one-click `stat` command stacks (fps/unit/gpu/
  scenerendering/rhi/initviews/streaming/profilegpu/clear).

Placeholder panels (structured, list planned actions, not yet implemented):
- **Cleanup** — project size, fix redirectors, save all, delete unused, settings audit.
- **Reports** — CSV/JSON export, before/after snapshots.

## Roadmap / next steps

Ordered by suggested priority:

1. **Safe ISM/HISM conversion fix** for groups reported by
   `FInstancingCandidatePass`. Preserve transforms and require a final
   compatibility check before replacing actors.
2. **More analyze passes** — textures (oversized / non-pow2 / mip settings),
   materials (slot count, instructions), Blueprints.
3. **Cleanup panel** — project size breakdown, fix redirectors, delete unused,
   project-settings audit.
4. **Reports panel** — CSV/JSON export of `FScanResult`, before/after snapshots.
5. **Settings** — expose `FAnalyzeThresholds` via a settings UI (likely a
   `UDeveloperSettings`; if those enums become UPROPERTYs, reintroduce `UENUM`
   and qualify them, e.g. `EOptimizationSeverity` — see ARCHITECTURE.md).

## Build / run

1. Plugin lives at `<Project>/Plugins/OptimizationToolset`.
2. Build the editor target (or let the editor compile on launch / Live Coding).
3. Open **Optimization Toolset** from the toolbar or Window menu.
4. Smoke test: **Scan Level** → check Analyze findings → **Optimize** → **Apply**
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
