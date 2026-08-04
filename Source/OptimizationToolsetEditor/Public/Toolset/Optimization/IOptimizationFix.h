// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"

/**
 * One safe auto-fix: resolves findings whose FixId matches GetId().
 *
 * IsSupported() is the runtime gate (RHI / platform / project state); the
 * compile-time gate stays in the implementation via OPTIMIZATION_* macros.
 * Apply() owns its own transaction so every fix is individually Undo/Redo-able.
 */
class IOptimizationFix
{
public:
	virtual ~IOptimizationFix() = default;

	/** Must match the FFinding::FixId this fix resolves. */
	virtual FName GetId() const = 0;

	/** Button label shown in the Optimize panel. */
	virtual FText GetLabel() const = 0;

	/** Runtime availability on the current engine / platform. */
	virtual bool IsSupported() const = 0;

	/** Apply the fix for one finding. Returns true if something changed. */
	virtual bool Apply(const FFinding& Finding) const = 0;
};
