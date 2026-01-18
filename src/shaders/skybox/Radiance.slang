[[vk::binding(0, 1)]] TextureCube<float4> u_cubeMap;
[[vk::binding(1, 1)]] SamplerState u_sampler;

struct RadianceUniforms {
    float roughness;
};
[[vk::binding(2, 1)]] ConstantBuffer<RadianceUniforms> u_radiance;

struct VertexOutput {
    float4 position : SV_Position;
    float3 localPosition : LOCATION0;
};

static const float PI = 3.14159265359;

float RadicalInverseVdC(uint bits) {
    uint b = bits;

    b = (b << 16u) | (b >> 16u);
    b = ((b & 0x55555555u) << 1u)  | ((b & 0xAAAAAAAAu) >> 1u);
    b = ((b & 0x33333333u) << 2u)  | ((b & 0xCCCCCCCCu) >> 2u);
    b = ((b & 0x0F0F0F0Fu) << 4u)  | ((b & 0xF0F0F0F0u) >> 4u);
    b = ((b & 0x00FF00FFu) << 8u)  | ((b & 0xFF00FF00u) >> 8u);

    return float(b) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint N) {
    return float2(float(i)/float(N), RadicalInverseVdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    float3 up = (abs(N.z) < 0.999) ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);

    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float3 N = normalize(input.localPosition);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0.0);
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, u_radiance.roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefilteredColor += u_cubeMap.SampleLevel(u_sampler, L * float3(-1, 1, 1), 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    return float4(prefilteredColor, 1.0);
}