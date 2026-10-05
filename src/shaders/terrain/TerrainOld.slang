#include "../Globals.slang"

[[vk::binding(0, 0)]] ConstantBuffer<GlobalUniforms> u_global;

[[vk::binding(1, 0)]] SamplerState u_environmentSampler;
[[vk::binding(2, 0)]] TextureCube<float4> u_irradianceMap;
[[vk::binding(3, 0)]] TextureCube<float4> u_radianceMap;
[[vk::binding(4, 0)]] Texture2D<float4> u_brdfLut;

struct TerrainUniforms {
    float tiling;
    float ambientIntensity;
};
[[vk::binding(0, 1)]] ConstantBuffer<TerrainUniforms> terrainUniforms;

[[vk::binding(1, 1)]] Texture2D<float4> u_albedoTexture;
[[vk::binding(2, 1)]] Texture2D<float4> u_metallicTexture;
[[vk::binding(3, 1)]] Texture2D<float4> u_roughnessTexture;
[[vk::binding(4, 1)]] Texture2D<float4> u_aoTexture;
[[vk::binding(5, 1)]] SamplerState u_sampler;

struct ChunkUniforms {
    float4x4 modelMatrix;
};
[[vk::binding(0, 2)]] ConstantBuffer<ChunkUniforms> chunkUniforms;

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 worldPosition : LOCATION0;
    float3 normal : LOCATION1;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    VertexOutput output;
    output.position = mul(u_global.viewProjMatrix, mul(chunkUniforms.modelMatrix, float4(input.position, 1.0)));
    output.worldPosition = mul(chunkUniforms.modelMatrix, float4(input.position, 1.0));
    output.normal = input.normal;
    return output;
}

static const float PI = 3.14159265359;

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float2 texCoord = float2(input.worldPosition.x, input.worldPosition.z) * terrainUniforms.tiling;
    float3 albedo = u_albedoTexture.Sample(u_sampler, texCoord).rgb;
    float metallic = u_metallicTexture.Sample(u_sampler, texCoord).r;
    float roughness = u_roughnessTexture.Sample(u_sampler, texCoord).r;
    float ao = u_aoTexture.Sample(u_sampler, texCoord).r;

    float3 F0 = float3(0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 N = normalize(input.normal);
    float3 V = normalize(u_global.viewPosition.xyz - input.worldPosition.xyz);
    float3 R = reflect(-V, N);

    float3 Lo = float3(0.0);

// Directional Light
    {
        float3 L = normalize(float3(0.5, 1.0, 0.5));
        float3 H = normalize(V + L);

        float3 radiance = float3(1.0, 0.95, 0.9) * 1.5;

        float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float3 ambient = float3(0.0);

// Environment
    {
        float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

        float3 kS = F;
        float3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        float3 irradiance = u_irradianceMap.Sample(u_environmentSampler, N).rgb;
        float3 diffuse = irradiance * albedo;

        float3 prefilteredColor = u_radianceMap.SampleLevel(u_environmentSampler, R, roughness * float(u_global.radianceMipMapCount)).rgb;
        float2 envBrdf = u_brdfLut.Sample(u_environmentSampler, float2(max(dot(N, V), 0.0), roughness)).rg;
        float3 specular = prefilteredColor * (F * envBrdf.x + envBrdf.y);

        ambient = (kD * diffuse + specular) * ao * terrainUniforms.ambientIntensity;
    }

    float3 color = ambient + Lo;

    return float4(color, 1.0);
}