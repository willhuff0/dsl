[[vk::binding(0, 0)]]
cbuffer PostProcessingUniforms
{
    float bloomThreshold;
    float exposure;
    uint toneMappingMode;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
};

[[vk::binding(1, 0)]]
SamplerState u_sampler;

[[vk::binding(2, 0)]]
Texture2D<float4> u_texture;

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target
{
    float3 color = u_texture.Sample(u_sampler, input.texCoords).rgb;
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));

    return (luminance > bloomThreshold) ? float4(color, 1.0) : float4(0.0);
}
