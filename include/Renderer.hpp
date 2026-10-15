#pragma once

#include "utils/ChunkedBuffer.hpp"
#include "utils/UploadQueue.hpp"
#include "ShaderHotReload.hpp"
#include "Provider.hpp"
#include "BindingProvider.hpp"

#include <dvk/dvk.hpp>

#include <vector>
#include <memory>
#include <utility>
#include <optional>
#include <mutex>

class InstanceComponent;

using ShaderIndex = uint32_t;
using MaterialIndex = uint32_t;
using TextureIndex = uint32_t;
using SamplerIndex = uint32_t;
using MeshIndex = uint32_t;
using InstanceIndex = uint32_t;
using DynamicFrameDataIndex = uint32_t;

static constexpr uint32_t NULL_INDEX = 0xFFFFFFFF;

struct VertexDesc {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv0;
    glm::vec2 uv1;
};

constexpr auto RENDERER_VERTEX_BINDINGS = {
    vk::VertexInputBindingDescription {
        .binding = 0,
        .stride = sizeof(VertexDesc),
        .inputRate = vk::VertexInputRate::eVertex,
    },
};
constexpr auto RENDERER_VERTEX_ATTRIBUTES = {
    vk::VertexInputAttributeDescription {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(VertexDesc, position),
    },
    vk::VertexInputAttributeDescription {
        .location = 1,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(VertexDesc, normal),
    },
    vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 0,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = offsetof(VertexDesc, tangent),
    },
    vk::VertexInputAttributeDescription {
        .location = 3,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = offsetof(VertexDesc, uv0),
    },
    vk::VertexInputAttributeDescription {
        .location = 4,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = offsetof(VertexDesc, uv1),
    },
};

struct MeshDesc {
    std::vector<uint32_t> indices;
    std::vector<VertexDesc> vertices;

    glm::vec3 boundingSphereCenter;
    float boundingSphereRadius;
};

struct MeshMetadata {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;

    uint32_t shaderIndex;

    glm::vec3 boundingSphereCenter;
    float boundingSphereRadius;
};

struct InstanceDesc {
    MeshIndex meshIndex;
    MaterialIndex materialIndex;

    bool isDynamic;
};

struct Instance {
    MeshIndex meshIndex;
    MaterialIndex materialIndex;

    vk::Bool32 isDynamic;

    InstanceIndex frameDataIndex;
};

struct InstanceFrameData {
    glm::mat4 modelMat;
};

struct ShaderPass {
    dvk::GraphicsPipeline::Ref pipeline;
    bool isTransparent;

    std::unique_ptr<ChunkedBuffer> materials;
    dvk::BindSet::Ref bindSet;
};

// Static scene data (vertices, indices, mesh metadata, instances, materials) lives in device-local
// ChunkedBuffers and is uploaded asynchronously through the UploadQueue. Anything added is only drawn once all
// of the uploads it depends on have completed. Removal is deferred until no frame in flight can reference it.
//
// Thread safety: AddMaterial, AddMesh, AddInstance and the Remove functions may be called from any thread.
// AddShader, SetShaderPipeline, AddTexture, AddSampler and everything per-frame are render-thread only.
class Renderer {
public:
    Renderer(BindingProvider* cameraProvider);

    ShaderIndex AddShader(const dvk::GraphicsPipeline::Ref& pipeline, vk::DeviceSize materialSize, bool isTransparent = false, const std::string& name = "");
    void SetShaderPipeline(uint32_t frameIndex, ShaderIndex shader, const dvk::GraphicsPipeline::Ref& pipeline);
    TextureIndex AddTexture(const dvk::TextureView::Ref& textureView);
    SamplerIndex AddSampler(const dvk::Sampler::Ref& sampler);
    MaterialIndex AddMaterial(ShaderIndex shader, const void* src, vk::DeviceSize size);
    MeshIndex AddMesh(ShaderIndex shader, const MeshDesc& meshDesc);
    InstanceIndex AddInstance(const std::shared_ptr<InstanceComponent>& component, const InstanceDesc& instanceDesc);

    // Removal takes effect at the start of the next frame. Remove a mesh's instances before the mesh, and
    // stop using a material before removing it. Dynamic instances are also removed automatically once their
    // component is destroyed.
    void RemoveInstance(InstanceIndex instance);
    void RemoveMesh(MeshIndex mesh);
    void RemoveMaterial(ShaderIndex shader, MaterialIndex material);

    // Call first each frame, after dvk::CommandEncoder::GetNextFrame().
    void UploadWorldState(uint32_t frameIndex);
    // The frame's submit must wait on the upload timeline at this value. See UploadQueue::GetWaitInfo.
    [[nodiscard]] UploadTicket GetPublishedUploads() const;

    // Shadow cascades pass cullNearPlane = false: casters between the light and the cascade must still be
    // drawn, and the depth-clamped shadow pipeline flattens them onto the near plane.
    void ComputeDrawCommands(
        dvk::CommandBuffer& commandBuffer,
        uint32_t frameIndex,
        BindingProvider* cameraProvider,
        bool cullNearPlane = true);
    void DrawOpaque(
        dvk::CommandBuffer& commandBuffer,
        uint32_t frameIndex,
        BindingProvider* cameraProvider,
        BindingProvider* environmentProvider,
        BindingProvider* directionalLightProvider);
    void DrawTransparent(
        dvk::CommandBuffer& commandBuffer,
        uint32_t frameIndex,
        BindingProvider* cameraProvider,
        BindingProvider* environmentProvider,
        BindingProvider* directionalLightProvider);
    void DrawDepthOnly(
        dvk::CommandBuffer& commandBuffer,
        uint32_t frameIndex,
        BindingProvider* cameraProvider,
        bool forShadows);

    [[nodiscard]] dvk::BindSetLayout::Ref GetStaticBindSetLayout() const;
    [[nodiscard]] dvk::BindSetLayout::Ref GetPerFrameBindSetLayout() const;
    [[nodiscard]] dvk::BindSetLayout::Ref GetShaderPassBindSetLayout() const;
    [[nodiscard]] dvk::BindSetLayout::Ref GetTexturesBindSetLayout() const;

private:
    struct MeshRecord {
        bool live = false;
        ShaderIndex shader = NULL_INDEX;
        ChunkedBuffer::Allocation vertices;
        ChunkedBuffer::Allocation indices;
        ChunkedBuffer::Allocation metadata;
        uint32_t instanceRefs = 0;
        UploadTicket ticket;
    };

    struct MaterialRecord {
        bool live = false;
        ChunkedBuffer::Allocation allocation;
        UploadTicket ticket;
    };

    struct InstanceRecord {
        bool live = false;
        bool published = false;
        MeshIndex mesh = NULL_INDEX;
        ChunkedBuffer::Allocation instance;
        // Empty for dynamic instances.
        ChunkedBuffer::Allocation staticFrameData;
        DynamicFrameDataIndex dynamicSlot = NULL_INDEX;
        // Index into _activeInstances while published.
        uint32_t activePosition = NULL_INDEX;
        // Covers the instance's own uploads plus its mesh's and material's.
        UploadTicket ticket;
    };

    struct DynamicSlot {
        std::weak_ptr<InstanceComponent> component;
        InstanceIndex instance = NULL_INDEX;
    };

    dvk::BindPool::Ref _bindPool;

    dvk::BindSetLayout::Ref _staticBindSetLayout;
    dvk::BindSetLayout::Ref _perFrameBindSetLayout;
    dvk::BindSetLayout::Ref _shaderPassBindSetLayout;
    dvk::BindSetLayout::Ref _texturesBindSetLayout;

    UploadQueue& _uploadQueue;
    std::unique_ptr<ChunkedBuffer> _vertices;
    std::unique_ptr<ChunkedBuffer> _indices;
    std::unique_ptr<ChunkedBuffer> _meshMetadata;
    std::unique_ptr<ChunkedBuffer> _instances;
    std::unique_ptr<ChunkedBuffer> _staticInstanceFrameData;

    dvk::BindSet::Ref _texturesSamplersBindSet;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> _staticBindSet;

    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _dynamicInstanceFrameData;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _meshToInstanceIndexOffsets;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _meshInstanceCounts;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _visibleInstanceIndices;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _shaderToCommandIndexOffsets;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _drawCounts;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _compactedCommands;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _activeInstanceIndices;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> _perFrameBindSet;

    Watcher _computePass1Watcher;
    dvk::ComputePipeline::Ref _computePass1Pipeline;
    Watcher _computePass2Watcher;
    dvk::ComputePipeline::Ref _computePass2Pipeline;

    // Guards everything from here to the render-thread-only section.
    mutable std::mutex _mutex;
    std::vector<MeshRecord> _meshes;
    // Indexed by shader, then material.
    std::vector<std::vector<MaterialRecord>> _materials;
    std::vector<InstanceRecord> _instanceRecords;
    std::vector<DynamicSlot> _dynamicSlots;
    std::vector<DynamicFrameDataIndex> _freeDynamicSlots;
    std::vector<InstanceIndex> _pendingInstances;
    std::vector<InstanceIndex> _removedInstances;
    std::vector<MeshIndex> _removedMeshes;
    std::vector<std::pair<ShaderIndex, MaterialIndex>> _removedMaterials;
    uint32_t _meshSlotHighWater = 0;
    std::vector<uint32_t> _shaderToLiveMeshCount;
    // Published instances, in no particular order. The cull pass runs over this list.
    std::vector<uint32_t> _activeInstances;
    std::vector<uint32_t> _meshToActiveInstanceCount;

    // Render thread only. Snapshots taken in UploadWorldState for the rest of the frame.
    UploadTicket _publishedUploads;
    uint32_t _numMeshSlots = 0;
    uint32_t _numActiveInstances = 0;
    std::vector<uint32_t> _activeInstancesStaging;
    std::vector<InstanceFrameData> _dynamicInstanceFrameDataStaging;
    std::vector<uint32_t> _meshToInstanceIndexOffsetsStaging;
    std::vector<uint32_t> _shaderToMaxMeshCount;
    std::vector<uint32_t> _shaderToCommandIndexOffsetsStaging;

    std::vector<dvk::TextureView::Ref> _textures;
    std::vector<dvk::Sampler::Ref> _samplers;
    std::vector<ShaderPass> _shaderPasses;
    uint32_t _numOpaqueShaderPasses = 0;
    uint32_t _numTransparentShaderPasses = 0;

    dvk::Sampler::Ref _defaultSampler;

    Watcher _depthOnlyPipelineWatcher;
    dvk::GraphicsPipeline::Ref _shadowPipeline;
    dvk::GraphicsPipeline::Ref _depthOnlyPipeline;

    void _processRemovalsLocked();
    void _publishCompletedLocked();
    void _activateLocked(InstanceIndex instance);
    void _deactivateLocked(InstanceIndex instance);

    bool _maybeResizeDynamicInstanceFrameData(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeMeshToInstanceIndexOffsets(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeMeshInstanceCounts(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeVisibleInstanceIndices(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeShaderToCommandIndexOffsets(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeDrawCounts(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeCompactedCommands(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeActiveInstanceIndices(uint32_t frameIndex, dvk::Binder& binder);

    void _computeMeshToInstanceIndexOffsets(uint32_t frameIndex);
    void _computeShaderToCommandIndexOffsets(uint32_t frameIndex);
};
