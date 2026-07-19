// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class SVerticalBox;
class FProperty;
class FIntProperty;
class FBoolProperty;

/**
 * Category-specific analysis thresholds shown above the Optimize tree.
 *
 * Renders the reflected settings for the selected category as first-class toolset
 * rows — a styled spin box per number, a switch per flag — rather than dropping in
 * the engine's IDetailsView, which arrives in its own visual language and refuses
 * to be themed to match the rest of the panel.
 */
class SCategorySettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCategorySettingsPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCategorySettingsPanel() override;

private:
	/** Rebuilds the rows when the selected category changes. Bound to OnChanged. */
	void Refresh();
	void RebuildRows();

	/** Which settings Category metadata values belong to a finding category. */
	static bool PropertyMatchesCategory(const FProperty& Property, ECategory Category);

	TSharedRef<SWidget> MakeSettingRow(FProperty& Property);
	TSharedRef<SWidget> MakeIntEditor(FIntProperty& Property);
	TSharedRef<SWidget> MakeBoolEditor(FBoolProperty& Property);

	/** Persist and re-scan after any edit — the same contract the old view had. */
	void CommitChange();

	bool HasSettingsForSelectedCategory() const;
	FText GetSettingsTitle() const;
	FText GetNoSettingsText() const;

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<SVerticalBox> SettingsBox;
	TOptional<ECategory> DisplayedCategory;
	FDelegateHandle ChangedHandle;
};
