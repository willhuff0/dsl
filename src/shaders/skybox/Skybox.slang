struct SkyboxUniforms {
    float4x4 projMatrix;
    float4x4 viewMatrix;
};
[[vk::binding(0, 0)]] ConstantBuffer<SkyboxUniforms> u_skybox;

[[vk::binding(1, 0)]] TextureCube<float4> u_cubeMap;
[[vk::binding(2, 0)]] SamplerState u_sampler;

struct VertexInput {
    float3 position : POSITION;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 localPosition : LOCATION0;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    // Using w = 0.0 removes the translation component of the view matrix effectively
    float4 viewPos = mul(u_skybox.viewMatrix, float4(input.position, 0.0));
    float4 clipPos = mul(u_skybox.projMatrix, viewPos);

    // Forces skybox to far plane (z = w after perspective divide)
    output.position = clipPos.xyww;
    output.localPosition = input.position;
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float3 color = u_cubeMap.Sample(u_sampler, input.localPosition).rgb;
    return float4(color, 1.0);
}