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
- **Analyze** *(functional)* — runs registered passes. Findings are grouped under
  category headers in an `STreeView` (each header carries a count and a dot in the
  group's worst severity, so a collapsed group still says whether it's worth
  opening). Above that: severity toggles, search, and a **Focus** button per row
  that frames the offending actor. Passes: `FStaticMeshPass` (excessive triangles,
  Nanite candidate, missing LODs, per-poly collision), `FTexturePass`
  (oversized, non-power-of-two, and missing mipmaps), `FMaterialPass` (slot
  count, empty/duplicate assignments, translucency, and two-sided review),
  `FLightingPass` (too many movable lights), `FInstancingCandidatePass`
  (conservative groups of compatible repeated static-mesh actors),
  `FProjectSettingsPass` (rendering settings that cost everywhere; ignores the
  level), `FBlueprintTickPass` (static Blueprint actors ticking every frame,
  reported per class), `FTextureCompressionPass` (asks materials which textures
  feed Normal / Roughness / Metallic / AO, then checks compression and sRGB
  against that role), and `FBlueprintDependencyPass` (walks each Blueprint's hard
  reference chain and reports what it drags in, naming the heaviest assets —
  the Size Map walk, run automatically across the level). The settings and tick
  passes carry no FixId on purpose — both would have to rewrite something shared
  (DefaultEngine.ini, a Blueprint's class defaults) from a panel that only
  scanned one level.

  `FBlueprintDependencyPass` needs the `AssetManagerEditor` plugin (declared in
  the .uplugin; it is `EnabledByDefault` in the engine). Sizes come from
  `IAssetManagerEditorModule::GetIntegerValueForCustomColumn(DiskSizeName)`, which
  reads the current registry source — the editor's own registry by default. For
  real shipped numbers the source has to be pointed at a cooked
  DevelopmentAssetRegistry via `SetCurrentRegistrySource`; that selector is what
  a "Final Packaging" size mode would be built on.
- **Optimize** *(functional)* — the findings that have a supported fix, grouped by
  category the same way, with per-row **Apply** and an **Apply all** button. Every
  fix is transactional (Undo/Redo) and the level auto-re-scans afterward. Fixes:
  `FEnableNaniteFix`,
  `FGenerateLODsFix`, `FSimpleCollisionFix`, `FReviewLightMobilityFix`,
  `FConvertToInstancesFix` (replaces a vetted group of repeated static-mesh
  actors with one HISM actor; reads the group from `FFinding::RelatedActors`),
  `FNormalmapCompressionFix` and `FDisableTextureSRGBFix` (edit the texture named
  by `FFinding::TargetAsset`).
- **Profile** *(functional)* — one-click `stat` command stacks (fps/unit/gpu/
  scenerendering/rhi/initviews/streaming/profilegpu/clear).
- **Settings** *(functional)* — project-wide analyze thresholds under
  **Project Settings → Plugins → Optimization Toolset**.

- **Cleanup** *(functional)* — a **project size** card (on-demand
  `FProjectSizeReport`: measures every package under /Game on disk, buckets raw
  asset classes into `EAssetCategory` — textures, static/skeletal meshes,
  materials, animations, audio, blueprints, levels, other — and draws them as one
  stacked bar plus a legend, so the whole footprint is on screen with nothing
  truncated). Then registry-driven project-wide actions, each a card with a Run
  button and a last-run summary. Destructive ones are tagged "NOT UNDOABLE" and
  confirm first, unless they run a better review themselves (`NeedsConfirmation()`).
  Actions: `FSaveDirtyPackagesAction`, `FFixUpRedirectorsAction`,
  `FDeleteUnusedAssetsAction`.

Placeholder panels (structured, list planned actions, not yet implemented):
- **Reports** — CSV/JSON export, before/after snapshots.

## Roadmap / next steps

Ordered by suggested priority:

1. **Reports panel** — the last placeholder. CSV/JSON export of `FScanResult`,
   before/after snapshots. `FFinding::TypeId` is the stable column to group and
   diff on. Deferred deliberately until the other features landed.
2. **Preview before apply** — Apply is currently a leap of faith. Worth a shared
   list-with-checkboxes widget so a user can untick individual actors/assets
   before ISM conversion or a delete. Also: `Fix_ConvertToInstances` is
   structural and runs under "Apply all" like any other fix; consider flagging
   structural fixes so bulk apply can't restructure a level silently.
3. **Delete unused, remaining polish** (ideas confirmed against the shipped
   Assets Cleaner plugin): delete now-empty folders afterwards; an "open in
   Reference Viewer / Size Map" row action so a user can verify *why* something
   looks unused before deleting; a warning when the list is enormous.
4. **"Final Packaging" size mode** — `IAssetManagerEditorModule::GetAvailableRegistrySources()`
   / `SetCurrentRegistrySource()` switch the size columns between the editor's
   registry and a cooked platform registry. That selector is all HXS's "Final
   Packaging" dropdown is. Only useful to someone who has actually cooked.
5. **PSO precaching audit** — a few more checks in `FProjectSettingsPass`. Note
   UE 5.2+ precaches automatically, so building a bundled `.spc` cache (HXS's
   "Stutter Killer") would be chasing a solved problem; auditing the settings is
   the part still worth doing.
6. **More analyze passes** — shader instruction counts (version/platform-aware).
7. **Ship prep** — `Resources/Icon128.png` is missing (plugin browser + FAB), and
   `.uplugin` has empty `CreatedBy` / `DocsURL` / `SupportURL`.

## Working agreements

- **Commits are authored by the repo owner**, with no AI co-author trailer. Work
  happens directly on `main`; don't spin up branches unless asked.
- **The engine is on disk** at `E:\EpicGames\UE_5.7` (5.8 is installed too).
  Grep its headers to verify an API *before* writing against it — this has caught
  a never-firing check, two wrong classes and a private member so far. It is much
  cheaper than a build round-trip.

## Reference material (ideas only — do not copy code)

Competitor plugins sit on disk for study. The owner holds a commercial licence
for Assets Cleaner, so reading is fine; **shipping any of their code inside a
plugin we sell on the same store is not**, licence or no licence. Learn the
approach and the APIs, write our own.

- `E:\Projects\Pal\` — Palatial's six plugins (moved out of `Plugins/`, they
  don't compile on 5.7). Their `pRegistry` independently arrives at the same
  registry-of-operations design we use, which is reassuring. Their task interface
  does `AnalyzeTask` → preview → `ExecuteTask`, which is where the "preview before
  apply" roadmap item comes from. Their mesh ops depend on Windows-only
  third-party binaries — a weakness worth naming in our listing, since we use the
  engine's own subsystems.
- `Plugins/AssetsCleaner` — a shipped, paid unused-asset tool (builds fine on
  5.7). Its settings confess that levels *"[cannot be] reliably check[ed] for
  references"*, which corroborates hard-excluding Worlds. It's also where the
  `EDependencyCategory::All` and project-settings-reference lessons came from.

## Build / run

1. Plugin lives at `<Project>/Plugins/OptimizationToolset`.
2. Build the editor target (or let the editor compile on launch / Live Coding).
3. Open **Optimization Toolset** from the toolbar or Window menu.
4. Optional: adjust thresholds in **Project Settings → Plugins → Optimization
   Toolset**.
5. Smoke test: **Scan Level** → check Analyze findings → **Optimize** → **Apply**
   on a finding → confirm the change (Nanite, LOD1–3, simple collision, light
   mobility, texture compression, or a group of actors collapsing into one HISM
   actor) and that **Ctrl+Z** reverts it.
6. Cleanup → **Measure** for the project size bar; the actions there are *not*
   Undo-able, so try them on a scratch project first.

## Gotchas already hit (don't rediscover these)

**Slate colour, two separate traps.** Both cost a build each.
- *Colours are linear.* Author the palette as sRGB hex via `FLinearColor(FColor(0x..))`
  or the whole UI washes out to grey.
- *Tints multiply.* `BorderBackgroundColor` and `ColorAndOpacity` multiply the
  brush's own tint, they don't replace it. Anything coloured at runtime must sit
  on a **white** brush — hence `Toolset.Fill` / `Fill.Rounded` / `Fill.Pill`, and
  hence the SVG icons being recoloured to `#FFFFFF`. A dark brush times a bright
  colour is just a dark colour; a 6%-alpha brush times anything is invisible.

**Count Slate brackets, don't eyeball them.** Restructuring a widget tree —
lifting a card out of its `STableRow`, say — drops a `[` and leaves its `]`, and
the compiler then blames a line far from the edit. Reading the nesting to check
it failed twice in a row here. This finds it in seconds:

```sh
awk '{ n=gsub(/\[/,"["); m=gsub(/\]/,"]"); o+=n; c+=m }
     END { printf "open=%d close=%d balance=%+d\n", o, c, o-c }' SToolsetWindow.cpp
```
Lambda captures (`[this]`) balance themselves, so they don't skew it. Per-function
balance plus a line-by-line running depth pinpoints the exact stray bracket.

**Verifying engine APIs: grep the class, not the file.** Two wrong assumptions
came from grepping a header and assuming the match belonged to the class above it:
- `bSupportAllShaderPermutations` is on `URendererOverrideSettings`, not
  `URendererSettings` — same header, different object.
- `GameDefaultMap` / `ServerDefaultMap` / `GlobalDefaultGameMode` are **private**
  on `UGameMapsSettings`; use the static getters.
- Also check the *property* name, not the console variable: the GI method member
  is `DynamicGlobalIllumination`, while the cvar is `r.DynamicGlobalIlluminationMethod`.

**The engine may already do what you're about to check for.** `UTexture` forces
`SRGB = false` whenever compression is Normalmap/Masks/Alpha/HDR
([Texture.cpp](E:/EpicGames/UE_5.7/Engine/Source/Runtime/Engine/Private/Texture.cpp)),
so a "normal map with sRGB on" check would never fire. What the engine *can't*
know is a texture's role — hence asking the materials instead.

**Asset references are wider than `Package`.** `GetReferencers`/`GetDependencies`
default to `EDependencyCategory::Package`. Use `::All` when deciding whether
something is unused: `Manage` references are how the Asset Manager links primary
assets to what its rules pull in, and they never appear as package references.
And some assets are referenced by *no* asset at all — the default game mode,
game instance and startup maps live in project settings as paths.

**dllexport + move-only members.** Don't put `MODULE_API` on a class holding
`TArray<TUniquePtr<...>>` — MSVC force-generates the copy ctor and it fails to
compile. `FToolsetRegistry` is intentionally *not* exported.

**Include paths that bit us:** `FSlateVectorImageBrush`/`FSlateImageBrush` are in
`Brushes/SlateImageBrush.h`; `IPlugin` is in `Interfaces/IPluginManager.h` (no
separate `IPlugin.h`); `AssetManagerEditorModule.h` is in a *plugin*
(`Engine/Plugins/Editor/AssetManagerEditor`), not `Engine/Source`.

**Risky version-sensitive APIs:** `UStaticMesh::NaniteSettings.bEnabled`
(direct member; fall back to `SetNaniteSettings()` if a version rejects it) and
`UStaticMeshEditorSubsystem::SetLodsWithNotification` / `FStaticMeshReductionOptions`
(module `StaticMeshEditor`; name may differ across versions).
