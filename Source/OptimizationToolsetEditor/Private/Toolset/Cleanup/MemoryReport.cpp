// Copyright Optimization Toolset. All Rights Reserved.

#include "Toolset/Cleanup/MemoryReport.h"

#include "ContentStreaming.h"
#include "DynamicRHI.h"
#include "PixelFormat.h"
#include "RHIStats.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/UObjectIterator.h"

namespace
{
	bool IsProjectAsset(const UObject& Object)
	{
		const FString Path = Object.GetPathName();
		return !Path.StartsWith(TEXT("/Engine/")) && !Path.StartsWith(TEXT("/Script/"))
			&& !Object.HasAnyFlags(RF_Transient);
	}

	/** A texture's likely role from its name — the breakdown the VRAM view shows. */
	FString RoleFromName(const FString& Name)
	{
		const FString Lower = Name.ToLower();
		if (Lower.Contains(TEXT("lightmap")) || Lower.Contains(TEXT("shadowmap")))
		{
			return TEXT("Lightmap");
		}
		if (Lower.StartsWith(TEXT("t_ui_")) || Lower.StartsWith(TEXT("ui_")) || Lower.Contains(TEXT("_ui_")))
		{
			return TEXT("UI");
		}
		if (Lower.EndsWith(TEXT("_n")) || Lower.Contains(TEXT("_normal")) || Lower.Contains(TEXT("_nrm")))
		{
			return TEXT("Normal");
		}
		if (Lower.EndsWith(TEXT("_d")) || Lower.Contains(TEXT("_diffuse")) || Lower.Contains(TEXT("_albedo")) || Lower.Contains(TEXT("_basecolor")))
		{
			return TEXT("Diffuse");
		}
		return TEXT("Other");
	}

	FString StreamStateOf(const UTexture2D& Texture)
	{
		if (Texture.VirtualTextureStreaming)
		{
			return TEXT("Virtual");
		}
		if (Texture.NeverStream)
		{
			return TEXT("NeverStream");
		}
		return FString::Printf(TEXT("Stream %d/%d"), Texture.GetNumResidentMips(), Texture.GetNumMips());
	}

	int64 TextureFullBytes(UTexture2D& Texture)
	{
		int64 Bytes = Texture.CalcTextureMemorySizeEnum(TMC_AllMips);
		if (Bytes <= 0)
		{
			Bytes = Texture.GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
		}
		return FMath::Max<int64>(Bytes, 0);
	}
}

FMemoryReport FMemoryReport::Compute(int32 TopN)
{
	FMemoryReport Report;
	const double StartTime = FPlatformTime::Seconds();

	// --- RHI / streaming figures --------------------------------------------
	FTextureMemoryStats RHIStats;
	RHIGetTextureMemoryStats(RHIStats);
	Report.DedicatedVideoMemory = FMath::Max<int64>(RHIStats.DedicatedVideoMemory, 0);
	Report.TexturePoolSize = FMath::Max<int64>(RHIStats.TexturePoolSize, 0);
	Report.StreamingPoolUsed = static_cast<int64>(RHIStats.StreamingMemorySize);

	if (IStreamingManager::Get_Concurrent())
	{
		IRenderAssetStreamingManager& Streamer = IStreamingManager::Get().GetTextureStreamingManager();
		Report.StreamingPoolBudget = Streamer.GetPoolSize();
		Report.StreamingOverBudget = FMath::Max<int64>(Streamer.GetMemoryOverBudget(), 0);
	}

	// --- Textures ------------------------------------------------------------
	TMap<FString, FMemoryRoleTotal> Roles;
	for (TObjectIterator<UTexture2D> It; It; ++It)
	{
		UTexture2D* Texture = *It;
		if (!Texture || !IsProjectAsset(*Texture))
		{
			continue;
		}

		FMemoryTextureRow Row;
		Row.Name = Texture->GetName();
		Row.Path = Texture->GetPathName();
		Row.SizeX = Texture->GetSizeX();
		Row.SizeY = Texture->GetSizeY();
		Row.Format = GPixelFormats[Texture->GetPixelFormat()].Name;
		Row.NumMips = Texture->GetNumMips();
		Row.ResidentMips = Texture->GetNumResidentMips();
		Row.FullBytes = TextureFullBytes(*Texture);

		const int64 Resident = Texture->CalcTextureMemorySizeEnum(TMC_ResidentMips);
		Row.ResidentBytes = Resident > 0 ? Resident : Row.FullBytes;
		Row.Role = RoleFromName(Row.Name);
		Row.StreamState = StreamStateOf(*Texture);

		if (Row.FullBytes <= 0 || Row.SizeX <= 0)
		{
			continue;
		}

		Report.TextureFullTotal += Row.FullBytes;
		Report.TextureResidentTotal += Row.ResidentBytes;

		FMemoryRoleTotal& RoleTotal = Roles.FindOrAdd(Row.Role);
		RoleTotal.Role = Row.Role;
		RoleTotal.FullBytes += Row.FullBytes;
		++RoleTotal.Count;

		Report.Textures.Add(MoveTemp(Row));
	}

	// --- Render targets ------------------------------------------------------
	for (TObjectIterator<UTextureRenderTarget2D> It; It; ++It)
	{
		UTextureRenderTarget2D* Target = *It;
		if (!Target || !Target->GetResource())
		{
			continue;
		}

		FMemoryTextureRow Row;
		Row.Name = Target->GetName();
		Row.Path = Target->GetPathName();
		Row.SizeX = Target->SizeX;
		Row.SizeY = Target->SizeY;
		Row.Format = GPixelFormats[Target->GetFormat()].Name;
		Row.FullBytes = FMath::Max<int64>(Target->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal), 0);
		Row.ResidentBytes = Row.FullBytes;
		Row.Role = TEXT("RenderTarget");
		Row.StreamState = TEXT("Resident");

		Report.RenderTargetTotal += Row.FullBytes;
		Report.RenderTargets.Add(MoveTemp(Row));
	}

	// --- Meshes --------------------------------------------------------------
	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		UStaticMesh* Mesh = *It;
		if (!Mesh || !IsProjectAsset(*Mesh))
		{
			continue;
		}

		FMemoryMeshRow Row;
		Row.Name = Mesh->GetName();
		Row.Path = Mesh->GetPathName();
		Row.Bytes = FMath::Max<int64>(Mesh->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal), 0);
		Row.LODs = Mesh->GetNumLODs();
		Row.Triangles = Mesh->GetNumTriangles(0);
		Row.Type = Mesh->IsNaniteEnabled() ? TEXT("Nanite") : TEXT("Static");

		Report.MeshTotal += Row.Bytes;
		Report.Meshes.Add(MoveTemp(Row));
	}

	for (TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* Mesh = *It;
		if (!Mesh || !IsProjectAsset(*Mesh))
		{
			continue;
		}

		FMemoryMeshRow Row;
		Row.Name = Mesh->GetName();
		Row.Path = Mesh->GetPathName();
		Row.Bytes = FMath::Max<int64>(Mesh->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal), 0);
		Row.LODs = Mesh->GetLODNum();
		Row.Type = TEXT("Skeletal");

		if (const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering())
		{
			if (RenderData->LODRenderData.Num() > 0)
			{
				int32 Tris = 0;
				for (const FSkelMeshRenderSection& Section : RenderData->LODRenderData[0].RenderSections)
				{
					Tris += Section.NumTriangles;
				}
				Row.Triangles = Tris;
			}
		}

		Report.MeshTotal += Row.Bytes;
		Report.Meshes.Add(MoveTemp(Row));
	}

	Report.GrandTotal = Report.TextureFullTotal + Report.RenderTargetTotal + Report.MeshTotal;

	// --- Sort + trim ---------------------------------------------------------
	Roles.GenerateValueArray(Report.TextureRoles);
	Report.TextureRoles.Sort([](const FMemoryRoleTotal& A, const FMemoryRoleTotal& B) { return A.FullBytes > B.FullBytes; });

	Report.Textures.Sort([](const FMemoryTextureRow& A, const FMemoryTextureRow& B) { return A.FullBytes > B.FullBytes; });
	Report.RenderTargets.Sort([](const FMemoryTextureRow& A, const FMemoryTextureRow& B) { return A.FullBytes > B.FullBytes; });
	Report.Meshes.Sort([](const FMemoryMeshRow& A, const FMemoryMeshRow& B) { return A.Bytes > B.Bytes; });

	if (TopN > 0)
	{
		if (Report.Textures.Num() > TopN) { Report.Textures.SetNum(TopN); }
		if (Report.RenderTargets.Num() > TopN) { Report.RenderTargets.SetNum(TopN); }
		if (Report.Meshes.Num() > TopN) { Report.Meshes.SetNum(TopN); }
	}

	Report.ComputeSeconds = FPlatformTime::Seconds() - StartTime;
	return Report;
}
