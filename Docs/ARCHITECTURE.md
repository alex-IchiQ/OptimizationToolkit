# Architecture & Conventions

How the plugin is put together, the naming rules, the design system, and —
most importantly — **how to add a new analyze pass or fix**.

## Module

Single editor module `OptimizationToolsetEditor` (`Type: Editor`, `LoadingPhase:
PostEngineInit`, Win64/Mac/Linux). Build deps of note in
`OptimizationToolsetEditor.Build.cs`: `UnrealEd`, `ToolMenus`, `LevelEditor`,
`Projects` (IPluginManager), `DeveloperSettings` (settings object),
`StaticMeshEditor` (LOD + collision subsystem), `AssetRegistry`, `AssetTools`
(redirector fixup, asset delete), `EngineSettings` (UGameMapsSettings),
`AssetManagerEditor` (asset disk sizes), Slate/SlateCore.

The `.uplugin` declares two plugin dependencies: `EditorScriptingUtilities` and
`AssetManagerEditor` (the latter is `EnabledByDefault` in the engine, but a
module dependency on it must not rely on that).

`FOptimizationToolsetEditorModule::StartupModule()` does three things:
`FToolsetStyle::Initialize()`, `FToolsetRegistry::Get().RegisterDefaults()`, and
registers the dockable tab + toolbar/menu entries.

## File layout

```
Public/Toolset/
  ToolsetCompat.h      OPTIMIZATION_* engine-version macros / feature flags
  OptimizationToolsetSettings.h  UDeveloperSettings-backed analyze thresholds
  ToolsetTypes.h       ESeverity, ECategory, EFindingScope, FFinding, FLevelStats, FScanResult, FAnalyzeThresholds
  ToolsetRegistry.h    FToolsetRegistry (+ includes the three interfaces)
  ToolsetModel.h       FToolsetModel (scan state + filters + fixes; the panels' one shared object)
  ToolsetStyle.h       FToolsetStyle (palette + brushes + text styles)
  SToolsetWindow.h     SToolsetWindow, EToolsetSection (chrome only)
  Analyzer/
    IAnalyzePass.h
    LevelScanContext.h FLevelScanContext (the one world walk, shared by passes)
    LevelAnalyzer.h    FLevelAnalyzer (drives passes)
  Optimization/
    IOptimizationFix.h
  Cleanup/
    ICleanupAction.h
    ProjectSizeReport.h  EAssetCategory + FProjectSizeReport (read-only, not an action)
Private/Toolset/
  ToolsetRegistry.cpp  Get() + RegisterDefaults() (the one place features are listed)
  ToolsetModel.cpp
  ToolsetStyle.cpp
  ToolsetWidgetUtils.h S() / Brush() shorthand, header-only, used by every panel
  SToolsetWindow.cpp
  Navigation/
    FindingNavigator   Routes finding actions to Content Browser, viewport,
                       Project Settings, or a rescan
  Panels/              one file per panel, like Passes/ and Fixes/:
                       SFindingTree (findings grouped by problem TypeId),
                       SFindingCard (one finding's presentation and actions),
                       SCategorySettingsPanel (category threshold details),
                       SDashboardPanel, SOptimizePanel,
                       SCleanupPanel, SProfilePanel, SPlaceholderPanel
  Analyzer/
    LevelAnalyzer.cpp
    Passes/            one file per pass: StaticMeshPass, TexturePass,
                       MaterialPass, LightingPass, InstancingCandidatePass,
                       ProjectSettingsPass, BlueprintTickPass,
                       TextureCompressionPass, BlueprintDependencyPass
  Optimization/
    Fixes/             one file per fix: EnableNaniteFix, DisableNaniteFix,
                       GenerateLODsFix,
                       SimpleCollisionFix, ReviewLightMobilityFix,
                       DeleteEmptyMeshActorFix,
                       ConvertToInstancesFix, TextureSettingsFixes (two fixes:
                       normal map compression + data texture sRGB)
                       + FixUtils.h (shared MeshFromFinding helper)
  Cleanup/
    ProjectSizeReport.cpp
    Actions/           one file per action: SaveDirtyPackagesAction,
                       FixUpRedirectorsAction, DeleteUnusedAssetsAction
Resources/
  *.svg (white, one per nav section), vera.png (mascot)
```

Include paths are relative to the `Public`/`Private` roots, e.g.
`#include "Toolset/Analyzer/IAnalyzePass.h"`.

## Two patterns, one idea

The registry keeps **features** from knowing about each other or the UI. The
model keeps **panels** from knowing about each other. Both exist so that adding
something means writing one file and registering it, never editing a hub.

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

- `FLevelAnalyzer::AnalyzeCurrentLevel(ExcludedLevelPackages)` walks the world
  **once**, skips actors belonging to unchecked Dashboard levels, and builds an
  `FLevelScanContext`, gathers `FScanResult::Stats` (the level-scale numbers the
  Dashboard shows — not findings, nobody judges them), runs every registered pass
  against it, sorts, and returns `FScanResult`. Passes never iterate the world themselves — if a new pass needs a
  type that isn't cached, add a bucket to `FLevelScanContext`.
- A finding's `FixId` (an `FName`) links it to the fix that resolves it. Optimize
  shows every finding; a second **Apply** button appears only when
  `FToolsetRegistry::FindFix(FixId)` returns a supported fix.
- A finding's `TypeId` (an `FName`, e.g. `Mesh.MissingLODs`) identifies the *kind*
  of problem. `Title` is localizable display text and can't serve as an id.
  `FScanResult::HealthScore()` groups penalties by `TypeId` and caps each type's
  contribution, so one problem repeated 200 times can't pin the score to zero.
  **Every finding must set it** — it's the first constructor argument.
- `EFindingScope` states who owns the problem: `Asset`, `Actor`, `Level`,
  `Project`, or `System`. It is a navigation/fix contract, not decoration:
  asset findings require `TargetAsset`, actor findings require `TargetActor`,
  and level findings must point at an addressable actor/group. The analyzer
  validates this contract after every pass.

## The model (`FToolsetModel`)

One plain object, created by `SToolsetWindow::Construct` and handed to each panel
as a `TSharedPtr`. It owns the scan result, per-level scope overrides, the derived finding lists, the
filters, and the fix operations.

```
RunScan() / ApplyFix() / InvalidateScan()    mutate, then broadcast
GetLastScan() / GetPreviousStats()           the numbers
GetVisibleFindings()                         filtered query, built on demand
CountForCategory(Category)                   the nav badges
SetSearchFilter() / ToggleSeverity() / SetCategoryFilter()
OnChanged()                                  FSimpleMulticastDelegate: "redraw"
```

- **Why it exists:** the scan result and filters used to be fields on
  `SToolsetWindow`, so every panel needing them had to be built by that window.
  That is how one widget became six panels and 1800 lines.
- **Views react to `OnChanged()`, they are not poked.** A fix applied in Optimize
  moves the Dashboard's numbers, the nav's badges and the findings tree; none of
  those should be listed at the call site.
- **Most panels don't subscribe at all.** Text bound with `Text_Lambda` re-reads
  the model every tick, so the Dashboard and the header are automatically live.
  Only `SOptimizePanel` subscribes for its `SFindingTree`, because an `STreeView` has to be told to
  rebuild. If you add a panel, prefer a bound lambda; subscribe only if you cache.
- **Panels that subscribe must unsubscribe in their destructor** — the model
  outlives them.
- Filters live in the model, not in the Optimize widget, because the nav badges
  and the tree must always describe the same visible result set.

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
   Severity, Category, Scope, Title, Subject; then set WhyItMatters, HowToFix,
   TargetActor/TargetAsset as required by that scope, and a `FixId` if a fix exists. Keep pass-local helpers in that
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

## How to add a panel

Unlike a pass or a fix, a panel is *not* registry-driven — sections are a fixed
enum, because each one is a designed screen rather than a repeatable unit.

1. Add `Private/Toolset/Panels/SReportsPanel.h/.cpp`, one class per file, an
   `SCompoundWidget` taking `SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)`.
   `#include "Toolset/ToolsetWidgetUtils.h"` and `using namespace ToolsetUI;` for
   the `S()` / `Brush()` shorthand.
2. Read the model through **bound lambdas** (`Text_Lambda`, `Visibility_Lambda`)
   and it stays live for free. Only subscribe to `OnChanged()` if you cache
   something a lambda can't rebuild — and then unsubscribe in your destructor.
3. Add the value to `EToolsetSection` and an `AddSlot()` in
   `SToolsetWindow::BuildContent()` **in the same order** — the switcher is
   indexed by the enum.
4. Add a `BuildNavItem(...)` row in `BuildSidebar()`, and an SVG in `Resources/`
   registered as `Toolset.Icon.*` in `ToolsetStyle.cpp`. **Author it white** —
   see the tint gotcha in HANDOFF.

If the panel lists findings, use `SFindingTree` and supply only the leaf card;
don't hand-roll a second tree.

## Naming conventions (enforced)

- **No `Opt` abbreviation.** Where a distinctive prefix is genuinely required —
  the global preprocessor macros — spell it out: `OPTIMIZATION_*`.
- Types: `FFinding`, `FScanResult`, `ESeverity`, `ECategory`, `EFindingScope`, `FAnalyzeThresholds`,
  `FLevelScanContext`, `FLevelAnalyzer`, `FToolsetStyle`, `SToolsetWindow`,
  `EToolsetSection`, `FToolsetRegistry`. Feature classes: `F<Thing>Pass`, `F<Thing>Fix`,
  one per file, named after the class.
- Finding `TypeId`s read `Domain.Problem`: `Mesh.MissingLODs`, `Texture.Oversized`,
  `Lighting.MovableLightOverBudget`. Fix ids read `Fix_Thing`; pass ids `Pass_Thing`.
- `ESeverity`/`ECategory`/`EFindingScope` are **plain enums, not UENUM** (no reflection needed
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
- **Nav**: wide 216px sidebar. **Scan** is a chunky 84px-tall accent block (icon
  above label) directly under the brand, not in the header: it is the one action
  every section depends on, so it sits where the eye starts rather than at the far
  end of a header whose title changes per section. Its style
  (`Toolset.Button.Scan`) exists only because `SButton` *adds* the style's
  `NormalPadding` to `ContentPadding` — a padding-free style lets the `SBox` decide
  the size. Below it, icon + label rows. Optimize lists every `ECategory` as a
  sub-item with a count, including empty categories so thresholds can be edited
  before scanning. Picking one narrows the panel (`FToolsetModel::SetCategoryFilter`), and the section header itself means
  "everything in here", so clicking it clears the narrowing. Clicking the section
  you're already in folds its list away — that keeps the arrow an indicator rather
  than a second hit target nested inside a button, which Slate handles badly.
  Icons are white SVGs
  (`Toolset.Icon.*`) tinted per state via `SImage.ColorAndOpacity` (teal when
  selected, grey otherwise). The **mascot** (`vera.png`, brush `Toolset.Mascot`)
  sits at the bottom in an `SScaleBox` (ScaleToFit, DownOnly) so it fills the space
  without overlapping the nav.
- **No stock editor controls in the content area.** The engine's checkbox,
  spin box and `IDetailsView` each arrive in their own visual language and can't be
  themed to match, so the toolset renders its own:
  - `SToolsetToggle` (`Panels/SToolsetToggle.*`) — a pill switch: a rounded track
    (accent on / grey off) with a knob slid between two `SHorizontalBox` spacers
    whose fill weights swap with state. State is a bound `IsChecked` attribute, so
    it fronts the model without holding any of its own — used for the level-scope
    rows and every bool setting.
  - `Toolset.SpinBox` — an `FSpinBoxStyle` on the card-inner surface with a teal
    drag fill and the up/down chevrons removed. Used by the category settings.
  - `SCategorySettingsPanel` builds its rows from **reflection**: it iterates the
    selected category's `UPROPERTY`s (`Category` metadata → finding category),
    renders each `FIntProperty` as a `Toolset.SpinBox` honouring its
    Clamp/UI min-max, each `FBoolProperty` as an `SToolsetToggle`, and writes back
    through the property onto the settings CDO. No `PropertyEditor` dependency.

## Where things are wired

- Tab / toolbar / menu: `OptimizationToolsetEditorModule.cpp`.
- Section switching: `SToolsetWindow` uses an `SWidgetSwitcher`; `EToolsetSection`
  indexes it, so **the AddSlot order in `BuildContent()` must match the enum**.
- Scan flow: `FToolsetModel::RunScan()` resolves the Dashboard level toggles to
  excluded package names, copies the outgoing `LastScan.Stats` into
  `PreviousStats` (the Dashboard's delta baseline — the rescan after a fix is the
  case that matters), calls `FLevelAnalyzer::AnalyzeCurrentLevel(Excluded)`, rebuilds
  `AllFindings`, then broadcasts `OnChanged()`.
- Apply flow: `FToolsetModel::ApplyFix()` calls
  `IOptimizationFix::Apply` then `RunScan()`, so the refresh is the scan's
  broadcast — no panel refreshes another.
- Filtering: the model applies filters; `SFindingTree::SetFindings()` regroups
  whatever it is handed. `SOptimizePanel` pushes `GetVisibleFindings()` into its
  tree on every `OnChanged()`.
- Grouping: `SFindingTree::BuildProblemTree()` groups the flat list by stable
  `FFinding::TypeId`. It re-expands every group afterwards, because the
  nodes are new objects each time and can't remember their own expansion.
- Finding presentation stays in `SFindingCard`; `SOptimizePanel` only composes
  filters, category settings, and the tree. `FFindingNavigator` owns every editor
  integration behind a card's primary action, keeping Content Browser and
  Project Settings API details out of Slate presentation code.
