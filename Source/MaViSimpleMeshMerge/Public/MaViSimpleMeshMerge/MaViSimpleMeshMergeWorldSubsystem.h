#pragma once

#include "CoreMinimal.h"

#include "Containers/Queue.h"

#include "Subsystems/WorldSubsystem.h"

#include "MaViSimpleMeshMergeWorldSubsystem.generated.h"

struct FMaViSimpleMeshMergeRequest;
struct FMaViSimpleMeshMergeRequestImpl;
struct FMaViSimpleMeshMergeRequestHandle;
class USkeletalMesh;

using FMaViSimpleMergedMeshReadySignature = TDelegate<void(USkeletalMesh*)>;
DECLARE_DYNAMIC_DELEGATE_OneParam(FYggMergedMeshReadyDelegate, USkeletalMesh*, Mesh);

struct FMaViSimpleMeshMergeCache;

UENUM(BlueprintType)
enum class EYggMeshMergeRequestResult : uint8
{
    Requested,
    Failed,
    UsingCached,
};

UCLASS()
class UMaViSimpleMeshMergeWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    UMaViSimpleMeshMergeWorldSubsystem();
    ~UMaViSimpleMeshMergeWorldSubsystem() override;

    static UMaViSimpleMeshMergeWorldSubsystem& Get(const UObject* WorldContext);

    void Deinitialize() override;
    void Tick(float DeltaTime) override;
    TStatId GetStatId() const override;
    ETickableTickType GetTickableTickType() const override;
    bool IsTickableInEditor() const override { return true; }
    bool IsTickableWhenPaused() const override { return true; }

    EYggMeshMergeRequestResult RequestMeshMerge(FMaViSimpleMeshMergeRequest&& Request, FMaViSimpleMergedMeshReadySignature&& Callback, FMaViSimpleMeshMergeRequestHandle& OutHandle);
    static void AbortRequest(const FMaViSimpleMeshMergeRequestHandle& Handle);

protected:
    bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    USkeletalMesh* FindCachedMesh(uint32 Hash, const FMaViSimpleMeshMergeRequest& Request);
    void PrepareRequests();
    void ProcessRequests();
    void OnResourcesLoaded(TSharedPtr<FMaViSimpleMeshMergeRequestImpl> RequestImpl);
    void ProcessRequest(FMaViSimpleMeshMergeRequestImpl& Request);
    void RunHousekeeping(float DeltaTime);

    TQueue<TSharedPtr<FMaViSimpleMeshMergeRequestImpl>, EQueueMode::Mpsc> Requests;
    TQueue<TSharedPtr<FMaViSimpleMeshMergeRequestImpl>, EQueueMode::Mpsc> PreparedRequests;

    TMap<uint32, TUniquePtr<const FMaViSimpleMeshMergeCache>> MergedMeshCache;
    float TimeSinceLastHousekeeping{ 0.f };
};
