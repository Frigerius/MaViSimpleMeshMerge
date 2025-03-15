#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshMerge.h"

#include "MaViSimpleMeshMergeRequest.generated.h"

struct FMaViSimpleMeshMergeRequestImpl;

USTRUCT(BlueprintType)
struct FMaViSimpleMeshMergeRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSkelMeshMergeSectionMapping> MeshSectionMappings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSkelMeshMergeUVTransformMapping UVTransformsPerMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USkeletalMesh>> MeshesToMerge;

    friend uint32 GetTypeHash(const FMaViSimpleMeshMergeRequest& InObject);
    bool operator==(const FMaViSimpleMeshMergeRequest& Other) const;
};

USTRUCT(BlueprintType)
struct MAVISIMPLEMESHMERGE_API FMaViSimpleMeshMergeRequestHandle
{
    GENERATED_BODY()
    FMaViSimpleMeshMergeRequestHandle() = default;

    explicit FMaViSimpleMeshMergeRequestHandle(const TSharedPtr<FMaViSimpleMeshMergeRequestImpl>& Request)
        : WeakRequest(Request) {}

    bool IsValid() const { return WeakRequest.Pin() != nullptr; }
    // Reset will not abort the Request, it will just null the weakptr!
    void Reset() { WeakRequest = nullptr; }

private:
    friend class UMaViSimpleMeshMergeWorldSubsystem;
    TWeakPtr<FMaViSimpleMeshMergeRequestImpl> WeakRequest;
};
