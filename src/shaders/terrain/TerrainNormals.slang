#include "../Globals.slang"

[[vk::binding(0, 0)]] ConstantBuffer<GlobalUniforms> u_global;

[[vk::binding(0, 1)]] Texture2D<float> u_heightMap;
[[vk::binding(1, 1)]] Texture2D<float4> u_normalMap;

[[vk::constant_id(0)]] const uint HEIGHTMAP_SIZE = 4096;
[[vk::constant_id(1)]] const float METERS_PER_TEXEL = 1.0;

struct VertexInput {
    float2 positionXZ;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color;
};

struct LodConstants {
    uint lod;
    float range;
};
[vk::push_constant] ConstantBuffer<LodConstants> u_lod;

[shader("vertex")]
VertexOutput vs_main(VertexInput input, uint vertexID : SV_VertexID) {
    bool isOuterEdge = max(abs(input.positionXZ.x), abs(input.positionXZ.y)) > (u_lod.range - 0.5);
    float snapLod = float(u_lod.lod);
    if (isOuterEdge) {
        snapLod += 1.0;
    }

    float gridSize = METERS_PER_TEXEL * exp2(snapLod);
    float2 snappedViewPos = floor(u_global.viewPos.xz / gridSize) * gridSize;

    float2 worldXZ = snappedViewPos + input.positionXZ;
    int2 worldTexelXZ = int2(floor(worldXZ / METERS_PER_TEXEL + float2(HEIGHTMAP_SIZE / 2.0)));
    int2 wrapped = int2(
        (worldTexelXZ.x % HEIGHTMAP_SIZE + HEIGHTMAP_SIZE) % HEIGHTMAP_SIZE,
        (worldTexelXZ.y % HEIGHTMAP_SIZE + HEIGHTMAP_SIZE) % HEIGHTMAP_SIZE
    );
    float height = u_heightMap.Load(int3(wrapped, 0));
    float3 normal = u_normalMap.Load(int3(wrapped, 0)).xyz;

    float3 pos = float3(worldXZ.x, height, worldXZ.y);

    const float lineLength = 1.0;
    bool isTip = (vertexID & 1) != 0;

    float3 vertexPos = pos + (isTip ? normal * lineLength : 0.0);

    VertexOutput output;
    output.position = mul(u_global.viewProjMat, float4(vertexPos, 1.0));
    output.color =
       isTip
            ? float3(1.0, 0.0, 0.0)
            : float3(0.0, 1.0, 0.0);
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    return float4(input.color, 1.0);
}