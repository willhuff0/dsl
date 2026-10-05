#include "../Globals.slang"
#include "RendererStructs.slang"

[[vk::binding(1, 0)]] StructuredBuffer<Instance> Instances;

[[vk::binding(0, 1)]] StructuredBuffer<uint32_t> MeshInstanceIndexOffsets;
[[vk::binding(1, 1)]] RWStructuredBuffer<Atomic<uint32_t>> MeshInstanceCounts;
[[vk::binding(2, 1)]] RWStructuredBuffer<uint32_t> VisibleInstanceIndices;

[[vk::binding(0, 2)]] ConstantBuffer<GlobalUniforms> Globals;

[[vk::push_constant]] uint32_t NumInstances;

[shader("compute")]
[numthreads(64, 1, 1)]
void CullInstances(uint32_t3 threadId: SV_DispatchThreadID) {
    uint32_t instanceIndex = threadId.x;
    if (instanceIndex >= NumInstances) return;

    Instance instance = Instances[instanceIndex];

    // Cull here

    uint32_t perMeshLocalInstanceIndex = MeshInstanceCounts[instance.meshIndex]++;
    uint32_t meshInstanceIndexOffset = MeshInstanceIndexOffsets[instance.meshIndex];

    VisibleInstanceIndices[meshInstanceIndexOffset + perMeshLocalInstanceIndex] = instanceIndex;
}