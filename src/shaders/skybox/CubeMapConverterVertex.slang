struct CubeMapCaptureUniforms {
    float4x4 viewProjMatrix;
};
[[vk::binding(0, 0)]] ConstantBuffer<CubeMapCaptureUniforms> u_capture;

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
    output.position = mul(u_capture.viewProjMatrix, float4(input.position, 1.0));
    output.localPosition = input.position;
    return output;
}