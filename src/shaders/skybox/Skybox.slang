#include "../Camera.slang"
#include "../Environment.slang"

[[vk::binding(0, 0)]] ConstantBuffer<CameraUniforms> u_camera;

[[vk::binding(1, 1)]] SamplerState u_environmentSampler;
[[vk::binding(2, 1)]] TextureCube<float4> u_environmentMap;

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
    float4 viewPos = mul(u_camera.viewMat, float4(input.position, 0.0));
    float4 clipPos = mul(u_camera.projMat, viewPos);

    // Forces skybox to far plane (z = w after perspective divide)
    output.position = clipPos.xyww;
    output.localPosition = input.position;
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float3 color = u_environmentMap.Sample(u_environmentSampler, input.localPosition).rgb;
    return float4(color, 1.0);
}