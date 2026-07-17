// Copyright Optimization Toolset. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolset/ToolsetTypes.h"
#include "Widgets/SCompoundWidget.h"

class FToolsetModel;

/**
 * The level-scale numbers on the Dashboard, in display order.
 *
 * An enum rather than five hand-built cards: the label, the tooltip, the value
 * and the delta all key off it, so a new number is one switch arm per property
 * and no new widget code.
 */
enum class ELevelStat : uint8
{
	Meshes = 0,
	Triangles,
	Actors,
	Materials,
	Lights,
	Count
};

/**
 * Landing panel: what the level *is* (level stats), then what is wrong with it
 * (severity counts), then how to proceed.
 *
 * Every number here is bound through a lambda onto the model, so this panel needs
 * no OnChanged subscription — a rescan redraws it on the next tick.
 */
class SDashboardPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDashboardPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FToolsetModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildLevelStatsCard();
	TSharedRef<SWidget> MakeLevelStatCell(ELevelStat Stat);
	TSharedRef<SWidget> MakeSeverityStatCard(ESeverity Severity);

	static FText LabelForLevelStat(ELevelStat Stat);
	static FText TooltipForLevelStat(ELevelStat Stat);
	static int64 ValueForLevelStat(const FLevelStats& Stats, ELevelStat Stat);

	/** Value now minus value at the previous scan. Meaningless until there are two. */
	int64 DeltaForLevelStat(ELevelStat Stat) const;
	FLinearColor DeltaColorForLevelStat(ELevelStat Stat) const;

	TSharedPtr<FToolsetModel> Model;
};
