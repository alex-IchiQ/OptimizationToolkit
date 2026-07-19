// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Slate/SCleanupView.h"

#include "AssetManagerEditorModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "GameMapsSettings.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "Toolset/Cleanup/ICleanupAction.h"
#include "Toolset/ToolsetRegistry.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCleanupView"

namespace CleanupColumns
{
	static const FName Check("Check");
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
	// Count is the "All groups" sentinel; the rest fill in after a scan.
	GroupOptions.Add(MakeShared<EAssetCategory>(EAssetCategory::Count));

	ChildSlot
	[
		SNew(SVerticalBox)

		// Top bar: summary (the Scan action lives in the shell now).
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6, 6, 6, 4))
		[
			SNew(STextBlock).Text(this, &SCleanupView::GetSummaryText)
		]

		// Project-wide actions (fix up redirectors, save dirty packages).
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6, 0, 6, 4))
		[
			BuildActionBar()
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
				SNew(STextBlock).Text(LOCTEXT("GroupLabel", "Group:")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(160.0f)
				[
					SAssignNew(GroupCombo, SComboBox<TSharedPtr<EAssetCategory>>)
					.OptionsSource(&GroupOptions)
					.OnGenerateWidget(this, &SCleanupView::MakeGroupComboEntry)
					.OnSelectionChanged(this, &SCleanupView::OnGroupSelected)
					[
						SNew(STextBlock).Text(this, &SCleanupView::GetGroupComboLabel)
					]
				]
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(12, 0, 0, 0))
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Filter by name…"))
				.OnTextChanged(this, &SCleanupView::OnSearchChanged)
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
					+ SHeaderRow::Column(CleanupColumns::Check).FixedWidth(28.0f).HAlignHeader(HAlign_Center)
						.HeaderContent()
						[
							SNew(SCheckBox)
							.IsChecked(this, &SCleanupView::GetSelectAllState)
							.OnCheckStateChanged(this, &SCleanupView::OnSelectAllChanged)
							.ToolTipText(LOCTEXT("SelectAll", "Select all shown"))
						]
					+ SHeaderRow::Column(CleanupColumns::Name).DefaultLabel(LOCTEXT("ColName", "Asset")).FillWidth(2.5f)
					+ SHeaderRow::Column(CleanupColumns::Type).DefaultLabel(LOCTEXT("ColType", "Type")).FillWidth(1.4f)
					+ SHeaderRow::Column(CleanupColumns::Size).DefaultLabel(LOCTEXT("ColSize", "Size")).FillWidth(1.0f).HAlignHeader(HAlign_Right).HAlignCell(HAlign_Right)
					+ SHeaderRow::Column(CleanupColumns::Path).DefaultLabel(LOCTEXT("ColPath", "Path")).FillWidth(3.0f)
					+ SHeaderRow::Column(CleanupColumns::Note).DefaultLabel(LOCTEXT("ColNote", "Reason")).FillWidth(2.0f)
					+ SHeaderRow::Column(CleanupColumns::Nav).DefaultLabel(FText::GetEmpty()).FixedWidth(32.0f).HAlignHeader(HAlign_Center))
			]
		]

		// Bottom bar: selection count + delete.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6, 0, 6, 6))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(this, &SCleanupView::GetSelectionText)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.IsEnabled_Lambda([this]() { return IsDeleteEnabled(); })
				.OnClicked(this, &SCleanupView::OnDeleteClicked)
				[
					SNew(STextBlock).Text(LOCTEXT("Delete", "Delete Selected"))
				]
			]
		]
	];
}

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------
void SCleanupView::BuildEntries()
{
	AllEntries.Reset();
	CheckedEntries.Reset();	// old rows are gone; their pointers no longer mean anything

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
		EAssetCategory Category = EAssetCategory::Other;
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
		const EAssetCategory Category = FProjectSizeReport::CategoryForClass(ClassName);

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
			Entry->Category = Category;
			AllEntries.Add(Entry);
		}

		Candidates.Add({ Asset, Type, Category, FPackageName::GetLongPackagePath(PackagePath), Size });
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
			Entry->Category = Candidate.Category;
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

	// Rebuild the group dropdown from the categories actually present, in enum order.
	TSet<EAssetCategory> Present;
	for (const TSharedPtr<FCleanupEntry>& Entry : AllEntries)
	{
		Present.Add(Entry->Category);
	}
	GroupOptions.Reset();
	GroupOptions.Add(MakeShared<EAssetCategory>(EAssetCategory::Count));	// "All"
	for (uint8 Index = 0; Index < static_cast<uint8>(EAssetCategory::Count); ++Index)
	{
		const EAssetCategory Category = static_cast<EAssetCategory>(Index);
		if (Present.Contains(Category))
		{
			GroupOptions.Add(MakeShared<EAssetCategory>(Category));
		}
	}
	GroupFilter.Reset();
	if (GroupCombo.IsValid())
	{
		GroupCombo->RefreshOptions();
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
		if (GroupFilter.IsSet() && Entry->Category != GroupFilter.GetValue())
		{
			continue;
		}
		if (!SearchText.IsEmpty() && !Entry->Asset.AssetName.ToString().Contains(SearchText))
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

void SCleanupView::OnSearchChanged(const FText& NewText)
{
	SearchText = NewText.ToString();
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
// Group combo
// ---------------------------------------------------------------------------
FText SCleanupView::GroupOptionLabel(const TSharedPtr<EAssetCategory>& Item)
{
	if (!Item.IsValid() || *Item == EAssetCategory::Count)
	{
		return LOCTEXT("GroupAll", "All");
	}
	return FProjectSizeReport::LabelForCategory(*Item);
}

TSharedRef<SWidget> SCleanupView::MakeGroupComboEntry(TSharedPtr<EAssetCategory> Item)
{
	return SNew(STextBlock).Text(GroupOptionLabel(Item));
}

void SCleanupView::OnGroupSelected(TSharedPtr<EAssetCategory> Item, ESelectInfo::Type)
{
	if (!Item.IsValid())
	{
		return;
	}
	// Count is the "All" sentinel; anything else narrows to that category.
	if (*Item == EAssetCategory::Count)
	{
		GroupFilter.Reset();
	}
	else
	{
		GroupFilter = *Item;
	}
	ApplyFilters();
}

FText SCleanupView::GetGroupComboLabel() const
{
	return GroupFilter.IsSet() ? FProjectSizeReport::LabelForCategory(GroupFilter.GetValue()) : LOCTEXT("GroupAll", "All");
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

	if (ColumnId == CleanupColumns::Check)
	{
		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SCleanupView::GetItemCheck, Entry)
			.OnCheckStateChanged(this, &SCleanupView::OnItemCheckChanged, Entry)
		];
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

// ---------------------------------------------------------------------------
// Project-wide actions
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SCleanupView::BuildActionBar()
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	// Registry-driven, minus Delete Unused — this page does its own delete.
	for (const TUniquePtr<ICleanupAction>& Action : FToolsetRegistry::Get().GetActions())
	{
		if (!Action || !Action->IsSupported() || Action->GetId() == TEXT("Cleanup_DeleteUnusedAssets"))
		{
			continue;
		}

		const ICleanupAction* ActionPtr = Action.Get();	// registry owns it; outlives the button
		Row->AddSlot().AutoWidth().Padding(FMargin(0, 0, 4, 0))
		[
			SNew(SButton)
			.ToolTipText(Action->GetDescription())
			.OnClicked(this, &SCleanupView::OnRunAction, ActionPtr)
			[
				SNew(STextBlock).Text(Action->GetButtonLabel())
			]
		];
	}

	// Last-run summary, so a fire-and-forget action still reports what it did.
	Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(10, 0, 0, 0))
	[
		SNew(STextBlock)
		.Text_Lambda([this]() { return LastActionResult; })
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
	];

	return Row;
}

FReply SCleanupView::OnRunAction(const ICleanupAction* Action)
{
	if (!Action)
	{
		return FReply::Handled();
	}

	// Anything that rewrites assets confirms first, unless it runs its own review.
	if (Action->NeedsConfirmation())
	{
		const FText Message = FText::Format(
			LOCTEXT("ConfirmAction", "{0}\n\n{1}\n\nThis cannot be undone. Continue?"),
			Action->GetTitle(), Action->GetDescription());
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}
	}

	FScopedSlowTask SlowTask(0.0f, Action->GetTitle());
	SlowTask.MakeDialog();

	LastActionResult = Action->Execute();
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Selection + delete
// ---------------------------------------------------------------------------
ECheckBoxState SCleanupView::GetItemCheck(TSharedPtr<FCleanupEntry> Entry) const
{
	return CheckedEntries.Contains(Entry) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCleanupView::OnItemCheckChanged(ECheckBoxState State, TSharedPtr<FCleanupEntry> Entry)
{
	if (State == ECheckBoxState::Checked)
	{
		CheckedEntries.Add(Entry);
	}
	else
	{
		CheckedEntries.Remove(Entry);
	}
}

ECheckBoxState SCleanupView::GetSelectAllState() const
{
	// Reflects the shown rows: all ticked, none, or some.
	if (VisibleEntries.Num() == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	int32 Checked = 0;
	for (const TSharedPtr<FCleanupEntry>& Entry : VisibleEntries)
	{
		Checked += CheckedEntries.Contains(Entry) ? 1 : 0;
	}
	if (Checked == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	return Checked == VisibleEntries.Num() ? ECheckBoxState::Checked : ECheckBoxState::Undetermined;
}

void SCleanupView::OnSelectAllChanged(ECheckBoxState State)
{
	// Toggle the shown rows only — a filtered-out row shouldn't be swept up.
	const bool bSelect = State == ECheckBoxState::Checked;
	for (const TSharedPtr<FCleanupEntry>& Entry : VisibleEntries)
	{
		if (bSelect)
		{
			CheckedEntries.Add(Entry);
		}
		else
		{
			CheckedEntries.Remove(Entry);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

bool SCleanupView::IsDeleteEnabled() const
{
	return CheckedEntries.Num() > 0;
}

FReply SCleanupView::OnDeleteClicked()
{
	TArray<FAssetData> ToDelete;
	for (const TSharedPtr<FCleanupEntry>& Entry : CheckedEntries)
	{
		if (Entry.IsValid())
		{
			ToDelete.Add(Entry->Asset);
		}
	}
	if (ToDelete.IsEmpty())
	{
		return FReply::Handled();
	}

	// Unreal's own dialog lists everything, re-checks references at delete time and
	// offers force delete — hand-rolling that would be worse in every way. After it,
	// rescan so the lists reflect what actually went.
	ObjectTools::DeleteAssets(ToDelete, /*bShowConfirmation*/ true);
	BuildEntries();
	return FReply::Handled();
}

FText SCleanupView::GetSelectionText() const
{
	return FText::Format(LOCTEXT("SelectedFmt", "{0} selected"), FText::AsNumber(CheckedEntries.Num()));
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
