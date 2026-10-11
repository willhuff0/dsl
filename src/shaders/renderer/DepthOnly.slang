#include "../Camera.slang"
#include "../Renderer.slang"

static const uint32_t NULL_INDEX = 0xFFFFFFFF;

[[vk::binding(1, 0)]] StructuredBuffer<Instance> Instances;
[[vk::binding(2, 0)]] StructuredBuffer<InstanceFrameData> StaticInstanceFrameDatas;

[[vk::binding(0, 1)]] StructuredBuffer<InstanceFrameData> DynamicInstanceFrameDatas;
[[vk::binding(3, 1)]] StructuredBuffer<uint32_t> VisibleInstanceIndices;

[[vk::binding(0, 2)]] ConstantBuffer<CameraUniforms> Camera;

struct VertexOutput {
    float4 projectedPosition : SV_Position;
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

    VertexOutput output;
    output.projectedPosition = mul(Camera.viewProjMat, mul(instanceFrameData.modelMat, float4(input.position, 1.0)));
    return output;
}
