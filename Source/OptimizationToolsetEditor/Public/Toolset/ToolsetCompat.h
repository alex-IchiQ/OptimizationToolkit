// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

/**
 * Engine-version gating helpers.
 *
 * The plugin targets a wide UE range (5.3 - 5.7). Some APIs (VSM controls,
 * newer VRS/TSR knobs, Nanite tessellation, etc.) only exist on recent
 * engines, so feature code is wrapped in the macros below. On FAB every
 * engine version is shipped as its own build from this same source tree —
 * the preprocessor keeps a single codebase compiling everywhere.
 *
 * Macros keep the full "OPTIMIZATION_" prefix on purpose: they live in the
 * global preprocessor namespace, so a distinctive prefix avoids clashes with
 * other plugins in the same project.
 */

#define OPTIMIZATION_UE_VERSION_AT_LEAST(Major, Minor) (ENGINE_MAJOR_VERSION > (Major) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))
#define OPTIMIZATION_UE_VERSION_BELOW(Major, Minor) (!OPTIMIZATION_UE_VERSION_AT_LEAST(Major, Minor))

// Feature flags derived from the engine version.
#define OPTIMIZATION_HAS_NANITE         OPTIMIZATION_UE_VERSION_AT_LEAST(5, 0)
#define OPTIMIZATION_HAS_VSM_CONTROLS   OPTIMIZATION_UE_VERSION_AT_LEAST(5, 4)
#define OPTIMIZATION_HAS_MODERN_TSR     OPTIMIZATION_UE_VERSION_AT_LEAST(5, 3)
