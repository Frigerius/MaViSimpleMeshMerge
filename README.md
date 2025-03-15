This Project supports a very simplified way to merge SkeletalMeshs belonging to the same USkeleton.

It also serves as example project on how to merge SkeletaLMeshs at runtime using Unreals Skeletal Mesh merger.

This Plugin is currently limited to use the component within an ACharacter, as we rely on getting the MeshComponent. How ever, this can be changed if needed by providing an Interface or injecting a Component.

The MeshMerger will reuse same configurations so it will only Merge a single configuration once and will re use it for other requests.

To use the Merger you simply have to hadd the MaViSimpleMeshMergeComponent to a Character and assign the Meshs you want to have merged.
