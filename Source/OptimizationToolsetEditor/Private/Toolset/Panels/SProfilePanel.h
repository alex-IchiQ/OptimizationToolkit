// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * One labelled thing the Profile panel can do to the viewport.
 *
 * Almost every entry is "run a console variable, then put the viewport in the
 * view mode that displays it" — so both halves live in one record and the panel
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
 * Console stat stacks and GPU visualizers for the editor viewport.
 *
 * Takes no model: nothing here reads the scan. It is the one panel that talks to
 * the viewport rather than to the level.
 */
class SProfilePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProfilePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** A titled block of buttons in a uniform grid. */
	static TSharedRef<SWidget> MakeSection(const FText& Title, const FText& Hint, const TArray<FProfileAction>& Actions, int32 Columns = 4);
	static TSharedRef<SWidget> MakeActionButton(const FProfileAction& Action);

	/** Runs the command and applies the view mode, in that order. */
	static void RunAction(const FProfileAction& Action);

	/** Sets the view mode on the level viewport the user is actually looking at. */
	static void ApplyViewMode(EViewModeIndex ViewMode);

	TSharedRef<SWidget> BuildCustomCommandCard();

	FString CustomCommand;
};
