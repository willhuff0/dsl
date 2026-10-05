#include "../Globals.slang"
#include "RendererStructs.slang"

[[vk::binding(1, 0)]] StructuredBuffer<Instance> Instances;
[[vk::binding(2, 1)]] StructuredBuffer<uint32_t> VisibleInstanceIndices;

[[vk::binding(0, 2)]] ConstantBuffer<GlobalUniforms> Globals;

[[vk::binding(1, 2)]] SamplerState EnvironmentSampler;
[[vk::binding(2, 2)]] TextureCube<float4> IrradianceMap;
[[vk::binding(3, 2)]] TextureCube<float4> RadianceMap;
[[vk::binding(4, 2)]] Texture2D<float2> BrdfLut;

[[vk::binding(0, 3)]] Sampler2D Textures[];

struct VertexOutput {
    float4 position : SV_Position;
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

    // Assume uniform scaling
    float3x3 vectorMatrix = float3x3(instance.modelMatrix);

    VertexOutput output;
    output.position = mul(instance.modelMatrix, float4(input.position, 1.0));
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

    uint32_t metallicRoughnessTexture;
    uint32_t metallicRoughnessUvIndex;
    float roughnessFactor;
    float metallicFactor;

    uint32_t normalTexture;
    uint32_t normalUvIndex;
    float normalScale;

    uint32_t occlusionTexture;
    uint32_t occlusionUvIndex;
    float occlusionStrength;

    float3 emissiveFactor;
    uint32_t emissiveTexture;
    uint32_t emissiveUvIndex;

    float ambientIntensity;
    uint32_t alphaMode;
    float alphaCutoff;
};
[[vk::binding(0, 4)]] StructuredBuffer<Material> Materials;

[shader("fragment")]
float4 FragmentMain(
    VertexOutput input
) : SV_Target {
    
}