struct PostProcessingUniforms
{
    float bloomThreshold;
    float exposure;
    uint toneMappingMode;
};

[[vk::binding(0, 0)]] ConstantBuffer<PostProcessingUniforms> u_postProcessing;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
};

[[vk::binding(1, 0)]] SamplerState u_sampler;
[[vk::binding(2, 0)]] Texture2D<float4> u_baseTexture;
// [[vk::binding(2, 1)]] Texture2D<float4> u_bloomTexture;

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target
{
    float3 baseColor = u_baseTexture.Sample(u_sampler, input.texCoords).rgb;
    //float3 bloomColor = u_bloomTexture.Sample(u_sampler, input.texCoords).rgb;

    float3 color = baseColor;// + bloomColor;

    if (u_postProcessing.toneMappingMode == 1) { color = color / (color + float3(1.0)); }
    else if (u_postProcessing.toneMappingMode == 2) { color = 1.0 - exp(-color * u_postProcessing.exposure); }

    color = pow(color, float3(1.0 / 2.2));

    return float4(color, 1.0);
}