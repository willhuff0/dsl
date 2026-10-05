struct Instance {
    float4x4 modelMatrix;

    float3 aabbMin;
    float3 aabbMax;

    uint32_t materialIndex;
    uint32_t meshIndex;
};

struct MeshMetadata {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;

    uint32_t shaderIndex;
}

struct IndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};

struct VertexInput {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv0;
    float2 uv1;
};