#pragma once

#include "utils/GrowingBuffer.hpp"
#include "ShaderHotReload.hpp"
#include "Provider.hpp"

#include <dvk/dvk.hpp>

#include <vector>
#include <memory>
#include <utility>
#include <optional>

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
};

struct MeshMetadata {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;

    uint32_t shaderIndex;

    alignas(16) glm::vec3 aabbMin;
    alignas(16) glm::vec3 aabbMax;
};
static_assert(sizeof(MeshMetadata) == 48);

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

    uint32_t numMaterials = 0;
    GrowingBuffer materials;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> bindSet;

    std::array<bool, FRAMES_IN_FLIGHT> needRebindMaterials{};

    void SetNeedRebindMaterials();
};

class Renderer {
public:
    Renderer();

    ShaderIndex AddShader(const dvk::GraphicsPipeline::Ref& pipeline, bool isTransparent = false, const std::string& name = "");
    void SetShaderPipeline(uint32_t frameIndex, ShaderIndex shader, const dvk::GraphicsPipeline::Ref& pipeline);
    TextureIndex AddTexture(const dvk::TextureView::Ref& textureView);
    SamplerIndex AddSampler(const dvk::Sampler::Ref& sampler);
    MaterialIndex AddMaterial(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, ShaderIndex shader, const void* src, uint64_t size);
    MeshIndex AddMesh(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, ShaderIndex shader, const MeshDesc& meshDesc);
    std::pair<InstanceIndex, std::optional<DynamicFrameDataIndex>> AddInstance(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const std::shared_ptr<InstanceComponent>& component, const InstanceDesc& instanceDesc);

    void Update(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);
    void DrawOpaque(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);
    void DrawTransparent(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);

    [[nodiscard]] dvk::BindSetLayout::Ref GetStaticBindSetLayout() const;
    [[nodiscard]] dvk::BindSetLayout::Ref GetPerFrameBindSetLayout() const;
    [[nodiscard]] dvk::BindSetLayout::Ref GetShaderPassBindSetLayout() const;
    [[nodiscard]] dvk::BindSetLayout::Ref GetTexturesBindSetLayout() const;

private:
    dvk::BindPool::Ref _bindPool;

    dvk::BindSetLayout::Ref _staticBindSetLayout;
    dvk::BindSetLayout::Ref _perFrameBindSetLayout;
    dvk::BindSetLayout::Ref _shaderPassBindSetLayout;
    dvk::BindSetLayout::Ref _texturesBindSetLayout;

    int32_t _vertexCount = 0;
    uint32_t _indexCount = 0;
    GrowingBuffer _vertexArray;
    GrowingBuffer _indexArray;
    dvk::BindSet::Ref _texturesSamplersBindSet;

    std::array<bool, FRAMES_IN_FLIGHT> _needRebindMeshMetadata{};
    std::array<bool, FRAMES_IN_FLIGHT> _needRebindInstances{};
    std::array<bool, FRAMES_IN_FLIGHT> _needRebindStaticInstanceFrameData{};
    uint32_t _numMeshes = 0;
    uint32_t _numStaticInstances = 0;
    uint32_t _numDynamicInstances = 0;
    GrowingBuffer _meshMetadata;
    GrowingBuffer _instances;
    GrowingBuffer _staticInstanceFrameData;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> _staticBindSet;

    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _dynamicInstanceFrameData;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _meshToInstanceIndexOffsets;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _meshInstanceCounts;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _visibleInstanceIndices;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _shaderToCommandIndexOffsets;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _drawCounts;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _compactedCommands;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> _perFrameBindSet;

    Watcher _computePass1Watcher;
    dvk::ComputePipeline::Ref _computePass1Pipeline;
    Watcher _computePass2Watcher;
    dvk::ComputePipeline::Ref _computePass2Pipeline;

    // Refs
    std::vector<InstanceFrameData> _dynamicInstanceFrameDataStaging;
    std::vector<std::weak_ptr<InstanceComponent>> _dynamicInstanceIndexToComponent;

    bool _needRecomputeInstanceIndexOffsets = true;
    std::vector<uint32_t> _meshToMaxInstanceCount;
    std::vector<uint32_t> _meshToInstanceIndexOffsetsStaging;

    bool _needRecomputeCommandIndexOffsets = true;
    std::vector<uint32_t> _shaderToMaxMeshCount;
    std::vector<uint32_t> _shaderToCommandIndexOffsetsStaging;

    std::vector<dvk::TextureView::Ref> _textures;
    std::vector<dvk::Sampler::Ref> _samplers;
    std::vector<ShaderPass> _shaderPasses;
    uint32_t _numOpaqueShaderPasses = 0;
    uint32_t _numTransparentShaderPasses = 0;

    dvk::Sampler::Ref _defaultSampler;

    void _setNeedRebindMeshMetadata();
    void _setNeedRebindInstances();
    void _setNeedRebindStaticInstanceFrameData();

    bool _maybeResizeDynamicInstanceFrameData(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeMeshToInstanceIndexOffsets(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeMeshInstanceCounts(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeVisibleInstanceIndices(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeShaderToCommandIndexOffsets(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeDrawCounts(uint32_t frameIndex, dvk::Binder& binder);
    bool _maybeResizeCompactedCommands(uint32_t frameIndex, dvk::Binder& binder);

    void _computeMeshToInstanceIndexOffsets(uint32_t frameIndex);
    void _computeShaderToCommandIndexOffsets(uint32_t frameIndex);

    void _writeDynamicInstanceFrameData(uint32_t frameIndex);
};