# Optimization / Profiling Toolset

Editor-only Unreal Engine plugin that analyzes, optimizes and profiles the
current level from a single dockable Slate panel.

Positioning (vs. FAB competitors): combines **severity-based analysis**
(like *Perfector: Level*) **with safe, Undo-able auto-fixes** (like *HXS
Optimizer*) **and one-click profiling**, across a wide engine range and at a
mid price — without the "beta" label or 5.6-only / Windows-only limits.

## Requirements

- Unreal Engine **5.3 – 5.7** (single source tree; per-version features gated
  in `OptToolsetCompat.h`). Each engine version is shipped as its own build on
  FAB.
- Built-in plugin **Editor Scripting Utilities** (enabled automatically).

## Install (into a project)

1. Copy this folder to `YourProject/Plugins/OptimizationToolset`.
2. Regenerate project files and build the editor target, **or** let the editor
   compile it on first launch.
3. Open via the **Optimize** toolbar button, or **Window → Optimization Toolset**.

## Structure

```
OptimizationToolset.uplugin
Source/OptimizationToolsetEditor/
  OptimizationToolsetEditor.Build.cs
  Public/
    OptimizationToolsetEditorModule.h   # module entry (stays at root)
    Toolset/
      ToolsetCompat.h     # engine-version feature gates (OPTIMIZATION_* macros)
      ToolsetTypes.h      # ESeverity / ECategory / EFindingScope, FFinding, FScanResult
      LevelAnalyzer.h     # read-only level analysis (FLevelAnalyzer)
      ToolsetStyle.h      # palette, rounded-card brushes, text styles (FToolsetStyle)
      SToolsetWindow.h    # main dockable panel (SToolsetWindow)
  Private/
    OptimizationToolsetEditorModule.cpp
    Toolset/
      LevelAnalyzer.cpp
      ToolsetStyle.cpp
      SToolsetWindow.cpp
```

Naming: the `Opt` abbreviation is dropped throughout. Feature code lives under
`Toolset/` (included as `#include "Toolset/…"`). Where a distinctive prefix is
genuinely required — the global preprocessor macros — the full word
`OPTIMIZATION_` is used instead of the abbreviation.

## What works today (v1.0 scaffold)

- **Dockable Slate window**: left nav rail, header with live **health score**
  gauge + **Scan Level** action, switched content area.
- **Dashboard**: severity summary cards + workflow guide.
- **Optimize** *(functional unified workspace)*: category thresholds above a
  problem-type tree, search + severity filters, and Analyze-style finding cards.
  Scope-aware navigation uses **Show in Content**, **Focus Actor**, or **Open Settings**; a second
  **Apply** button appears when a safe transactional auto-fix exists. The card's
  performance rationale is available on hover.
- **Profile** *(functional)*: one-click `stat` command stacks (fps / unit / gpu
  / scenerendering / rhi / initviews / streaming / profilegpu / clear).
- **Cleanup** *(functional)*: project-size measurement and guarded maintenance
  actions. **Reports** remains a designed placeholder.

## Next up (roadmap)

- Optimize: expand the supported fix set and add optional batch previews.
- Cleanup: project size breakdown, fix redirectors, delete unused, settings audit.
- Reports: CSV/JSON export, before/after snapshots.
