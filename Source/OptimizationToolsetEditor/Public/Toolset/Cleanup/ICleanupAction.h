// Copyright 2026 IchiQ (Aleksey Karpov). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * One project-wide cleanup operation.
 *
 * The third feature family alongside IAnalyzePass and IOptimizationFix, and it
 * follows the same rule: one action = one class = one file, registered in
 * FToolsetRegistry::RegisterDefaults(). The Cleanup panel builds itself from
 * the registry, so adding an action never touches the window.
 *
 * Unlike a fix, an action isn't tied to a finding — it operates on the project
 * as a whole, and most of what it does is not Undo-able. Anything that rewrites
 * or removes assets must report IsDestructive() so the panel can confirm first.
 */
class ICleanupAction
{
public:
	virtual ~ICleanupAction() = default;

	/** Stable id, also used to key the last-run summary in the panel. */
	virtual FName GetId() const = 0;

	/** Card heading, e.g. "Fix up redirectors". */
	virtual FText GetTitle() const = 0;

	/** One or two sentences on what this does and what it touches. */
	virtual FText GetDescription() const = 0;

	/** Text on the run button, e.g. "Fix up". */
	virtual FText GetButtonLabel() const = 0;

	/** True when the action rewrites or removes assets. Tags the card as not undoable. */
	virtual bool IsDestructive() const { return false; }

	/**
	 * Whether the panel should put its own confirmation in front of Execute().
	 * Defaults to IsDestructive(); override to false when Execute() already runs
	 * a better review of its own, so the user isn't asked the same thing twice.
	 */
	virtual bool NeedsConfirmation() const { return IsDestructive(); }

	/** Runtime availability on the current engine / project state. */
	virtual bool IsSupported() const { return true; }

	/** Runs the action and returns a human-readable summary of what happened. */
	virtual FText Execute() const = 0;
};
