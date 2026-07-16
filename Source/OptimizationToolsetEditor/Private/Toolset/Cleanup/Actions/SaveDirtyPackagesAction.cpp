// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Cleanup/Actions/SaveDirtyPackagesAction.h"

#include "FileHelpers.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "SaveDirtyPackagesAction"

FText FSaveDirtyPackagesAction::GetTitle() const
{
	return LOCTEXT("Title", "Save all modified packages");
}

FText FSaveDirtyPackagesAction::GetDescription() const
{
	return LOCTEXT("Description", "Writes every unsaved map and content package to disk. Run this before a scan so results reflect what is actually on disk.");
}

FText FSaveDirtyPackagesAction::GetButtonLabel() const
{
	return LOCTEXT("Button", "Save all");
}

FText FSaveDirtyPackagesAction::Execute() const
{
	TArray<UPackage*> DirtyMapPackages;
	TArray<UPackage*> DirtyContentPackages;
	FEditorFileUtils::GetDirtyWorldPackages(DirtyMapPackages);
	FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

	const int32 DirtyCount = DirtyMapPackages.Num() + DirtyContentPackages.Num();
	if (DirtyCount == 0)
	{
		return LOCTEXT("NothingToSave", "Nothing to save — no modified packages.");
	}

	FEditorFileUtils::SaveDirtyPackages(
		/*bPromptUserToSave*/ false,
		/*bSaveMapPackages*/ true,
		/*bSaveContentPackages*/ true);

	// Re-query rather than trusting the return value: anything still dirty was
	// skipped (read-only on disk, checked out by someone else, and so on).
	DirtyMapPackages.Reset();
	DirtyContentPackages.Reset();
	FEditorFileUtils::GetDirtyWorldPackages(DirtyMapPackages);
	FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);
	const int32 RemainingCount = DirtyMapPackages.Num() + DirtyContentPackages.Num();

	if (RemainingCount > 0)
	{
		return FText::Format(
			LOCTEXT("SavedWithRemainder", "Saved {0} of {1} packages; {2} could not be written (read-only or not checked out)."),
			FText::AsNumber(DirtyCount - RemainingCount), FText::AsNumber(DirtyCount), FText::AsNumber(RemainingCount));
	}

	return FText::Format(LOCTEXT("Saved", "Saved {0} packages."), FText::AsNumber(DirtyCount));
}

#undef LOCTEXT_NAMESPACE
