struct GlobalUniforms {
    float4x4 viewProjMatrix;
    float4 viewPosition;
    uint radianceMipMapCount;
};
[[vk::binding(0, 0)]] ConstantBuffer<GlobalUniforms> u_global;

[[vk::binding(0, 1)]] Texture2D<float> u_heightmap;

[[vk::constant_id(0)]] const uint HEIGHTMAP_SIZE;
[[vk::constant_id(1)]] const float METERS_PER_TEXEL;

struct VertexInput {
    float2 positionXZ;
};

struct VertexOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    float2 worldXZ = u_global.viewPosition.xz + input.positionXZ - float2(HEIGHTMAP_SIZE / 2.0, HEIGHTMAP_SIZE / 2.0);
    int2 worldTexelXZ = int2(floor(worldXZ / METERS_PER_TEXEL));
    int2 wrapped = int2(
        (worldTexelXZ.x % HEIGHTMAP_SIZE + HEIGHTMAP_SIZE) % HEIGHTMAP_SIZE,
        (worldTexelXZ.y % HEIGHTMAP_SIZE + HEIGHTMAP_SIZE) % HEIGHTMAP_SIZE
    );
    float height = u_heightmap.Load(int3(wrapped, 1));

    VertexOutput output;
    output.position = float4(worldXZ.x, height, worldXZ.y, 1.0);
    return output;
}
