// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class IDetailsView;
struct FPropertyAndParent;
struct FPropertyChangedEvent;

/** Category-specific analysis thresholds shown above the Optimize tree. */
class SCategorySettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCategorySettingsPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCategorySettingsPanel() override;

private:
	void Refresh();
	bool IsSettingVisible(const FPropertyAndParent& PropertyAndParent) const;
	bool HasSettingsForSelectedCategory() const;
	FText GetSettingsTitle() const;
	FText GetNoSettingsText() const;
	void OnSettingChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<IDetailsView> SettingsView;
	TOptional<ECategory> DisplayedCategory;
	FDelegateHandle ChangedHandle;
};
