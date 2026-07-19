// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/Panels/SProfilePanel.h"	// FProfileAction (command + view mode)
#include "Widgets/SCompoundWidget.h"

/**
 * Profile page for the new UI: the same verified stat / visualizer actions as the
 * old panel, rendered in plain default editor styling and living in Slate/ so the
 * redesign stays self-contained.
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
