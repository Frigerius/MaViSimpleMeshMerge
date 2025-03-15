#include "MaViSimpleMeshMerge/MaViSimpleMeshMergeComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "MaViSimpleMeshMerge/MaViSimpleMeshMergeWorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaViSimpleMeshMergeComponent)

UMaViSimpleMeshMergeComponent::UMaViSimpleMeshMergeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMaViSimpleMeshMergeComponent::OverrideData(const TArray<TSoftObjectPtr<USkeletalMesh>>& NewMeshes, const TSoftObjectPtr<UMaterialInterface>& NewMaterial, const TSoftClassPtr<UAnimInstance>& NewAnimClass)
{
    if (!ensure(!NewMeshes.IsEmpty()))
        return;
    MeshesToMerge = NewMeshes;
    if (!NewMaterial.IsNull())
    {
        Material = NewMaterial;
    }
    if (!NewAnimClass.IsNull())
    {
        AnimClass = NewAnimClass;
    }
    RefreshMesh();
}

void UMaViSimpleMeshMergeComponent::OnRegister()
{
    Super::OnRegister();
    RefreshMesh();
}

void UMaViSimpleMeshMergeComponent::OnUnregister()
{
    if (StreamingHandle)
    {
        StreamingHandle->CancelHandle();
        StreamingHandle = nullptr;
    }
    UMaViSimpleMeshMergeWorldSubsystem::AbortRequest(RequestHandle);
    Cast<ACharacter>(GetOwner())->GetMesh()->SetSkeletalMesh(nullptr);
    Super::OnUnregister();
}

void UMaViSimpleMeshMergeComponent::OnMeshReady(USkeletalMesh* Mesh) const
{
    if (!Mesh)
        return;
    auto* OwnerMesh = CastChecked<ACharacter>(GetOwner())->GetMesh();
    OwnerMesh->SetSkeletalMesh(Mesh, true);
    OwnerMesh->TickAnimation(0, false);
    OwnerMesh->RefreshBoneTransforms();
    OwnerMesh->ClearMotionVector();
#if WITH_EDITOR
    if (GetWorld()->IsGameWorld())
#endif
    {
        MeshMergeFinished.Broadcast();
    }
}

void UMaViSimpleMeshMergeComponent::OnStreamingCompleted()
{
    StreamingHandle = nullptr;
    auto* OwnerMesh = CastChecked<ACharacter>(GetOwner())->GetMesh();

    if (UClass* AnimClassPtr = AnimClass.Get())
    {
        OwnerMesh->SetAnimInstanceClass(AnimClassPtr);
    }
    OwnerMesh->SetMaterial(0, Material.Get());
}

void UMaViSimpleMeshMergeComponent::RefreshMesh()
{
    auto& Merger = UMaViSimpleMeshMergeWorldSubsystem::Get(this);
    UMaViSimpleMeshMergeWorldSubsystem::AbortRequest(RequestHandle);
    FMaViSimpleMeshMergeRequest Request;
#if !UE_BUILD_SHIPPING
    Request.MeshesToMerge = MeshesToMerge;
    {
        const int32 Num = Request.MeshesToMerge.Num();
        Request.MeshesToMerge.RemoveAll([](const TSoftObjectPtr<USkeletalMesh>& Mesh) { return Mesh.IsNull(); });
        UE_CLOG(GetWorld()->IsGameWorld() && Num != Request.MeshesToMerge.Num(), Log_MaViMeshMerge, Error, TEXT("Detected Empty Meshes on %s"), *GetOwner()->GetName());
    }
#else
    Request.MeshesToMerge = MeshesToMerge;
#endif
    Merger.RequestMeshMerge(MoveTemp(Request), FMaViSimpleMergedMeshReadySignature::CreateUObject(this, &UMaViSimpleMeshMergeComponent::OnMeshReady), RequestHandle);

    TArray<FSoftObjectPath> PathsToLoad;
    if (!Material.IsNull())
        PathsToLoad.Emplace(Material.ToSoftObjectPath());
    if (!AnimClass.IsNull())
        PathsToLoad.Emplace(AnimClass.ToSoftObjectPath());
    if (!PathsToLoad.IsEmpty())
    {
        StreamingHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(MoveTemp(PathsToLoad), FStreamableDelegate::CreateUObject(this, &UMaViSimpleMeshMergeComponent::OnStreamingCompleted));
        check(StreamingHandle);
    }
}
