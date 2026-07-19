// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnToolsetToggleChanged, bool);

/**
 * A pill switch, on brand where a stock SCheckBox is not.
 *
 * The editor's checkbox reads as a form control from a different design system;
 * this is a rounded track (accent when on, grey when off) with a knob that slides
 * between the ends. State is a bound attribute, so it can front the model without
 * this widget holding any of its own — flip the source and the knob follows.
 */
class SToolsetToggle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SToolsetToggle) {}
		SLATE_ATTRIBUTE(bool, IsChecked)
		SLATE_EVENT(FOnToolsetToggleChanged, OnToggled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	bool IsOn() const;
	FReply HandleClicked();

	TAttribute<bool> IsCheckedAttr;
	FOnToolsetToggleChanged OnToggled;
};
