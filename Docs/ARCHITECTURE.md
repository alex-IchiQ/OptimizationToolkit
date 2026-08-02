# Architecture and conventions

## Module lifecycle

The plugin contains one editor module, `OptimizationToolsetEditor`, loaded at
`PostEngineInit`. `FOptimizationToolsetEditorModule` initializes both Slate
style sets, registers the feature registry, and installs the dock tab, Window
menu entry and toolbar button. The tab content is `SOptimizeShell`.

`AssetManagerEditor` is the only declared plugin dependency. It is required for
disk-size columns used by project cleanup and Blueprint dependency analysis.

## UI composition

All active UI lives under `Private/Toolset/Slate/`:

```text
SOptimizeShell
├── SDashboardView
├── SOptimizeView
├── SAnalyzerView
├── SProfileView
└── SCleanupView
```

`SOptimizeShell` owns one `FToolsetModel` and passes it to Dashboard and
Optimize. The shell's single Scan button runs level analysis first, then the
project-size, memory and cleanup sweeps owned by their views.

`FOptimizeStyle` is the plugin's single style set. It owns surfaces, text and
control styles, semantic severity/category colors, navigation icons and mascot.

## Registry pattern

`FToolsetRegistry` owns three kinds of extension:

```text
IAnalyzePass      Run(Context, Thresholds, OutFindings)
IOptimizationFix  GetId / GetLabel / IsSupported / Apply
ICleanupAction    GetId / metadata / Execute
```

`RegisterDefaults()` is the only place concrete passes, fixes and project-wide
cleanup actions are listed. Features do not reference Slate widgets.

### Adding an analysis pass

1. Implement `IAnalyzePass` under `Private/Toolset/Analyzer/Passes/`.
2. Use the buckets in `FLevelScanContext`; do not perform another world walk.
3. Add a stable `TypeId`, category, severity, scope, reason and recommendation
   to every `FFinding`.
4. Set `TargetAsset`, `TargetActor`, `RelatedActors` and `FixId` according to
   the finding's navigation/fix contract.
5. Register the pass in `FToolsetRegistry::RegisterDefaults()`.

If a pass needs a type not present in the context, extend
`FLevelScanContext` and populate it in the central analyzer.

### Adding a fix

1. Implement `IOptimizationFix` under
   `Private/Toolset/Optimization/Fixes/`.
2. Give it a stable ID and assign that ID to compatible findings.
3. Validate targets again in `Apply`; scan results can become stale.
4. Wrap editor mutations in `FScopedTransaction` where Undo/Redo is supported,
   call `Modify()`, mark packages dirty and notify the relevant editor systems.
5. Register the fix in `FToolsetRegistry::RegisterDefaults()`.

The model applies selected fixes and performs one scan after the batch. Group
findings are copied and narrowed to selected actors before the fix is called.

### Adding a cleanup action

Use `ICleanupAction` only for independent project-wide commands displayed in
the Clean Up action bar. The page itself owns its reviewed possible-duplicate
and unreferenced-asset list, so do not add a second delete-unused action.

## Shared model

`FToolsetModel` owns:

- the latest and previous level statistics;
- shared findings;
- search, category and severity filters;
- session-only per-level inclusion overrides;
- fix execution and post-fix rescanning.

Views subscribe to `OnChanged()` instead of being updated directly by the code
that caused a change. Any setting or scope mutation that changes scan input must
call `InvalidateScan()` before another fix can be applied.

## Analysis contracts

`FLevelAnalyzer` performs one walk across the persistent level and all loaded
sub-levels. Excluded level packages are skipped. Unloaded levels are never
implicitly loaded by a scan.

`EFindingScope` determines navigation:

- `Asset` opens the content asset.
- `Actor` focuses the level actor.
- `Project` opens the relevant Project Settings page and filter.
- `Level` and `System` must provide an addressable destination when navigation
  is offered.

Project settings and Blueprint class-level findings deliberately avoid unsafe
auto-fixes that would change shared project or class defaults based on one level
scan.

## Clean Up contracts

The cleanup sweep waits for a complete Asset Registry. Cancellation discards
the partial result. Unreferenced checks use all dependency categories and
exclude maps, redirectors, World Partition external packages and packages named
by project settings.

Possible duplicates are an intentionally conservative review aid: same short
name, asset class and disk size in different folders. They are not guaranteed
byte-identical. Deletion always goes through `ObjectTools::DeleteAssets` with
Unreal's confirmation dialog.

## Compatibility

Version-sensitive engine calls belong behind macros in `ToolsetCompat.h`.
Verify APIs against the target engine source before adding a compatibility
branch. Avoid private engine headers and non-exported editor helpers.

Important module dependencies include `StaticMeshEditor` for LOD/collision
operations, `MaterialEditor` for shader statistics, `AssetRegistry` and
`AssetTools` for project scans, and `AssetManagerEditor` for disk sizes.

## Current technical debt

The following are useful follow-ups but are not release-blocking:

- split settings composition out of `SDashboardView`;
- extract the registry sweep from `SCleanupView` into a reusable scanner;
- extract the affected-assets table from `SOptimizeView`;
- replace sequential modal project sweeps with one phased/cancellable scan;
