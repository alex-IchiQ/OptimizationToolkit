# Architecture & Conventions

How the plugin is put together, the naming rules, the design system, and —
most importantly — **how to add a new analyze pass or fix**.

## Module

Single editor module `OptimizationToolsetEditor` (`Type: Editor`, `LoadingPhase:
PostEngineInit`, Win64/Mac/Linux). Build deps of note in
`OptimizationToolsetEditor.Build.cs`: `UnrealEd`, `ToolMenus`, `LevelEditor`,
`Projects` (IPluginManager), `DeveloperSettings`, `StaticMeshEditor` (LOD
subsystem), Slate/SlateCore.

`FOptimizationToolsetEditorModule::StartupModule()` does three things:
`FToolsetStyle::Initialize()`, `FToolsetRegistry::Get().RegisterDefaults()`, and
registers the dockable tab + toolbar/menu entries.

## File layout

```
Public/Toolset/
  ToolsetCompat.h      OPTIMIZATION_* engine-version macros / feature flags
  OptimizationToolsetSettings.h  UDeveloperSettings-backed analyze thresholds
  ToolsetTypes.h       ESeverity, ECategory, FFinding, FScanResult, FAnalyzeThresholds
  ToolsetRegistry.h    FToolsetRegistry (+ includes the two interfaces)
  ToolsetStyle.h       FToolsetStyle (palette + brushes + text styles)
  SToolsetWindow.h     SToolsetWindow, EToolsetSection
  Analyzer/
    IAnalyzePass.h
    LevelScanContext.h FLevelScanContext (the one world walk, shared by passes)
    LevelAnalyzer.h    FLevelAnalyzer (drives passes)
  Optimization/
    IOptimizationFix.h
  Cleanup/
    ICleanupAction.h
Private/Toolset/
  ToolsetRegistry.cpp  Get() + RegisterDefaults() (the one place features are listed)
  ToolsetStyle.cpp
  SToolsetWindow.cpp
  Analyzer/
    LevelAnalyzer.cpp
    Passes/            one file per pass: StaticMeshPass, TexturePass,
                       MaterialPass, LightingPass, InstancingCandidatePass
  Optimization/
    Fixes/             one file per fix: EnableNaniteFix, GenerateLODsFix,
                       SimpleCollisionFix, ReviewLightMobilityFix
                       + FixUtils.h (shared MeshFromFinding helper)
  Cleanup/
    Actions/           one file per action: SaveDirtyPackagesAction,
                       FixUpRedirectorsAction
Resources/
  *.svg (white, one per nav section), vera.png (mascot)
```

Include paths are relative to the `Public`/`Private` roots, e.g.
`#include "Toolset/Analyzer/IAnalyzePass.h"`.

## The registry pattern

Features are pluggable classes behind two interfaces; a singleton registry owns
them. The window and analyzer iterate the registry — **features never reference
each other or the UI.**

```
IAnalyzePass      Run(context, thresholds, out)          → appends FFindings (read-only)
IOptimizationFix  GetId / GetLabel / IsSupported / Apply → resolves findings, transactional
ICleanupAction    GetId / GetTitle / IsDestructive / Execute → project-wide, NOT undoable
FToolsetRegistry  GetPasses() / GetFixes() / GetActions() / FindFix(FixId) / RegisterDefaults()
```

- `FLevelAnalyzer::AnalyzeCurrentLevel()` walks the world **once** into an
  `FLevelScanContext`, runs every registered pass against it, sorts, and returns
  `FScanResult`. Passes never iterate the world themselves — if a new pass needs a
  type that isn't cached, add a bucket to `FLevelScanContext`.
- A finding's `FixId` (an `FName`) links it to the fix that resolves it. The
  Optimize panel shows a finding only if `FToolsetRegistry::FindFix(FixId)` returns
  a fix whose `IsSupported()` is true.
- A finding's `TypeId` (an `FName`, e.g. `Mesh.MissingLODs`) identifies the *kind*
  of problem. `Title` is localizable display text and can't serve as an id.
  `FScanResult::HealthScore()` groups penalties by `TypeId` and caps each type's
  contribution, so one problem repeated 200 times can't pin the score to zero.
  **Every finding must set it** — it's the first constructor argument.

### Version gating philosophy (important)

Because **each FAB build compiles for exactly one engine version**, do **not**
write separate per-version strategy classes — only one ever compiles. Instead:
- **Compile-time** API differences → `#if OPTIMIZATION_HAS_X` *inside* the feature
  (macros in `ToolsetCompat.h`). A whole feature can be `#if`'d out around both its
  implementation and its registration.
- **Runtime** availability (RHI, GPU caps, project state) → `IOptimizationFix::IsSupported()`.

## How to add an analyze pass

1. Add `Private/Toolset/Analyzer/Passes/FoliagePass.h` — one file per pass:
   ```cpp
   class FFoliagePass : public IAnalyzePass
   {
   public:
       virtual FName GetId() const override { return TEXT("Pass_Foliage"); }
       virtual void Run(const FLevelScanContext& Context, const FAnalyzeThresholds& T, FScanResult& Out) const override;
   };
   ```
2. Implement `Run` in the matching `.cpp` — iterate `Context.Actors` (or a
   pre-bucketed array like `Context.StaticMeshActors`), build `FFinding`s and
   `Out.Findings.Add(...)`. Each finding takes a stable `TypeId` first, then
   Severity, Category, Title, Subject; then set WhyItMatters, HowToFix,
   TargetActor, and a `FixId` if a fix exists. Keep pass-local helpers in that
   file's anonymous namespace. It must be **read-only**.
3. Register it: add `AddPass(MakeUnique<FFoliagePass>());` in
   `FToolsetRegistry::RegisterDefaults()` (`ToolsetRegistry.cpp`).

If the pass reports a property of an *asset* rather than of a placed actor,
de-dupe by that asset — otherwise a mesh used 200 times yields 200 findings.

That's it — the window picks it up; no UI changes.

## How to add a fix

1. Add `Private/Toolset/Optimization/Fixes/SimpleCollisionFix.h` — one file per fix:
   ```cpp
   class FSimpleCollisionFix : public IOptimizationFix
   {
   public:
       virtual FName GetId() const override { return TEXT("Fix_SimpleCollision"); } // must match a finding's FixId
       virtual FText GetLabel() const override;      // button text, e.g. "Simplify collision"
       virtual bool IsSupported() const override;
       virtual bool Apply(const FFinding& Finding) const override;
   };
   ```
2. Implement in the matching `.cpp`. **Apply must own a transaction:**
   ```cpp
   const FScopedTransaction Transaction(LOCTEXT("...Tx", "..."));
   Asset->Modify();
   /* mutate */
   Asset->PostEditChange();
   Asset->MarkPackageDirty();
   return true; // true if something changed
   ```
   Use the `MeshFromFinding()` helper (`Fixes/FixUtils.h`) to get the static mesh
   behind a finding.
3. Register it: `AddFix(MakeUnique<FSimpleCollisionFix>());` in `RegisterDefaults()`.

The Optimize panel then shows any finding whose `FixId` matches, with an Apply
button labelled by `GetLabel()`. After Apply the window re-scans automatically.

## How to add a cleanup action

Same shape, but an action isn't tied to a finding — it operates on the whole
project, and **nothing here is Undo-able** (no transaction can take back a
resaved or deleted asset).

1. Add `Private/Toolset/Cleanup/Actions/DeleteUnusedAction.h` implementing
   `ICleanupAction`: `GetId`, `GetTitle`, `GetDescription`, `GetButtonLabel`,
   and `Execute()` returning a human-readable summary ("Fixed up 12 redirectors").
2. Override `IsDestructive()` to return true if it rewrites or removes assets —
   the panel then shows a "NOT UNDOABLE" tag and confirms before running.
3. Register it: `AddAction(MakeUnique<FDeleteUnusedAction>());` in `RegisterDefaults()`.

`Execute()` should stay honest about partial results: report what actually
happened rather than what was attempted (see `FSaveDirtyPackagesAction`, which
re-queries dirty packages afterwards instead of trusting the return value).

## Naming conventions (enforced)

- **No `Opt` abbreviation.** Where a distinctive prefix is genuinely required —
  the global preprocessor macros — spell it out: `OPTIMIZATION_*`.
- Types: `FFinding`, `FScanResult`, `ESeverity`, `ECategory`, `FAnalyzeThresholds`,
  `FLevelScanContext`, `FLevelAnalyzer`, `FToolsetStyle`, `SToolsetWindow`,
  `EToolsetSection`, `FToolsetRegistry`. Feature classes: `F<Thing>Pass`, `F<Thing>Fix`,
  one per file, named after the class.
- Finding `TypeId`s read `Domain.Problem`: `Mesh.MissingLODs`, `Texture.Oversized`,
  `Lighting.MovableLightOverBudget`. Fix ids read `Fix_Thing`; pass ids `Pass_Thing`.
- `ESeverity`/`ECategory` are **plain enums, not UENUM** (no reflection needed
  yet). If a settings `UObject` later needs them as UPROPERTY, reintroduce `UENUM`
  and qualify to avoid a global-name clash: `EOptimizationSeverity` etc.
- Slate style keys are prefixed `Toolset.*`.

## Design system (Rider "Islands" + mascot palette)

- **Islands layout**: flat dark backdrop with rounded, slightly-lighter panels
  ("islands", brush `Toolset.Island`, radius 10, solid 1px edge) for sidebar /
  header / content, separated by 7px gaps. Cards (`Toolset.Card`) elevate one more
  step. Solid (not translucent) outlines — a translucent edge leaves a faint seam
  on the corner arc over the dark backdrop.
- **Palette** = the mascot's scheme (`ToolsetStyle.cpp`): accent **teal `#17B9A6`**
  (hair tips / jacket piping), surfaces echo her **charcoal jacket** (`#23262A`),
  and the **amber** severity colour matches her eyes. Text on the teal accent is
  dark `#161719`.
- **Colours are linear**: author every colour as `FLinearColor(FColor(0x..))`
  (sRGB→linear) — raw sRGB fractions wash the UI out to grey.
- **Nav**: wide 216px sidebar, icon + label rows. Icons are white SVGs
  (`Toolset.Icon.*`) tinted per state via `SImage.ColorAndOpacity` (teal when
  selected, grey otherwise). The **mascot** (`vera.png`, brush `Toolset.Mascot`)
  sits at the bottom in an `SScaleBox` (ScaleToFit, DownOnly) so it fills the space
  without overlapping the nav.

## Where things are wired

- Tab / toolbar / menu: `OptimizationToolsetEditorModule.cpp`.
- Section switching: `SToolsetWindow` uses an `SWidgetSwitcher`; `EToolsetSection`
  indexes it.
- Scan flow: `SToolsetWindow::RunScan()` calls `FLevelAnalyzer::AnalyzeCurrentLevel()`,
  rebuilds `AllFindings` + `FixableFindings`, refreshes both list views.
- Apply flow: `SToolsetWindow::ApplyFix()` / `OnApplyAllFixes()` call
  `IOptimizationFix::Apply` then `RunScan()` to refresh.
