#include "../Globals.slang"
#include "../renderer/RendererStructs.slang"
#include "../Fog.slang"

static const uint32_t NULL_INDEX = 0xFFFFFFFF;
static const float PI = 3.14159265359;

static const float3 DIRECTIONAL_LIGHT_DIRECTION = float3(0.5, 0.5, 0.5);
static const float3 DIRECTIONAL_LIGHT_RADIANCE = float3(1.0, 0.95, 0.9) * 1.5;

static const float AMBIENT_INTENSITY = 1.0;

[[vk::constant_id(1)]] const bool DOUBLE_SIDED = false;

[[vk::binding(1, 0)]] StructuredBuffer<Instance> Instances;
[[vk::binding(2, 0)]] StructuredBuffer<InstanceFrameData> StaticInstanceFrameDatas;

[[vk::binding(0, 1)]] StructuredBuffer<InstanceFrameData> DynamicInstanceFrameDatas;
[[vk::binding(3, 1)]] StructuredBuffer<uint32_t> VisibleInstanceIndices;

[[vk::binding(0, 2)]] ConstantBuffer<GlobalUniforms> Globals;
[[vk::binding(1, 2)]] SamplerState EnvironmentSampler;
[[vk::binding(2, 2)]] TextureCube<float4> EnvironmentMap;
[[vk::binding(3, 2)]] TextureCube<float4> IrradianceMap;
[[vk::binding(4, 2)]] TextureCube<float4> RadianceMap;
[[vk::binding(5, 2)]] Texture2D<float2> BrdfLut;

[[vk::binding(0, 3)]] Texture2D Textures[];
[[vk::binding(1, 3)]] SamplerState Samplers[];

struct VertexOutput {
    float4 projectedPosition : SV_Position;
    float3 position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv0;
    float2 uv1;
    nointerpolation uint32_t materialIndex;
};

[shader("vertex")]
VertexOutput VertexMain(
    VertexInput input,
    uint32_t instanceId: SV_VulkanInstanceID
) {
    uint32_t instanceIndex = VisibleInstanceIndices[instanceId];
    Instance instance = Instances[instanceIndex];
    InstanceFrameData instanceFrameData;
    if (instance.isDynamic) {
        instanceFrameData = DynamicInstanceFrameDatas[instance.frameDataIndex];
    } else {
        instanceFrameData = StaticInstanceFrameDatas[instance.frameDataIndex];
    }

    // Assume uniform scaling
    float3x3 vectorMatrix = float3x3(instanceFrameData.modelMat);

    float4 position = mul(instanceFrameData.modelMat, float4(input.position, 1.0));

    VertexOutput output;
    output.projectedPosition = mul(Globals.viewProjMat, position);
    output.position = position.xyz;
    output.normal = mul(vectorMatrix, input.normal);
    output.tangent = mul(vectorMatrix, input.tangent.xyz);
    output.bitangent = cross(output.normal, output.tangent) * input.tangent.w;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.materialIndex = instance.materialIndex;
    return output;
}

struct Material {
    float4 baseColorFactor;
    uint32_t baseColorTexture;
    uint32_t baseColorUvIndex;

    uint32_t metallicTexture;
    uint32_t metallicUvIndex;
    float metallicFactor;

    uint32_t roughnessTexture;
    uint32_t roughnessUvIndex;
    float roughnessFactor;

    uint32_t normalTexture;
    uint32_t normalUvIndex;
    float normalScale;

    uint32_t occlusionTexture;
    uint32_t occlusionUvIndex;
    float occlusionStrength;

    uint32_t emissiveTexture;
    uint32_t emissiveUvIndex;
    float3 emissiveFactor;

    float alphaCutoff;
};
[[vk::binding(0, 4)]] StructuredBuffer<Material> Materials;

float3 F_Schlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float D_GGX(float NdotH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float3 F_SchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

float2 getUV(VertexOutput input, int texCoordIndex) {
    if (texCoordIndex == 1) return input.uv1;
    return input.uv0;
}

float3x3 getTangentFrame(float3 normal, float3 position, float2 uv) {
    float3 dp1 = ddx(position);
    float3 dp2 = ddy(position);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 tangent = normalize(dp1 * duv2.y - dp2 * duv1.y);
    float3 bitangent = -normalize(cross(normal, tangent));

    tangent = normalize(tangent - normal * dot(normal, tangent));

    return float3x3(tangent, bitangent, normal);
}

[shader("fragment")]
float4 FragmentMain(
    VertexOutput input,
    bool isFrontFace: SV_IsFrontFace,
) : SV_Target {
    Material material = Materials[input.materialIndex];

    float4 baseColor = material.baseColorFactor;
    if (material.baseColorTexture != NULL_INDEX) {
        float4 baseColorTex = Textures[material.baseColorTexture].Sample(EnvironmentSampler, getUV(input, material.baseColorUvIndex));
        baseColor *= baseColorTex;
    }

    if (material.alphaCutoff > 1.5f) {
        // OPAQUE
        baseColor.a = 1.0;
    } else {
        // MASKED
        if (baseColor.a < material.alphaCutoff)
            discard;
        baseColor.a = 1.0;
    }

    float metallic = material.metallicFactor;
    if (material.metallicTexture != NULL_INDEX) {
        float4 metallicTex = Textures[material.metallicTexture].Sample(EnvironmentSampler, getUV(input, material.metallicUvIndex));
        metallic *= metallicTex.b;
    }

    float roughness = material.roughnessFactor;
    if (material.roughnessTexture != NULL_INDEX) {
        float4 roughnessTex = Textures[material.roughnessTexture].Sample(EnvironmentSampler, getUV(input, material.roughnessUvIndex));
        roughness *= roughnessTex.g;
    }

    float3 N = normalize(input.normal);
    if (DOUBLE_SIDED && !isFrontFace) N = -N;
    if (material.normalTexture != NULL_INDEX) {
        float2 normalUV = getUV(input, material.normalUvIndex);
        float3x3 tbn;
        if (length(input.tangent) > 0.001) {
            float3 t = normalize(input.tangent);
            float3 b = normalize(input.bitangent);
            if (DOUBLE_SIDED && !isFrontFace) {
                t = -t;
                b = -b;
            }
            tbn = float3x3(t, b, N);
        } else {
            tbn = getTangentFrame(N, input.position, normalUV);
        }

        float3 nTex = Textures[material.normalTexture].Sample(EnvironmentSampler, normalUV).rgb * 2.0 - 1.0;
        nTex = normalize(float3(nTex.xy * material.normalScale, nTex.z));
        N = normalize(mul(nTex, tbn));
    }

    float3 V = normalize(Globals.viewPos - input.position);
    float3 L = normalize(DIRECTIONAL_LIGHT_DIRECTION);
    float3 H = normalize(V + L);
    float3 radiance = DIRECTIONAL_LIGHT_RADIANCE;

    float NdotV = max(dot(N, V), 0.000001);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    float3 F0 = lerp(float3(0.04), baseColor.rgb, metallic);
    float NDF = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    float3 F = F_Schlick(HdotV, F0);

    float3 specular = (NDF * G * F) / (4.0 * NdotV * NdotL + 0.0001);
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    float3 diffuse = kD * baseColor.rgb / PI;
    float3 Lo = (diffuse + specular) * radiance * NdotL;

    float3 F_IBL = F_SchlickRoughness(NdotV, F0, roughness);
    float3 kS_IBL = F_IBL;
    float3 kD_IBL = (1.0 - kS_IBL) * (1.0 - metallic);

    float3 irradiance = IrradianceMap.Sample(EnvironmentSampler, N).rgb;
    float3 diffuseIBL = irradiance * baseColor.rgb;

    float maxReflectionLOD = float(Globals.radianceMipMapCount) - 1.0;
    float3 R = reflect(-V, N);
    float3 prefilteredColor = RadianceMap.SampleLevel(EnvironmentSampler, R, roughness * maxReflectionLOD).rgb;
    float2 envBRDF = BrdfLut.Sample(EnvironmentSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);
    // float3 specularIBL = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);

    float3 ambient = (kD_IBL * diffuseIBL + specularIBL) * AMBIENT_INTENSITY;

    float ao = 1.0;
    if (material.occlusionTexture != NULL_INDEX) {
        ao = Textures[material.occlusionTexture].Sample(EnvironmentSampler, getUV(input, material.occlusionUvIndex)).r;
        ao = 1.0 + material.occlusionStrength * (ao - 1.0);
    }

    float3 emissive = material.emissiveFactor;
    if (material.emissiveTexture != NULL_INDEX) {
        float3 eTex = Textures[material.emissiveTexture].Sample(EnvironmentSampler, getUV(input, material.emissiveUvIndex)).rgb;
        emissive *= eTex;
    }

    float shadow = 0.0; // calculateShadow(input.position, N, L);
    float3 color = ambient * ao + Lo * (1.0 - shadow) + emissive;

    color = ApplyDistanceFog(
        color,
        input.position,
        Globals.viewPos,
        EnvironmentSampler,
        EnvironmentMap
    );

    return float4(color, baseColor.a);
}