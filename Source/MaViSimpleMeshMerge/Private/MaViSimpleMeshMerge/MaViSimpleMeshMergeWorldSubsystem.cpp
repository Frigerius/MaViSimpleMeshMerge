#include "MaViSimpleMeshMerge/MaViSimpleMeshMergeWorldSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "MaViSimpleMeshMerge/MaViSimpleMeshMergeRequest.h"
#include "MaViSimpleMeshMerge/MaViTimeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaViSimpleMeshMergeWorldSubsystem)

struct FStreamableHandle;

struct FMaViSimpleMeshMergeRequestImpl final
{
    FMaViSimpleMeshMergeRequestImpl() = default;
    FMaViSimpleMeshMergeRequestImpl(const FMaViSimpleMeshMergeRequestImpl&) = delete;
    FMaViSimpleMeshMergeRequestImpl(FMaViSimpleMeshMergeRequestImpl&&) = delete;
    FMaViSimpleMeshMergeRequestImpl& operator=(const FMaViSimpleMeshMergeRequestImpl&) = delete;
    FMaViSimpleMeshMergeRequestImpl& operator=(FMaViSimpleMeshMergeRequestImpl&&) = delete;
    ~FMaViSimpleMeshMergeRequestImpl();

    void AssignLoadedMeshes(TArray<USkeletalMesh*>&& InMeshes);
    const TArray<USkeletalMesh*>& GetMeshes() const { return LoadedMeshes; }

    std::atomic<bool> bIsAborted{ false };

    FMaViSimpleMeshMergeRequest Request;
    uint32 RequestHash{ 0 };
    FMaViSimpleMergedMeshReadySignature Callback;

private:
    TArray<USkeletalMesh*> LoadedMeshes;

public:
    UE_MT_DECLARE_RW_ACCESS_DETECTOR(StreamableHandleMTDetector);
    TSharedPtr<FStreamableHandle> StreamableHandle;
};

struct FMaViSimpleMeshMergeCache
{
    FMaViSimpleMeshMergeRequest Request;
    TWeakObjectPtr<USkeletalMesh> Mesh;
};

FMaViSimpleMeshMergeRequestImpl::~FMaViSimpleMeshMergeRequestImpl()
{
    for (USkeletalMesh* Mesh : LoadedMeshes)
    {
        Mesh->ReleaseRef();
    }
}

void FMaViSimpleMeshMergeRequestImpl::AssignLoadedMeshes(TArray<USkeletalMesh*>&& InMeshes)
{
    check(LoadedMeshes.IsEmpty())
    LoadedMeshes = MoveTemp(InMeshes);
    for (USkeletalMesh* Mesh : LoadedMeshes)
    {
        Mesh->AddRef();
    }
}

UMaViSimpleMeshMergeWorldSubsystem::UMaViSimpleMeshMergeWorldSubsystem() = default;

UMaViSimpleMeshMergeWorldSubsystem::~UMaViSimpleMeshMergeWorldSubsystem() = default;

UMaViSimpleMeshMergeWorldSubsystem& UMaViSimpleMeshMergeWorldSubsystem::Get(const UObject* WorldContext)
{
    auto* Instance = WorldContext->GetWorld()->GetSubsystem<UMaViSimpleMeshMergeWorldSubsystem>();
    check(Instance);
    return *Instance;
}

void UMaViSimpleMeshMergeWorldSubsystem::Deinitialize()
{
    Super::Deinitialize();
    Requests.Empty();
    PreparedRequests.Empty();
    MergedMeshCache.Empty();
}

void UMaViSimpleMeshMergeWorldSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TRACE_CPUPROFILER_EVENT_SCOPE(UMaViSimpleMeshMergeWorldSubsystem::Tick);
    PrepareRequests();
    ProcessRequests();
    RunHousekeeping(DeltaTime);
}

TStatId UMaViSimpleMeshMergeWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMaViSimpleMeshMergeWorldSubsystem, STATGROUP_Tickables);
}

ETickableTickType UMaViSimpleMeshMergeWorldSubsystem::GetTickableTickType() const
{
    if (Super::GetTickableTickType() == ETickableTickType::Never)
        return ETickableTickType::Never;
    return ETickableTickType::Always;
}

EYggMeshMergeRequestResult UMaViSimpleMeshMergeWorldSubsystem::RequestMeshMerge(FMaViSimpleMeshMergeRequest&& Request, FMaViSimpleMergedMeshReadySignature&& Callback, FMaViSimpleMeshMergeRequestHandle& OutHandle)
{
    check(IsInitialized());
    check(Callback.IsBound());
    const uint32 RequestHash = GetTypeHash(Request);
    if (USkeletalMesh* Mesh = FindCachedMesh(RequestHash, Request))
    {
        Callback.Execute(Mesh);
        return EYggMeshMergeRequestResult::UsingCached;
    }

    if (Request.MeshesToMerge.IsEmpty())
        return EYggMeshMergeRequestResult::Failed;
    const auto RequestImpl = MakeShared<FMaViSimpleMeshMergeRequestImpl>();
    RequestImpl->Request = MoveTemp(Request);
    RequestImpl->Callback = MoveTemp(Callback);
    RequestImpl->RequestHash = RequestHash;
    Requests.Enqueue(RequestImpl);
    OutHandle = FMaViSimpleMeshMergeRequestHandle{ RequestImpl };
    return EYggMeshMergeRequestResult::Requested;
}

void UMaViSimpleMeshMergeWorldSubsystem::AbortRequest(const FMaViSimpleMeshMergeRequestHandle& Handle)
{
    if (const auto RequestImpl = Handle.WeakRequest.Pin())
    {
        RequestImpl->bIsAborted = true;
        RequestImpl->Callback.Unbind();
        {
            UE_MT_SCOPED_READ_ACCESS(RequestImpl->StreamableHandleMTDetector);
            if (RequestImpl->StreamableHandle)
            {
                RequestImpl->StreamableHandle->CancelHandle();
            }
        }
    }
}

bool UMaViSimpleMeshMergeWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    switch (WorldType)
    {
        case EWorldType::None:
            return false;
        case EWorldType::Game:
            return true;
        case EWorldType::Editor:
            return true;
        case EWorldType::PIE:
            return true;
        case EWorldType::EditorPreview:
            return true;
        case EWorldType::GamePreview:
            return true;
        case EWorldType::GameRPC:
            return false;
        case EWorldType::Inactive:
            return false;
        default:
            return false;
    }
}

USkeletalMesh* UMaViSimpleMeshMergeWorldSubsystem::FindCachedMesh(uint32 Hash, const FMaViSimpleMeshMergeRequest& Request)
{
    if (const TUniquePtr<const FMaViSimpleMeshMergeCache>* CachePtr = MergedMeshCache.Find(Hash))
    {
        const FMaViSimpleMeshMergeCache& Cache = *CachePtr->Get();
        check(Cache.Request == Request);
        if (USkeletalMesh* Mesh = Cache.Mesh.Get())
        {
            return Mesh;
        }
        MergedMeshCache.Remove(Hash);
    }
    return nullptr;
}

void UMaViSimpleMeshMergeWorldSubsystem::PrepareRequests()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(UMaViSimpleMeshMergeWorldSubsystem::PrepareRequests);
    TSharedPtr<FMaViSimpleMeshMergeRequestImpl> RequestImpl;
    while (Requests.Dequeue(RequestImpl))
    {
        if (RequestImpl->bIsAborted)
            continue;
        if (USkeletalMesh* Mesh = FindCachedMesh(RequestImpl->RequestHash, RequestImpl->Request))
        {
            RequestImpl->Callback.ExecuteIfBound(Mesh);
            continue;
        }
        auto Delegate = FStreamableDelegate::CreateUObject(this, &ThisClass::OnResourcesLoaded, RequestImpl);
        TArray<FSoftObjectPath> AssetsToLoad;
        Algo::Transform(RequestImpl->Request.MeshesToMerge, AssetsToLoad, [](const TSoftObjectPtr<USkeletalMesh>& SoftMesh) { return SoftMesh.ToSoftObjectPath(); });
        auto Handle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(MoveTemp(AssetsToLoad), MoveTemp(Delegate));
        check(Handle);
        {
            UE_MT_SCOPED_WRITE_ACCESS(RequestImpl->StreamableHandleMTDetector);
            RequestImpl->StreamableHandle = MoveTemp(Handle);
        }
    }
}

void UMaViSimpleMeshMergeWorldSubsystem::ProcessRequests()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(UMaViSimpleMeshMergeWorldSubsystem::ProcessRequests);
    const double CurrentTime = FPlatformTime::Seconds();
    constexpr double TimeThreshold = 100_MicroSeconds;
    TSharedPtr<FMaViSimpleMeshMergeRequestImpl> RequestImpl;
    while (((FPlatformTime::Seconds() - CurrentTime) <= TimeThreshold)
        && PreparedRequests.Dequeue(RequestImpl))
    {
        if (RequestImpl->bIsAborted)
            continue;

        if (USkeletalMesh* Mesh = FindCachedMesh(RequestImpl->RequestHash, RequestImpl->Request))
        {
            RequestImpl->Callback.ExecuteIfBound(Mesh);
            continue;
        }
        ProcessRequest(*RequestImpl);
    }
}

void UMaViSimpleMeshMergeWorldSubsystem::ProcessRequest(FMaViSimpleMeshMergeRequestImpl& Request)
{
    USkeleton* Skeleton = Request.GetMeshes()[0]->GetSkeleton();
#if DO_ENSURE
    for (USkeletalMesh* Mesh : Request.GetMeshes())
    {
        if (!ensureAlways(Mesh->GetSkeleton() == Skeleton))
        {
            // Fail instead of clean as we don't know if we have other arrays with mapping data which would break.
            Request.Callback.ExecuteIfBound(nullptr);
            return;
        }
    }
#endif
    const bool bNeedsToMerge = Request.GetMeshes().Num() > 1 || (!Request.Request.MeshSectionMappings.IsEmpty() || !Request.Request.UVTransformsPerMesh.UVTransformsPerMesh.IsEmpty());

    USkeletalMesh* Result = nullptr;
    if (bNeedsToMerge)
    {
        constexpr EMeshBufferAccess BufferAccess = EMeshBufferAccess::ForceCPUAndGPU;
        constexpr int32 StripTopLODs{ 0 };
        Result = NewObject<USkeletalMesh>();
        Result->SetSkeleton(Skeleton);
        FSkelMeshMergeUVTransformMapping& UVTransformsPerMesh = Request.Request.UVTransformsPerMesh;
        FSkeletalMeshMerge Merger{
            Result,
            Request.GetMeshes(),
            Request.Request.MeshSectionMappings,
            StripTopLODs,
            BufferAccess,
            UVTransformsPerMesh.UVTransformsPerMesh.Num() > 0 ? &UVTransformsPerMesh : nullptr
        };
        if (!Merger.DoMerge())
        {
            UE_LOG(Log_MaViMeshMerge, Warning, TEXT("Merge failed!"));
            Request.Callback.ExecuteIfBound(nullptr);
            return;
        }
        // This is needed because apparently the call during the merge process doesn't work properly
        Result->RebuildSocketMap();
    }
    else
    {
        Result = Request.GetMeshes()[0];
    }
    MergedMeshCache.Emplace(Request.RequestHash, MakeUnique<FMaViSimpleMeshMergeCache>(MoveTemp(Request.Request), Result));
    Request.Callback.ExecuteIfBound(Result);
}

void UMaViSimpleMeshMergeWorldSubsystem::OnResourcesLoaded(TSharedPtr<FMaViSimpleMeshMergeRequestImpl> RequestImpl)
{
    if (RequestImpl->bIsAborted)
        return;
    TArray<USkeletalMesh*> Meshes;
    Meshes.Reserve(RequestImpl->Request.MeshesToMerge.Num());
    for (const auto& SoftMesh : RequestImpl->Request.MeshesToMerge)
    {
        USkeletalMesh* Mesh = SoftMesh.Get();
        check(Mesh);
        Meshes.Emplace(Mesh);
    }
    RequestImpl->AssignLoadedMeshes(MoveTemp(Meshes));
    PreparedRequests.Enqueue(MoveTemp(RequestImpl));
}

void UMaViSimpleMeshMergeWorldSubsystem::RunHousekeeping(float DeltaTime)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(UMaViSimpleMeshMergeWorldSubsystem::RunHousekeeping);
    const float TmpTimeSinceLastHousekeeping = TimeSinceLastHousekeeping + DeltaTime;
    constexpr float HousekeepingInterval{ 60.f };
    if (TmpTimeSinceLastHousekeeping < HousekeepingInterval)
    {
        TimeSinceLastHousekeeping = TmpTimeSinceLastHousekeeping;
        return;
    }
    TimeSinceLastHousekeeping = 0.f;
    for (auto Iter = MergedMeshCache.CreateIterator(); Iter; ++Iter)
    {
        auto& [Request, WeakMesh] = *Iter;
        if (!WeakMesh.IsValid())
            Iter.RemoveCurrent();
    }
}
