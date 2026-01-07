[[vk::binding(0, 1)]] TextureCube<float4> u_cubeMap;
[[vk::binding(1, 1)]] SamplerState u_sampler;

struct VertexOutput {
    float4 position : SV_Position;
    float3 localPosition : LOCATION0;
};

static const float PI = 3.14159265359;

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float3 normal = normalize(input.localPosition);

    float3 irradiance = float3(0.0);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            float3 sampleVal = u_cubeMap.Sample(u_sampler, sampleVec * float3(-1, 1, 1)).rgb * cos(theta) * sin(theta);
            irradiance += sampleVal;

            nrSamples += 1.0;
        }
    }
    irradiance = PI * irradiance * (1.0 / nrSamples);

    return float4(irradiance, 1.0);
}