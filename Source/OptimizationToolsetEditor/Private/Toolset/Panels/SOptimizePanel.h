// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;
class IDetailsView;
class SFindingTree;
struct FPropertyAndParent;
struct FPropertyChangedEvent;

/** Unified workspace for configuring, reviewing, navigating to, and fixing findings. */
class SOptimizePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOptimizePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOptimizePanel() override;

private:
	void Refresh();
	TSharedRef<SWidget> BuildSettingsPanel();
	TSharedRef<SWidget> MakeSeverityFilterButton(ESeverity Severity);
	TSharedRef<SWidget> MakeFindingCard(TSharedPtr<FFinding> Item);

	bool IsSettingVisible(const FPropertyAndParent& PropertyAndParent) const;
	bool HasSettingsForSelectedCategory() const;
	FText GetSettingsTitle() const;
	FText GetNoSettingsText() const;
	void OnSettingChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	static bool CanNavigateTo(const FFinding& Finding);
	static FText GetScopeLabel(EFindingScope Scope);
	static FText GetNavigationLabel(const FFinding& Finding);
	static void NavigateTo(const FFinding& Finding);
	static void OpenProjectRenderingSettings(FName FindingTypeId = NAME_None);

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<SFindingTree> Tree;
	TSharedPtr<IDetailsView> SettingsView;
	TOptional<ECategory> DisplayedSettingsCategory;
	FDelegateHandle ChangedHandle;
};
