#include "MaViSimpleMeshMerge/MaViSimpleMeshMergeRequest.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaViSimpleMeshMergeRequest)

uint32 GetTypeHash(const FMaViSimpleMeshMergeRequest& InObject)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FYggMeshMergeRequest::GetTypeHash)
    uint32 Hash = 0;
    for (const auto& SoftMesh : InObject.MeshesToMerge)
    {
        Hash = HashCombine(Hash, GetTypeHash(SoftMesh));
    }
    for (const auto& Mapping : InObject.MeshSectionMappings)
    {
        for (int32 SectionID : Mapping.SectionIDs)
        {
            Hash = HashCombine(Hash, GetTypeHash(SectionID));
        }
    }
    for (const auto& UVTransforms : InObject.UVTransformsPerMesh.UVTransformsPerMesh)
    {
        for (const FTransform& Transform : UVTransforms.UVTransforms)
        {
            Hash = HashCombine(Hash, GetTypeHash(Transform));
        }
    }
    return Hash;
}

bool FMaViSimpleMeshMergeRequest::operator==(const FMaViSimpleMeshMergeRequest& Other) const
{
    if (MeshesToMerge != Other.MeshesToMerge)
        return false;
    {
        const int32 NumMappings = MeshSectionMappings.Num();
        if (NumMappings != Other.MeshSectionMappings.Num())
            return false;
        for (int32 MappingIndex{ 0 }; MappingIndex < NumMappings; ++MappingIndex)
        {
            if (MeshSectionMappings[MappingIndex].SectionIDs != Other.MeshSectionMappings[MappingIndex].SectionIDs)
                return false;
        }
    }
    {
        const int32 NumUVTransforms = UVTransformsPerMesh.UVTransformsPerMesh.Num();
        if (NumUVTransforms != Other.UVTransformsPerMesh.UVTransformsPerMesh.Num())
            return false;
        for (int32 Index{ 0 }; Index < NumUVTransforms; ++Index)
        {
            const int32 NumTransforms = UVTransformsPerMesh.UVTransformsPerMesh[Index].UVTransforms.Num();
            const auto& UVTransformsPerMeshA = UVTransformsPerMesh.UVTransformsPerMesh[Index];
            const auto& UVTransformsPerMeshB = Other.UVTransformsPerMesh.UVTransformsPerMesh[Index];
            for (int32 TransformIndex{ 0 }; TransformIndex < NumTransforms; ++TransformIndex)
            {
                if (UVTransformsPerMeshA.UVTransforms[TransformIndex].Equals(UVTransformsPerMeshB.UVTransforms[TransformIndex]))
                    return false;
            }
        }
    }
    return true;
}
