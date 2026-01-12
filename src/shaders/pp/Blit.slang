struct VertexOutput {
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
};

[[vk::constant_id(0)]] const uint c_extent;

[shader("vertex")]
VertexOutput vs_main(uint vertexIndex : SV_VertexID) {
    static const float2 positions[] = {
        // 1st triangle
        float2(0.0,  0.0),  // center
        float2(1.0,  0.0),  // right, center
        float2(0.0,  1.0),  // center, top

        // 2nd triangle
        float2(0.0,  1.0),  // center, top
        float2(1.0,  0.0),  // right, center
        float2(1.0,  1.0),  // right, top
    };

    VertexOutput output;
    float2 xy = positions[vertexIndex];
    output.position = float4(xy * 2.0 - 1.0, 0.0, 1.0);
    output.texCoords = float2(xy.x, 1.0 - xy.y);
    return output;
}

[[vk::binding(1, 0)]] SamplerState u_sampler;
[[vk::binding(0, 1)]] Texture2D<float4> u_texture;

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    return float4(u_texture.Sample(u_sampler, input.texCoords).rgb, 1.0);
}