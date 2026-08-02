# Optimization Toolset — handoff

Read this file together with [ARCHITECTURE.md](ARCHITECTURE.md) before changing
the plugin.

## Product

Optimization Toolset is an editor-only Unreal Engine plugin targeting UE
5.3–5.7. It combines severity-based level analysis, reviewed auto-fixes,
profiling shortcuts, memory reporting and project cleanup in one dockable Slate
window.

Development repository:
`E:\Projects\PluginTest\Plugins\OptimizationToolset`.

## Current implementation

The active root widget is `SOptimizeShell`; the previous `SToolsetWindow` and
`Private/Toolset/Panels` implementation has been removed. The current pages are
under `Private/Toolset/Slate`.

- Dashboard shows level statistics, findings totals, project size, thresholds
  and loaded-level scope.
- Optimize groups findings by problem type and level, provides scope-aware
  navigation, per-row fixes and selected batch fixes.
- Analyzer reports loaded texture, render-target and static-mesh memory. Texture
  streaming metrics are guarded when the streaming manager is unavailable.
- Profile controls editor viewport stat stacks, view modes and Nanite/Lumen/VSM
  visualizers.
- Clean Up reports possible duplicate and unreferenced assets and exposes Save
  Dirty Packages and Fix Up Redirectors actions.

The single Scan button runs the shared level analysis followed by each
project-wide report. Settings and level-scope changes invalidate stale analysis.

## Implemented analysis passes

- Static meshes: triangle budgets, Nanite candidates, low-poly Nanite review,
  Nanite/material incompatibility, missing LODs, collision and empty actors.
- Textures: density/size, power-of-two, mipmaps and compression-role checks.
- Materials: slots, assignments, translucency, two-sided use, samplers and
  representative shader instructions.
- Lighting: movable-light budget per loaded level and lightmap resolution.
- Repeated compatible actors as HISM/ISM candidates.
- Project rendering settings, including texture-streaming-disabled checks when
  the project texture footprint makes the warning relevant.
- Blueprint tick and hard dependency-chain size.

## Implemented fixes

- enable or disable Nanite;
- generate LODs and simple collision;
- review light mobility;
- delete an empty static-mesh actor;
- convert a selected compatible actor group to instances;
- correct normal-map compression and data-texture sRGB;
- enable texture streaming.

Fixes validate their targets and use editor transactions where supported. A
batch rescan happens once after the selected fixes.

## Cleanup safety

Unreferenced is not synonymous with unused: runtime paths and name-based loads
may not exist in the Asset Registry. Maps, redirectors, World Partition external
packages and project-settings roots are excluded, but users must still review
the Unreal delete dialog.

Possible duplicates are based on name, class and disk size across folders. The
word “possible” is deliberate; no content hash is currently calculated.

## Follow-up work

Recommended architecture work after the current UI is stabilized:

1. Introduce a phased scan coordinator so one click does not open several
   sequential progress dialogs or repeat registry work.
2. Extract Dashboard settings, Cleanup scanning and Optimize affected-assets
   presentation into focused components.
3. Add cooked-registry selection for final packaging size reports.
4. Add CSV/JSON snapshots only if a Reports workflow is brought back to the UI.

## Ship checklist

- Build and smoke-test each supported engine version separately.
- Scan a persistent map with loaded and excluded sub-levels.
- Exercise every navigation destination and transactional fix, then Undo.
- Test with texture streaming both enabled and disabled.
- Start a cleanup scan while the Asset Registry is busy and cancel a scan; no
  partial result should be presented.
- Review plugin metadata (`CreatedBy`, documentation and support URLs) before
  FAB submission.
- Add a marketplace/plugin-browser icon if the final distribution requires one.

## Working rules

- Verify version-sensitive APIs against the installed engine source before
  coding against them.
- Keep feature logic independent of Slate and register concrete passes/fixes in
  `FToolsetRegistry`.
- Do not add AI co-author trailers to commits.
- Do not copy competitor plugin source; use it only to understand workflows and
  public engine APIs.
