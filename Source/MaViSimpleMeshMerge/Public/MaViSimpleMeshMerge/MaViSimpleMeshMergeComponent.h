#pragma once

#include "CoreMinimal.h"
#include "MaViSimpleMeshMergeRequest.h"
#include "Components/ActorComponent.h"
#include "MaViSimpleMeshMergeComponent.generated.h"

class UAnimInstance;
struct FStreamableHandle;
class USkeleton;
class USkeletalMesh;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMaViSimpleMeshMergeFinished);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MAVISIMPLEMESHMERGE_API UMaViSimpleMeshMergeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMaViSimpleMeshMergeComponent();

    void OverrideData(const TArray<TSoftObjectPtr<USkeletalMesh>>& NewMeshes, const TSoftObjectPtr<UMaterialInterface>& NewMaterial, const TSoftClassPtr<UAnimInstance>& NewAnimClass);

protected:
    void OnRegister() override;
    void OnUnregister() override;

    void OnMeshReady(USkeletalMesh* Mesh) const;
    void OnStreamingCompleted();

    void RefreshMesh();

    UPROPERTY(EditAnywhere)
    TSoftClassPtr<UAnimInstance> AnimClass;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UMaterialInterface> Material;

    UPROPERTY(EditAnywhere)
    TArray<TSoftObjectPtr<USkeletalMesh>> MeshesToMerge;

    UPROPERTY(BlueprintAssignable)
    FMaViSimpleMeshMergeFinished MeshMergeFinished;

    TSharedPtr<FStreamableHandle> StreamingHandle;
    FMaViSimpleMeshMergeRequestHandle RequestHandle;
};
