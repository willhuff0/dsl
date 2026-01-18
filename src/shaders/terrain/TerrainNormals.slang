struct GlobalUniforms {
    float4x4 viewProjMatrix;
    float4 viewPosition;
    uint radianceMipMapCount;
};
[[vk::binding(0, 0)]] ConstantBuffer<GlobalUniforms> u_global;

[[vk::binding(0, 1)]] Texture2D<float> u_heightMap;
[[vk::binding(1, 1)]] Texture2D<float4> u_normalMap;

[[vk::constant_id(0)]] const uint HEIGHTMAP_SIZE = 4096;
[[vk::constant_id(1)]] const float METERS_PER_TEXEL = 1.0;

struct VertexInput {
    float2 positionXZ;
};

struct VertexOutput {
    float4 worldPosition;
    float3 normal;
};

struct GeometryOutput {
    float4 position : SV_Position;
    float3 color;
};

struct LodConstants {
    uint lod;
    float range;
};
[vk::push_constant] ConstantBuffer<LodConstants> u_lod;

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    bool isOuterEdge = max(abs(input.positionXZ.x), abs(input.positionXZ.y)) > (u_lod.range - 0.5);
    float snapLod = float(u_lod.lod);
    if (isOuterEdge) {
        snapLod += 1.0;
    }

    float gridSize = METERS_PER_TEXEL * exp2(snapLod);
    float2 snappedViewPos = floor(u_global.viewPosition.xz / gridSize) * gridSize;

    float2 worldXZ = snappedViewPos + input.positionXZ;
    int2 worldTexelXZ = int2(floor(worldXZ / METERS_PER_TEXEL + float2(HEIGHTMAP_SIZE / 2.0)));
    int2 wrapped = int2(
        (worldTexelXZ.x % HEIGHTMAP_SIZE + HEIGHTMAP_SIZE) % HEIGHTMAP_SIZE,
        (worldTexelXZ.y % HEIGHTMAP_SIZE + HEIGHTMAP_SIZE) % HEIGHTMAP_SIZE
    );
    float height = u_heightMap.Load(int3(wrapped, 0));
    float4 normal = u_normalMap.Load(int3(wrapped, 0));

    VertexOutput output;
    output.worldPosition = float4(worldXZ.x, height, worldXZ.y, 1.0);
    output.normal = normal.xyz;
    return output;
}

[shader("geometry")]
[maxvertexcount(2)]
void gs_main(point VertexOutput input[1], inout LineStream<GeometryOutput> outputStream) {
    float3 pos = input[0].worldPosition.xyz;
    float3 normal = input[0].normal; // Assuming normal is in appropriate space (e.g., World)
    float lineLength = 1.0;

    GeometryOutput v0;
    v0.position = mul(u_global.viewProjMatrix, float4(pos, 1.0));
    v0.color = float3(0.0, 1.0, 0.0); // Base color
    outputStream.Append(v0);

    GeometryOutput v1;
    v1.position = mul(u_global.viewProjMatrix, float4(pos + normal * lineLength, 1.0));
    v1.color = float3(1.0, 0.0, 0.0); // Tip color
    outputStream.Append(v1);

    outputStream.RestartStrip();
}


[shader("fragment")]
float4 fs_main(GeometryOutput input) : SV_Target {
    return float4(input.color, 1.0);
}