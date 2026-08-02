// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"	// EViewModeIndex
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * One labelled thing the Profile page can do to the viewport.
 *
 * Almost every entry is "run a console variable, then put the viewport in the
 * view mode that displays it" — so both halves live in one record and the page
 * builds itself from arrays instead of a hundred hand-written buttons.
 */
struct FProfileAction
{
	FText Label;

	/** Console command / cvar assignment. Empty to only change the view mode. */
	FString Command;

	/** VMI_Unknown leaves the viewport's view mode alone (the stat buttons do). */
	EViewModeIndex ViewMode = VMI_Unknown;

	FText Tooltip;
};

/**
 * Profile page: console stat stacks and GPU visualizers for the editor viewport.
 *
 * Takes no model — nothing here reads the scan; it is the one page that talks to
 * the viewport rather than to the level.
 */
class SProfileView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProfileView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	static TSharedRef<SWidget> MakeSection(const FText& Title, const FText& Hint, const TArray<FProfileAction>& Actions, int32 Columns);
	static TSharedRef<SWidget> MakeActionButton(const FProfileAction& Action);
	TSharedRef<SWidget> BuildCustomCommandCard();

	static void RunAction(const FProfileAction& Action);
	static void ApplyViewMode(EViewModeIndex ViewMode);

	/** Whether the viewport currently reflects this action (its stat, view mode or
	 *  visualization channel). Read straight from the viewport, so it survives the
	 *  tab closing and reopening. Utility actions (Reset to Lit, Clear stats) have
	 *  no state and are never highlighted. */
	static bool IsActionActive(const FProfileAction& Action);

	FString CustomCommand;
};
