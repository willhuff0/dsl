[[vk::binding(0, 1)]] Texture2D<float4> u_equirectangularMap;
[[vk::binding(1, 1)]] SamplerState u_sampler;

struct VertexOutput {
    float4 position : SV_Position;
    float3 localPosition : LOCATION0;
};

static const float PI = 3.14159265359;

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float3 position = normalize(input.localPosition);

    float2 uv = float2(atan2(position.x, position.z), asin(position.y));
    uv *= float2(1.0 / (2.0 * PI), 1.0 / PI);
    uv += 0.5;

    float3 color = u_equirectangularMap.Sample(u_sampler, uv).rgb;

    // Tone mapping - Reinhard
    // color = color / (color + float3(1.0));

    // Tone mapping - ACES
    // color = 1.0 - exp(-color * 1.0);

    color = min(color, float3(100.0));

    return float4(color, 1.0);
}