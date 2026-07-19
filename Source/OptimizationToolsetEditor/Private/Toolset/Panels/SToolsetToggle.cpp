// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Panels/SToolsetToggle.h"
#include "Toolset/ToolsetWidgetUtils.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SToolsetToggle"

using namespace ToolsetUI;

namespace
{
	// Track dimensions. The knob is the track height minus the inset on both sides.
	constexpr float TrackWidth = 38.0f;
	constexpr float TrackHeight = 20.0f;
	constexpr float KnobInset = 3.0f;
	constexpr float KnobSize = TrackHeight - KnobInset * 2.0f;

	// Off-track grey — a step up from the card edge so the switch reads as a
	// control, not a divot. On-track colour is the accent, set at build time.
	const FLinearColor TrackOff = FLinearColor(FColor(0x45, 0x4B, 0x53));
}

void SToolsetToggle::Construct(const FArguments& InArgs)
{
	IsCheckedAttr = InArgs._IsChecked;
	OnToggled = InArgs._OnToggled;

	ChildSlot
	[
		SNew(SButton)
		// Transparent normal with a faint hover: the track is the visual, the
		// button only carries the click and a hover cue.
		.ButtonStyle(&S(), "Toolset.Nav.Button")
		.ContentPadding(FMargin(0))
		.OnClicked(this, &SToolsetToggle::HandleClicked)
		[
			SNew(SBox)
			.WidthOverride(TrackWidth)
			.HeightOverride(TrackHeight)
			[
				SNew(SBorder)
				.BorderImage(Brush("Toolset.Fill.Pill"))
				.BorderBackgroundColor_Lambda([this]()
				{
					return IsOn() ? FSlateColor(FToolsetStyle::Accent) : FSlateColor(TrackOff);
				})
				.Padding(FMargin(KnobInset))
				[
					// The knob rides between two spacers whose weights swap with
					// state: 1/0 pins it left, 0/1 pins it right.
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(TAttribute<float>::CreateLambda([this]() { return IsOn() ? 1.0f : 0.0f; }))
					[ SNullWidget::NullWidget ]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(KnobSize).HeightOverride(KnobSize)
						[
							SNew(SBorder)
							.BorderImage(Brush("Toolset.Fill.Pill"))
							.BorderBackgroundColor_Lambda([this]()
							{
								// Dark knob on the bright accent stays legible; light
								// knob on the grey off-track keeps the same shape reading.
								return IsOn() ? FSlateColor(FToolsetStyle::OnAccent) : FSlateColor(FToolsetStyle::TextPrimary);
							})
							[ SNullWidget::NullWidget ]
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(TAttribute<float>::CreateLambda([this]() { return IsOn() ? 0.0f : 1.0f; }))
					[ SNullWidget::NullWidget ]
				]
			]
		]
	];
}

bool SToolsetToggle::IsOn() const
{
	return IsCheckedAttr.Get(false);
}

FReply SToolsetToggle::HandleClicked()
{
	OnToggled.ExecuteIfBound(!IsOn());
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
