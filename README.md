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
      ToolsetTypes.h      # ESeverity / ECategory enums, FFinding, FScanResult
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
- **Analyze** *(functional)*: real read-only passes — non-Nanite excessive
  triangles, Nanite candidates, missing LODs, per-poly collision on props, too
  many movable lights. Search + severity filters, **Focus** button frames the
  offending actor in the viewport.
- **Profile** *(functional)*: one-click `stat` command stacks (fps / unit / gpu
  / scenerendering / rhi / initviews / streaming / profilegpu / clear).
- **Optimize / Cleanup / Reports**: designed placeholder panels listing the
  planned actions, wired into navigation and ready to fill in.

## Next up (roadmap)

- Optimize: transactional batch fixes (Enable Nanite, Generate LODs, Light
  mobility, Texture compression, ISM/HISM instancing, Simple collision).
- Analyze: texture/material passes (oversized, non-pow2, slot count).
- Cleanup: project size breakdown, fix redirectors, delete unused, settings audit.
- Reports: CSV/JSON export, before/after snapshots.
