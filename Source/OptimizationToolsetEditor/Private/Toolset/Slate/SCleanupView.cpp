// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SCleanupView.h"

#include "AssetManagerEditorModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCleanupView"

namespace CleanupColumns
{
	static const FName Name("Name");
	static const FName Type("Type");
	static const FName Size("Size");
	static const FName Path("Path");
	static const FName Note("Note");
	static const FName Nav("Nav");
}

namespace
{
	/**
	 * Packages named by project settings rather than referenced by an asset — the
	 * default maps, game mode and game instance live as config paths, so nothing on
	 * disk references them and they must never be called unused. (Same guard the
	 * Delete Unused action uses.)
	 */
	TSet<FString> GatherConfigNamedPackages()
	{
		TSet<FString> Packages;
		const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
		if (!MapsSettings)
		{
			return Packages;
		}

		auto AddPath = [&Packages](const FSoftObjectPath& Path)
		{
			const FString PackageName = Path.GetLongPackageName();
			if (!PackageName.IsEmpty())
			{
				Packages.Add(PackageName);
			}
		};

		AddPath(MapsSettings->EditorStartupMap);
		AddPath(MapsSettings->TransitionMap);
		AddPath(MapsSettings->GameInstanceClass);
		AddPath(FSoftObjectPath(UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Default)));
		AddPath(FSoftObjectPath(UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Server)));
		AddPath(FSoftObjectPath(UGameMapsSettings::GetGlobalDefaultGameMode()));
		return Packages;
	}

	bool IsExcludedPackage(const FString& PackagePath, FName ClassName, const TSet<FString>& ConfigNamed)
	{
		if (ConfigNamed.Contains(PackagePath))
		{
			return true;
		}
		// World Partition stores a package per actor here; the map doesn't reference
		// them like normal assets, so they read as unused but back level content.
		if (PackagePath.Contains(TEXT("/__ExternalActors__/")) || PackagePath.Contains(TEXT("/__ExternalObjects__/")))
		{
			return true;
		}
		// Maps are the roots everything hangs off — nothing references them — and a
		// fixed-up redirector is unreferenced by design and has its own action.
		return ClassName == TEXT("World") || ClassName == TEXT("ObjectRedirector");
	}

	int64 DiskSizeOf(IAssetManagerEditorModule& Module, const FAssetData& Asset)
	{
		int64 Size = 0;
		if (Module.GetIntegerValueForCustomColumn(Asset, IAssetManagerEditorModule::DiskSizeName, Size) && Size > 0)
		{
			return Size;
		}
		return 0;
	}
}

void SCleanupView::Construct(const FArguments& InArgs)
{
	TypeOptions.Add(MakeShared<FString>(TEXT("All")));

	ChildSlot
	[
		SNew(SVerticalBox)

		// Top bar: scan + summary.
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SCleanupView::OnScanClicked)
				[
					SNew(STextBlock).Text(LOCTEXT("Scan", "Scan Project"))
				]
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(12, 0, 0, 0))
			[
				SNew(STextBlock).Text(this, &SCleanupView::GetSummaryText)
			]
		]

		// Filter bar: kind buttons + type combo.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6, 0, 6, 4))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 3, 0))[ MakeKindButton(ECleanupFilter::All, LOCTEXT("FilterAll", "All")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 3, 0))[ MakeKindButton(ECleanupFilter::Duplicate, LOCTEXT("FilterDup", "Duplicate")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 12, 0))[ MakeKindButton(ECleanupFilter::Unused, LOCTEXT("FilterUnused", "Unused")) ]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 6, 0))
			[
				SNew(STextBlock).Text(LOCTEXT("TypeLabel", "Type:")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(160.0f)
				[
					SAssignNew(TypeCombo, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TypeOptions)
					.OnGenerateWidget(this, &SCleanupView::MakeTypeComboEntry)
					.OnSelectionChanged(this, &SCleanupView::OnTypeSelected)
					[
						SNew(STextBlock).Text(this, &SCleanupView::GetTypeComboLabel)
					]
				]
			]
		]

		// The list.
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(6, 0, 6, 6))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(ListView, SListView<TSharedPtr<FCleanupEntry>>)
				.ListItemsSource(&VisibleEntries)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SCleanupView::GenerateRow)
				.HeaderRow(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(CleanupColumns::Name).DefaultLabel(LOCTEXT("ColName", "Asset")).FillWidth(2.5f)
					+ SHeaderRow::Column(CleanupColumns::Type).DefaultLabel(LOCTEXT("ColType", "Type")).FillWidth(1.4f)
					+ SHeaderRow::Column(CleanupColumns::Size).DefaultLabel(LOCTEXT("ColSize", "Size")).FillWidth(1.0f).HAlignHeader(HAlign_Right).HAlignCell(HAlign_Right)
					+ SHeaderRow::Column(CleanupColumns::Path).DefaultLabel(LOCTEXT("ColPath", "Path")).FillWidth(3.0f)
					+ SHeaderRow::Column(CleanupColumns::Note).DefaultLabel(LOCTEXT("ColNote", "Reason")).FillWidth(2.0f)
					+ SHeaderRow::Column(CleanupColumns::Nav).DefaultLabel(FText::GetEmpty()).FixedWidth(32.0f).HAlignHeader(HAlign_Center))
			]
		]
	];
}

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------
FReply SCleanupView::OnScanClicked()
{
	BuildEntries();
	return FReply::Handled();
}

void SCleanupView::BuildEntries()
{
	AllEntries.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		bHasScanned = true;	// so the summary can explain why it's empty
		ApplyFilters();
		return;
	}

	IAssetManagerEditorModule* EditorModule = IAssetManagerEditorModule::IsAvailable()
		? &IAssetManagerEditorModule::Get() : nullptr;
	if (EditorModule)
	{
		EditorModule->GetCurrentRegistrySource();	// force the size source to initialize
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	const TSet<FString> ConfigNamed = GatherConfigNamedPackages();

	// Working record per non-excluded asset, so unused and duplicate share one walk.
	struct FWorking
	{
		FAssetData Asset;
		FString Type;
		FString Folder;
		int64 Size = 0;
	};
	TArray<FWorking> Candidates;
	Candidates.Reserve(Assets.Num());

	FScopedSlowTask SlowTask(Assets.Num(), LOCTEXT("Scanning", "Scanning project assets..."));
	SlowTask.MakeDialog(true);

	for (const FAssetData& Asset : Assets)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}
		SlowTask.EnterProgressFrame(1);

		const FString PackagePath = Asset.PackageName.ToString();
		const FName ClassName = Asset.AssetClassPath.GetAssetName();
		if (IsExcludedPackage(PackagePath, ClassName, ConfigNamed))
		{
			continue;
		}

		const int64 Size = EditorModule ? DiskSizeOf(*EditorModule, Asset) : 0;
		const FString Type = ClassName.ToString();

		// --- Unused: nothing references it in any dependency category. ----------
		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(Asset.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::All);
		Referencers.Remove(Asset.PackageName);
		if (Referencers.Num() == 0)
		{
			TSharedPtr<FCleanupEntry> Entry = MakeShared<FCleanupEntry>();
			Entry->Asset = Asset;
			Entry->Kind = ECleanupKind::Unused;
			Entry->DiskSize = Size;
			Entry->TypeName = Type;
			AllEntries.Add(Entry);
		}

		Candidates.Add({ Asset, Type, FPackageName::GetLongPackagePath(PackagePath), Size });
	}

	// --- Duplicates: same base name + type + size, across two or more folders. ---
	// A name heuristic, not a content hash: it catches the same pack re-imported
	// into different folders, which is the common real duplicate.
	TMap<FString, TArray<int32>> Groups;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FWorking& Candidate = Candidates[Index];
		const FString BaseName = FPackageName::GetShortName(Candidate.Asset.PackageName).ToLower();
		const FString Key = FString::Printf(TEXT("%s|%s|%lld"), *BaseName, *Candidate.Type, Candidate.Size);
		Groups.FindOrAdd(Key).Add(Index);
	}

	for (const TPair<FString, TArray<int32>>& Group : Groups)
	{
		if (Group.Value.Num() < 2)
		{
			continue;
		}
		TSet<FString> Folders;
		for (int32 Index : Group.Value)
		{
			Folders.Add(Candidates[Index].Folder);
		}
		if (Folders.Num() < 2)
		{
			continue;	// same name in one folder isn't a re-import duplicate
		}
		for (int32 Index : Group.Value)
		{
			const FWorking& Candidate = Candidates[Index];
			TSharedPtr<FCleanupEntry> Entry = MakeShared<FCleanupEntry>();
			Entry->Asset = Candidate.Asset;
			Entry->Kind = ECleanupKind::Duplicate;
			Entry->DiskSize = Candidate.Size;
			Entry->TypeName = Candidate.Type;
			Entry->DuplicateKey = Group.Key;
			Entry->DuplicateGroupSize = Group.Value.Num();
			AllEntries.Add(Entry);
		}
	}

	// Heaviest first.
	AllEntries.Sort([](const TSharedPtr<FCleanupEntry>& A, const TSharedPtr<FCleanupEntry>& B)
	{
		return A->DiskSize > B->DiskSize;
	});

	// Rebuild the type dropdown from the types actually present.
	TSet<FString> Types;
	for (const TSharedPtr<FCleanupEntry>& Entry : AllEntries)
	{
		Types.Add(Entry->TypeName);
	}
	TArray<FString> SortedTypes = Types.Array();
	SortedTypes.Sort();
	TypeOptions.Reset();
	TypeOptions.Add(MakeShared<FString>(TEXT("All")));
	for (const FString& Type : SortedTypes)
	{
		TypeOptions.Add(MakeShared<FString>(Type));
	}
	TypeFilter = TEXT("All");
	if (TypeCombo.IsValid())
	{
		TypeCombo->RefreshOptions();
	}

	bHasScanned = true;
	ApplyFilters();
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------
void SCleanupView::ApplyFilters()
{
	VisibleEntries.Reset();
	for (const TSharedPtr<FCleanupEntry>& Entry : AllEntries)
	{
		if (KindFilter == ECleanupFilter::Duplicate && Entry->Kind != ECleanupKind::Duplicate)
		{
			continue;
		}
		if (KindFilter == ECleanupFilter::Unused && Entry->Kind != ECleanupKind::Unused)
		{
			continue;
		}
		if (TypeFilter != TEXT("All") && Entry->TypeName != TypeFilter)
		{
			continue;
		}
		VisibleEntries.Add(Entry);
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

void SCleanupView::SetKindFilter(ECleanupFilter Filter)
{
	KindFilter = Filter;
	ApplyFilters();
}

TSharedRef<SWidget> SCleanupView::MakeKindButton(ECleanupFilter Filter, const FText& Label)
{
	return SNew(SButton)
		.ContentPadding(FMargin(10, 4))
		.ButtonColorAndOpacity_Lambda([this, Filter]()
		{
			return KindFilter == Filter ? FStyleColors::Primary : FSlateColor(FLinearColor::White);
		})
		.OnClicked_Lambda([this, Filter]() { SetKindFilter(Filter); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Label)
		];
}

// ---------------------------------------------------------------------------
// Type combo
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SCleanupView::MakeTypeComboEntry(TSharedPtr<FString> Item)
{
	return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
}

void SCleanupView::OnTypeSelected(TSharedPtr<FString> Item, ESelectInfo::Type)
{
	if (Item.IsValid())
	{
		TypeFilter = *Item;
		ApplyFilters();
	}
}

FText SCleanupView::GetTypeComboLabel() const
{
	return FText::FromString(TypeFilter);
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------
TSharedRef<ITableRow> SCleanupView::GenerateRow(TSharedPtr<FCleanupEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable)
{
	class SCleanupRow : public SMultiColumnTableRow<TSharedPtr<FCleanupEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SCleanupRow) {}
			SLATE_ARGUMENT(TFunction<TSharedRef<SWidget>(const FName&)>, OnCell)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
		{
			OnCell = InArgs._OnCell;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0, 2)), InOwner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			return OnCell ? OnCell(Column) : SNullWidget::NullWidget;
		}

	private:
		TFunction<TSharedRef<SWidget>(const FName&)> OnCell;
	};

	return SNew(SCleanupRow, OwnerTable)
		.OnCell([this, Entry](const FName& Column) { return GenerateCell(Column, Entry); });
}

TSharedRef<SWidget> SCleanupView::GenerateCell(const FName& ColumnId, TSharedPtr<FCleanupEntry> Entry)
{
	if (!Entry.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnId == CleanupColumns::Nav)
	{
		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4, 2))
			.ToolTipText(LOCTEXT("ShowAsset", "Show Asset"))
			.OnClicked_Lambda([Entry]()
			{
				if (GEditor)
				{
					if (UObject* Object = Entry->Asset.GetAsset())
					{
						GEditor->SyncBrowserToObject(Object);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SImage).Image(FAppStyle::GetBrush("Icons.Search")).ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	FText Text;
	EHorizontalAlignment Align = HAlign_Left;
	if (ColumnId == CleanupColumns::Name)
	{
		Text = FText::FromName(Entry->Asset.AssetName);
	}
	else if (ColumnId == CleanupColumns::Type)
	{
		Text = FText::FromString(Entry->TypeName);
	}
	else if (ColumnId == CleanupColumns::Size)
	{
		Text = Entry->DiskSize > 0 ? FText::AsMemory(Entry->DiskSize) : FText::FromString(TEXT("—"));
		Align = HAlign_Right;
	}
	else if (ColumnId == CleanupColumns::Path)
	{
		Text = FText::FromName(Entry->Asset.PackageName);
	}
	else if (ColumnId == CleanupColumns::Note)
	{
		Text = Entry->Kind == ECleanupKind::Duplicate
			? FText::Format(LOCTEXT("DupNote", "Duplicate · {0} copies"), FText::AsNumber(Entry->DuplicateGroupSize))
			: LOCTEXT("UnusedNote", "No references");
	}

	return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(6, 0))
	[
		SNew(STextBlock)
		.Text(Text)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		.Justification(Align == HAlign_Right ? ETextJustify::Right : ETextJustify::Left)
	];
}

FText SCleanupView::GetSummaryText() const
{
	if (!bHasScanned)
	{
		return LOCTEXT("NoScan", "No scan yet — press Scan Project.");
	}

	int32 Duplicates = 0;
	int32 Unused = 0;
	for (const TSharedPtr<FCleanupEntry>& Entry : AllEntries)
	{
		(Entry->Kind == ECleanupKind::Duplicate ? Duplicates : Unused)++;
	}
	return FText::Format(LOCTEXT("SummaryFmt", "{0} duplicate · {1} unused"),
		FText::AsNumber(Duplicates), FText::AsNumber(Unused));
}

#undef LOCTEXT_NAMESPACE
