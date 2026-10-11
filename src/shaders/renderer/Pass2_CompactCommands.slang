#include "../Renderer.slang"

[[vk::binding(0, 0)]] StructuredBuffer<MeshMetadata> MeshMetadatas;

[[vk::binding(1, 1)]] StructuredBuffer<uint32_t> MeshInstanceIndexOffsets;
[[vk::binding(2, 1)]] StructuredBuffer<uint32_t> MeshInstanceCounts;
[[vk::binding(4, 1)]] StructuredBuffer<uint32_t> ShaderCommandIndexOffsets;
[[vk::binding(5, 1)]] RWStructuredBuffer<Atomic<uint32_t>> DrawCounts;
[[vk::binding(6, 1)]] RWStructuredBuffer<IndirectCommand> CompactedCommands;

[[vk::push_constant]] uint32_t NumMeshes;

[shader("compute")]
[numthreads(64, 1, 1)]
void CompactCommands(uint32_t3 threadId: SV_DispatchThreadID) {
    uint32_t meshIndex = threadId.x;
    if (meshIndex >= NumMeshes) return;

    uint32_t meshInstanceCount = MeshInstanceCounts[meshIndex];
    if (meshInstanceCount == 0) return;

    MeshMetadata meshMetadata = MeshMetadatas[meshIndex];
    uint32_t meshInstanceIndexOffset = MeshInstanceIndexOffsets[meshIndex];

    uint32_t perShaderLocalDrawIndex = DrawCounts[meshMetadata.shaderIndex]++;

    uint32_t commandIndexOffset = ShaderCommandIndexOffsets[meshMetadata.shaderIndex];
    uint32_t compactedIndex = commandIndexOffset + perShaderLocalDrawIndex;

    CompactedCommands[compactedIndex].indexCount = meshMetadata.indexCount;
    CompactedCommands[compactedIndex].instanceCount = meshInstanceCount;
    CompactedCommands[compactedIndex].firstIndex = meshMetadata.firstIndex;
    CompactedCommands[compactedIndex].vertexOffset = meshMetadata.vertexOffset;
    CompactedCommands[compactedIndex].firstInstance = meshInstanceIndexOffset;
}