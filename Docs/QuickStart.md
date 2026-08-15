# Ultimate Optimization Toolkit — Quick Start

## Installation

### Install from Fab

1. Add the plugin to your Fab library and install it for the matching Unreal
   Engine version.
2. Open the target project and enable **Ultimate Optimization Toolkit** under
   **Edit → Plugins** if it is not already enabled.
3. Restart the editor when prompted.

### Install into a project manually

1. Copy the plugin folder to
   `<YourProject>/Plugins/OptimizationToolkit`.
2. Regenerate project files and build the editor target, or allow Unreal Engine
   to compile the plugin when the project opens.
3. Enable the plugin under **Edit → Plugins** and restart the editor if prompted.

The plugin is editor-only. It does not add runtime code to the packaged game and
does not require Blueprint setup.

## First use

1. Open the map you want to analyze. Load any sub-levels that should participate
   in the scan.
2. Open the toolkit using the **Optimize** toolbar button or
   **Window → Ultimate Optimization Toolkit**.
3. On the Dashboard, review **Scanning Scope** and disable any loaded level that
   should be excluded.
4. Optionally adjust the analysis thresholds on the Dashboard.
5. Press **Scan** in the left sidebar.
6. Open **Optimize** to review findings grouped by problem type. Use the
   navigation action to open the affected asset, focus an actor, or open Project
   Settings. Apply an automatic fix only after reviewing the recommendation.
7. Use **Analyzer** for loaded memory, **Profile** for viewport profiling and
   visualizers, and **Clean Up** for project-wide maintenance.

## Safety

- Scanning is read-only. Assets and actors change only after an explicit fix or
  confirmed cleanup action.
- Supported editor fixes use Undo/Redo where the engine operation permits it.
- Cleanup deletion is destructive and may not be undoable. Review Unreal
  Engine's deletion dialog and use source control or a backup.
- “Unreferenced” does not guarantee “unused” when content is loaded dynamically
  by name, string, configuration, or custom code.
- “Possible duplicate” is a review hint based on matching name, type, and disk
  size; it is not a byte-for-byte content comparison.

## Support

- Documentation: https://github.com/alex-IchiQ/OptimizationToolkit/blob/main/README.md
- Issues: https://github.com/alex-IchiQ/OptimizationToolkit/issues
