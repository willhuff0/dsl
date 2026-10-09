struct Instance {
    uint32_t materialIndex;
    uint32_t meshIndex;

    bool isDynamic;

    uint32_t frameDataIndex;
};

struct InstanceFrameData {
    float4x4 modelMat;
};

struct MeshMetadata {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t shaderIndex;

    float3 aabbMin;
    float3 aabbMax;
};

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