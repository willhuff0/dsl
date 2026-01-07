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
[[vk::binding(2, 0)]] Texture2D<float4> u_texture;

[[vk::constant_id(0)]] const bool c_horizontal;

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target
{
    const float weight[] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

    uint width, height;
    u_texture.GetDimensions(width, height);
    float2 texOffset = 1.0 / float2(width, height);

    float3 result = u_texture.Sample(u_sampler, input.texCoords).rgb * weight[0];

    if (c_horizontal) {
        for (int i = 1; i < 5; i++) {
            result += u_texture.Sample(u_sampler, input.texCoords + float2(texOffset.x * float(i), 0.0)).rgb * weight[i];
            result += u_texture.Sample(u_sampler, input.texCoords - float2(texOffset.x * float(i), 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; i++) {
            result += u_texture.Sample(u_sampler, input.texCoords + float2(0.0, texOffset.y * float(i))).rgb * weight[i];
            result += u_texture.Sample(u_sampler, input.texCoords - float2(0.0, texOffset.y * float(i))).rgb * weight[i];
        }
    }

    return float4(result, 1.0);
}