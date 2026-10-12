#include "../Camera.slang"
#include "../Renderer.slang"

[[vk::binding(0, 0)]] StructuredBuffer<MeshMetadata> MeshMetadatas;
[[vk::binding(1, 0)]] StructuredBuffer<Instance> Instances;
[[vk::binding(2, 0)]] StructuredBuffer<InstanceFrameData> StaticInstanceFrameDatas;

[[vk::binding(0, 1)]] StructuredBuffer<InstanceFrameData> DynamicInstanceFrameDatas;
[[vk::binding(1, 1)]] StructuredBuffer<uint32_t> MeshInstanceIndexOffsets;
[[vk::binding(2, 1)]] RWStructuredBuffer<Atomic<uint32_t>> MeshInstanceCounts;
[[vk::binding(3, 1)]] RWStructuredBuffer<uint32_t> VisibleInstanceIndices;

[[vk::binding(0, 2)]] ConstantBuffer<CameraUniforms> Camera;

[[vk::push_constant]] uint32_t NumInstances;

struct BoundingSphere {
    float3 center;
    float radius;
};

void ExtractFrustumPlanes(float4x4 VP, out float4 planes[6]) {
    planes[0] = VP[3] + VP[0];
    planes[1] = VP[3] - VP[0];
    planes[2] = VP[3] + VP[1];
    planes[3] = VP[3] - VP[1];
    planes[4] = VP[2];
    planes[5] = VP[3] - VP[2];

    for (int i = 0; i < 6; ++i) {
        float length = length(planes[i].xyz);
        planes[i] /= length;
    }
}

bool IsSphereVisible(BoundingSphere boundingSphere, float4 frustumPlanes[6]) {
    // Check the sphere against all 6 frustum planes
    [unroll]
    for (int i = 0; i < 6; ++i) {
        float3 planeNormal = frustumPlanes[i].xyz;
        float planeDist = frustumPlanes[i].w;

        float dist = dot(planeNormal, boundingSphere.center) + planeDist;

        if (dist < -boundingSphere.radius) {
            return false;
        }
    }
    return true;
}

BoundingSphere TransformSphere(float4x4 modelMatrix, float3 localCenter, float localRadius) {
    BoundingSphere worldSphere;

    worldSphere.center = mul(modelMatrix, float4(localCenter, 1.0)).xyz;

    float scaleX = length(modelMatrix[0].xyz);
    float scaleY = length(modelMatrix[1].xyz);
    float scaleZ = length(modelMatrix[2].xyz);

    float maxScale = max(scaleX, max(scaleY, scaleZ));

    worldSphere.radius = localRadius * maxScale;

    return worldSphere;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CullInstances(uint32_t3 threadId: SV_DispatchThreadID) {
    uint32_t instanceIndex = threadId.x;
    if (instanceIndex >= NumInstances) return;

    Instance instance = Instances[instanceIndex];
    InstanceFrameData instanceFrameData;
    if (instance.isDynamic) {
        instanceFrameData = DynamicInstanceFrameDatas[instance.frameDataIndex];
    } else {
        instanceFrameData = StaticInstanceFrameDatas[instance.frameDataIndex];
    }
    MeshMetadata meshMetadata = MeshMetadatas[instance.meshIndex];

    float4 planes[6];
    ExtractFrustumPlanes(Camera.viewProjMat, planes);

    BoundingSphere boundingSphere = TransformSphere(instanceFrameData.modelMat, meshMetadata.boundingSphereCenter, meshMetadata.boundingSphereRadius);
    if (!IsSphereVisible(boundingSphere, planes)) return;

    uint32_t perMeshLocalInstanceIndex = MeshInstanceCounts[instance.meshIndex]++;
    uint32_t meshInstanceIndexOffset = MeshInstanceIndexOffsets[instance.meshIndex];

    VisibleInstanceIndices[meshInstanceIndexOffset + perMeshLocalInstanceIndex] = instanceIndex;
}