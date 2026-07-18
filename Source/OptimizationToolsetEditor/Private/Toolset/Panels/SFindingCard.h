// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;

/** Presentation and actions for one finding leaf in the Optimize tree. */
class SFindingCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFindingCard) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
		SLATE_ARGUMENT(TSharedPtr<FFinding>, Finding)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	static FText GetScopeLabel(EFindingScope Scope);

	TSharedPtr<FToolsetModel> Model;
	TSharedPtr<FFinding> Finding;
};
