#include <Renderer.hpp>

#include "utils/Paths.hpp"
#include "components/InstanceComponent.hpp"
#include "utils/GrowingBuffer.hpp"
#include "components/PhysicsComponent.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

static constexpr uint64_t BUFFER_GROW_FACTOR = 2;
static constexpr uint32_t MAX_SHADER_PASSES = 64;
static constexpr uint32_t MAX_TEXTURES = 1024 * 64;
static constexpr uint32_t MAX_SAMPLERS = 128;
static constexpr uint32_t NUM_STATIC_BUFFERS = 3;
static constexpr uint32_t NUM_PER_FRAME_BUFFERS = 8;
static constexpr uint32_t NUM_STORAGE_BUFFERS = (NUM_STATIC_BUFFERS + NUM_PER_FRAME_BUFFERS) * FRAMES_IN_FLIGHT + MAX_SHADER_PASSES;

// Static scene capacities. Each is a single chunk allocated up front in device-local memory, since the
// draw path binds one vertex buffer, one index buffer and one buffer per StructuredBuffer.
static constexpr uint32_t MAX_VERTICES = 8 * 1024 * 1024;       // 448 MiB at 56 bytes per vertex
static constexpr uint32_t MAX_INDICES = 32 * 1024 * 1024;       // 128 MiB
static constexpr uint32_t MAX_MESHES = 64 * 1024;
static constexpr uint32_t MAX_INSTANCES = 256 * 1024;
static constexpr uint32_t MAX_MATERIALS_PER_SHADER = 16 * 1024;

Renderer::Renderer(BindingProvider* cameraProvider) : _uploadQueue(GetUploadQueue()) {
    _vertices = std::make_unique<ChunkedBuffer>(_uploadQueue, ChunkedBuffer::Desc {
        .label = "[Mesh Renderer] Vertices",
        .usage = vk::BufferUsageFlagBits::eVertexBuffer,
        .elementSize = sizeof(VertexDesc),
        .elementsPerChunk = MAX_VERTICES,
    });
    _indices = std::make_unique<ChunkedBuffer>(_uploadQueue, ChunkedBuffer::Desc {
        .label = "[Mesh Renderer] Indices",
        .usage = vk::BufferUsageFlagBits::eIndexBuffer,
        .elementSize = sizeof(uint32_t),
        .elementsPerChunk = MAX_INDICES,
    });
    _meshMetadata = std::make_unique<ChunkedBuffer>(_uploadQueue, ChunkedBuffer::Desc {
        .label = "[Mesh Renderer] Mesh Metadata",
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .elementSize = sizeof(MeshMetadata),
        .elementsPerChunk = MAX_MESHES,
    });
    _instances = std::make_unique<ChunkedBuffer>(_uploadQueue, ChunkedBuffer::Desc {
        .label = "[Mesh Renderer] Instances",
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .elementSize = sizeof(Instance),
        .elementsPerChunk = MAX_INSTANCES,
    });
    _staticInstanceFrameData = std::make_unique<ChunkedBuffer>(_uploadQueue, ChunkedBuffer::Desc {
        .label = "[Mesh Renderer] Static Instance Frame Data",
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .elementSize = sizeof(InstanceFrameData),
        .elementsPerChunk = MAX_INSTANCES,
    });

    _bindPool = dvk::BindPool::Create(GetDevice())
            .SetLabel("[Mesh Renderer]")
            .SetMaxSets(1 + 2 * FRAMES_IN_FLIGHT + MAX_SHADER_PASSES)
            .AddEntry(vk::DescriptorType::eStorageBuffer, NUM_STORAGE_BUFFERS)
            .AddEntry(vk::DescriptorType::eSampledImage, MAX_TEXTURES)
            .AddEntry(vk::DescriptorType::eSampler, MAX_SAMPLERS)
            .SetFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
            .Build();

    _shaderPassBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Shader Pass")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();

    _texturesBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Textures Samplers")
        .SetFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
        .AddEntry(0)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .SetCount(MAX_TEXTURES)
            .SetFlags(vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .SetCount(MAX_SAMPLERS)
            .SetFlags(vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind)
            .FinishEntry()
        .Build();
    _texturesSamplersBindSet = dvk::BindSet::Create(GetDevice(), _bindPool, _texturesBindSetLayout, "[Mesh Renderer] Textures Samplers");

    _staticBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Static")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
            .FinishEntry(2)
        .Build();
    _staticBindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), _bindPool, _staticBindSetLayout, "[Mesh Renderer] Static");
    // Chunks never move, so these are bound once.
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshMetadata->GetChunk(0))
                .FinishBuffer()
            .BindBuffer(1)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_instances->GetChunk(0))
                .FinishBuffer()
            .BindBuffer(2)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_staticInstanceFrameData->GetChunk(0))
                .FinishBuffer()
            .Update(_staticBindSet[i]);
    }

    _dynamicInstanceFrameData = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Dynamic Instance Frame Data")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(BUFFER_START_SIZE)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build<FRAMES_IN_FLIGHT>();
    _meshToInstanceIndexOffsets = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Mesh to Instance Index Offset")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(BUFFER_START_SIZE)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build<FRAMES_IN_FLIGHT>();
    _meshInstanceCounts = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Mesh to Instance Count")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
        .SetSize(BUFFER_START_SIZE)
        .Build<FRAMES_IN_FLIGHT>();
    _visibleInstanceIndices = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Visible Instance Indices")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(BUFFER_START_SIZE)
        .Build<FRAMES_IN_FLIGHT>();
    _shaderToCommandIndexOffsets = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Shader to Command Index Offset")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(BUFFER_START_SIZE)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build<FRAMES_IN_FLIGHT>();
    _drawCounts = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Draw Counts")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst)
        .SetSize(BUFFER_START_SIZE)
        .Build<FRAMES_IN_FLIGHT>();
    _compactedCommands = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Compacted Commands")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer)
        .SetSize(BUFFER_START_SIZE)
        .Build<FRAMES_IN_FLIGHT>();
    _activeInstanceIndices = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Active Instance Indices")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(BUFFER_START_SIZE)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build<FRAMES_IN_FLIGHT>();
    _perFrameBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Per Frame")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry(2)
        .AddEntry(3)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
            .FinishEntry()
        .AddEntry(4)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry(4)
        .Build();
    _perFrameBindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), _bindPool, _perFrameBindSetLayout, "[Mesh Renderer] Per Frame");
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_dynamicInstanceFrameData[i])
                .FinishBuffer()
            .BindBuffer(1)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshToInstanceIndexOffsets[i])
                .FinishBuffer()
            .BindBuffer(2)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshInstanceCounts[i])
                .FinishBuffer()
            .BindBuffer(3)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_visibleInstanceIndices[i])
                .FinishBuffer()
            .BindBuffer(4)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_shaderToCommandIndexOffsets[i])
                .FinishBuffer()
            .BindBuffer(5)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_drawCounts[i])
                .FinishBuffer()
            .BindBuffer(6)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_compactedCommands[i])
                .FinishBuffer()
            .BindBuffer(7)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_activeInstanceIndices[i])
                .FinishBuffer()
            .Update(_perFrameBindSet[i]);
    }

    auto computePipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Compute")
        .AddPushConstantRange()
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .SetSize(2 * sizeof(uint32_t))
            .FinishPushConstantRange()
        .AddBindSetLayout(_staticBindSetLayout)
        .AddBindSetLayout(_perFrameBindSetLayout)
        .AddBindSetLayout(cameraProvider->GetLayout())
        .Build();
    _computePass1Watcher = ShaderHotReload::CreateAndWatch(RENDERER_COMPUTE_PASS1_SHADER_PATH, [this, computePipelineLayout](const auto& path) {
        auto computePass1Shader = dvk::Shader::LoadFile(GetDevice(), path, "[Mesh Renderer] Compute Pass 1");
        this->_computePass1Pipeline = dvk::ComputePipeline::Create(GetDevice(), computePipelineLayout)
            .SetLabel("[Mesh Renderer] Compute Pass 1")
            .SetStage(computePass1Shader, "CullInstances").FinishStage()
            .Build();
    });
    _computePass2Watcher = ShaderHotReload::CreateAndWatch(RENDERER_COMPUTE_PASS2_SHADER_PATH, [this, computePipelineLayout](const auto& path) {
        auto computePass2Shader = dvk::Shader::LoadFile(GetDevice(), path, "[Mesh Renderer] Compute Pass 2");
        this->_computePass2Pipeline = dvk::ComputePipeline::Create(GetDevice(), computePipelineLayout)
            .SetLabel("[Mesh Renderer] Compute Pass 2")
            .SetStage(computePass2Shader, "CompactCommands").FinishStage()
            .Build();
    });

    _defaultSampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Default Sampler")
        .SetAddressMode(vk::SamplerAddressMode::eRepeat)
        .SetAnisotropy(true, 8.0f)
        .Build();
    dvk::Binder()
        .BindSampler(1)
            .SetArrayTarget(0, 1)
            .SetSampler(_defaultSampler)
            .FinishSampler()
        .Update(_texturesSamplersBindSet);

    auto depthOnlyPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Depth Only Layout")
        .AddBindSetLayout(_staticBindSetLayout)
        .AddBindSetLayout(_perFrameBindSetLayout)
        .AddBindSetLayout(cameraProvider->GetLayout())
        .Build();
    _depthOnlyPipelineWatcher = ShaderHotReload::CreateAndWatch(RENDERER_DEPTH_ONLY_SHADER_PATH, [this, depthOnlyPipelineLayout](const auto& path) {
        auto depthOnlyShader = dvk::Shader::LoadFile(GetDevice(), path, "[Mesh Renderer] Depth Only Module");
        _shadowPipeline = dvk::GraphicsPipeline::Create(GetDevice(), depthOnlyPipelineLayout)
            .SetLabel("[Mesh Renderer] Shadow Depth Pipeline")
            .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
            .SetRasterizer()
                .SetCullMode(vk::CullModeFlagBits::eFront)
                // Casters between the light and the cascade's near plane are clamped to depth 0 instead of
                // being clipped, so they still occlude. Clamping is per fragment, so triangles crossing the
                // near plane stay correct too.
                .EnableDepthClamp()
                .FinishRasterizer()
            .AddStage(vk::ShaderStageFlagBits::eVertex, depthOnlyShader, "VertexMain").FinishStage()
            .EnableDepth(SHADOW_DEPTH_FORMAT)
            .Build();
        _depthOnlyPipeline = dvk::GraphicsPipeline::Create(GetDevice(), depthOnlyPipelineLayout)
            .SetLabel("[Mesh Renderer] Depth Only Pipeline")
            .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
            .AddStage(vk::ShaderStageFlagBits::eVertex, depthOnlyShader, "VertexMain").FinishStage()
            .SetDepth()
                .SetFormat(GetDepthFormat())
                .SetDepthCompare(vk::CompareOp::eLess)
                .EnableDepthTest()
                .EnableDepthWrite()
                .FinishDepth()
            .Build();
    });
}

ShaderIndex Renderer::AddShader(const dvk::GraphicsPipeline::Ref& pipeline, vk::DeviceSize materialSize, bool isTransparent, const std::string& name) {
    auto materials = std::make_unique<ChunkedBuffer>(_uploadQueue, ChunkedBuffer::Desc {
        .label = "[Mesh Renderer] '" + name + "' Materials",
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .elementSize = materialSize,
        .elementsPerChunk = MAX_MATERIALS_PER_SHADER,
    });
    auto bindSet = dvk::BindSet::Create(GetDevice(), _bindPool, _shaderPassBindSetLayout, "[Mesh Renderer] '" + name + "'");
    dvk::Binder()
        .BindBuffer(0)
            .SetBufferType(vk::DescriptorType::eStorageBuffer)
            .SetBuffer(materials->GetChunk(0))
            .FinishBuffer()
        .Update(bindSet);

    std::lock_guard lock(_mutex);

    ShaderIndex index = _shaderPasses.size();
    _shaderPasses.emplace_back(ShaderPass {
        .pipeline = pipeline,
        .isTransparent = isTransparent,
        .materials = std::move(materials),
        .bindSet = std::move(bindSet),
    });
    _materials.emplace_back();
    _shaderToLiveMeshCount.emplace_back(0);

    if (isTransparent) {
        _numTransparentShaderPasses++;
    } else {
        _numOpaqueShaderPasses++;
    }

    return index;
}

void Renderer::SetShaderPipeline(uint32_t frameIndex, ShaderIndex shader, const dvk::GraphicsPipeline::Ref& pipeline) {
    auto& shaderPass = _shaderPasses.at(shader);
    if (shaderPass.pipeline != nullptr) {
        GetCommandEncoder().DropRefOnceNotInUse(frameIndex, shaderPass.pipeline);
    }
    shaderPass.pipeline = pipeline;
}

TextureIndex Renderer::AddTexture(const dvk::TextureView::Ref& textureView) {
    TextureIndex index = _textures.size();
    _textures.emplace_back(textureView);

    dvk::Binder()
        .BindTexture(0)
            .SetArrayTarget(index, 1)
            .SetTextureType(vk::DescriptorType::eSampledImage)
            .SetImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .SetTextureView(textureView)
            .FinishTexture()
        .Update(_texturesSamplersBindSet);

    return index;
}

SamplerIndex Renderer::AddSampler(const dvk::Sampler::Ref& sampler) {
    SamplerIndex index = _samplers.size();
    _samplers.emplace_back(sampler);

    dvk::Binder()
        .BindSampler(1)
            .SetArrayTarget(index, 1)
            .SetSampler(sampler)
            .FinishSampler()
        .Update(_texturesSamplersBindSet);

    return index;
}

MaterialIndex Renderer::AddMaterial(ShaderIndex shader, const void* src, vk::DeviceSize size) {
    ChunkedBuffer* materials;
    {
        std::lock_guard lock(_mutex);
        materials = _shaderPasses.at(shader).materials.get();
    }
    assert(size == materials->GetElementSize());

    auto [allocation, ticket] = materials->AllocateAndUpload(src, 1);
    if (!allocation) throw std::runtime_error("[Mesh Renderer] Out of material space, raise MAX_MATERIALS_PER_SHADER");

    std::lock_guard lock(_mutex);
    MaterialIndex index = allocation.firstElement;
    auto& records = _materials[shader];
    if (records.size() <= index) records.resize(index + 1);
    records[index] = MaterialRecord {
        .live = true,
        .allocation = allocation,
        .ticket = ticket,
    };
    return index;
}

MeshIndex Renderer::AddMesh(ShaderIndex shader, const MeshDesc& meshDesc) {
    auto vertexCount = static_cast<uint32_t>(meshDesc.vertices.size());
    auto indexCount = static_cast<uint32_t>(meshDesc.indices.size());

    auto vertices = _vertices->Allocate(vertexCount);
    auto indices = _indices->Allocate(indexCount);
    auto metadata = _meshMetadata->Allocate(1);
    if (!vertices || !indices || !metadata) {
        // Nothing was uploaded yet, so these can go straight back.
        _vertices->Free(vertices);
        _indices->Free(indices);
        _meshMetadata->Free(metadata);
        throw std::runtime_error("[Mesh Renderer] Out of mesh space (or empty mesh), raise MAX_VERTICES, MAX_INDICES or MAX_MESHES");
    }

    MeshMetadata meshMetadata = {
        .indexCount = indexCount,
        .firstIndex = indices.firstElement,
        .vertexOffset = static_cast<int32_t>(vertices.firstElement),
        .shaderIndex = shader,
        .boundingSphereCenter = meshDesc.boundingSphereCenter,
        .boundingSphereRadius = meshDesc.boundingSphereRadius,
    };

    UploadTicket ticket;
    ticket |= _vertices->Upload(vertices, meshDesc.vertices.data(), vertexCount);
    ticket |= _indices->Upload(indices, meshDesc.indices.data(), indexCount);
    ticket |= _meshMetadata->Upload(metadata, &meshMetadata, 1);

    std::lock_guard lock(_mutex);
    MeshIndex index = metadata.firstElement;
    if (_meshes.size() <= index) {
        _meshes.resize(index + 1);
        _meshToActiveInstanceCount.resize(index + 1);
    }
    _meshes[index] = MeshRecord {
        .live = true,
        .shader = shader,
        .vertices = vertices,
        .indices = indices,
        .metadata = metadata,
        .ticket = ticket,
    };
    _meshSlotHighWater = std::max(_meshSlotHighWater, index + 1);
    _shaderToLiveMeshCount[shader]++;
    return index;
}

InstanceIndex Renderer::AddInstance(const std::shared_ptr<InstanceComponent>& component, const InstanceDesc& instanceDesc) {
    auto instanceAllocation = _instances->Allocate(1);
    if (!instanceAllocation) throw std::runtime_error("[Mesh Renderer] Out of instance space, raise MAX_INSTANCES");
    InstanceIndex index = instanceAllocation.firstElement;

    Instance instance {
        .meshIndex = instanceDesc.meshIndex,
        .materialIndex = instanceDesc.materialIndex,
        .isDynamic = instanceDesc.isDynamic ? vk::True : vk::False,
    };

    UploadTicket ticket;
    ChunkedBuffer::Allocation staticFrameData;
    DynamicFrameDataIndex dynamicSlot = NULL_INDEX;
    if (instanceDesc.isDynamic) {
        std::lock_guard lock(_mutex);
        if (!_freeDynamicSlots.empty()) {
            dynamicSlot = _freeDynamicSlots.back();
            _freeDynamicSlots.pop_back();
        } else {
            dynamicSlot = static_cast<DynamicFrameDataIndex>(_dynamicSlots.size());
            _dynamicSlots.emplace_back();
        }
        _dynamicSlots[dynamicSlot] = DynamicSlot {
            .component = component,
            .instance = index,
        };
        instance.frameDataIndex = dynamicSlot;
    } else {
        staticFrameData = _staticInstanceFrameData->Allocate(1);
        if (!staticFrameData) {
            _instances->Free(instanceAllocation);
            throw std::runtime_error("[Mesh Renderer] Out of static frame data space, raise MAX_INSTANCES");
        }
        InstanceFrameData data {
            .modelMat = component->GetNode()->GetMatrix(),
        };
        ticket |= _staticInstanceFrameData->Upload(staticFrameData, &data, 1);
        instance.frameDataIndex = staticFrameData.firstElement;
    }

    ticket |= _instances->Upload(instanceAllocation, &instance, 1);

    std::lock_guard lock(_mutex);
    auto& mesh = _meshes.at(instanceDesc.meshIndex);
    assert(mesh.live);
    mesh.instanceRefs++;
    // The instance can only be drawn once its mesh and material are resident too.
    ticket |= mesh.ticket;
    ticket |= _materials.at(mesh.shader).at(instanceDesc.materialIndex).ticket;

    if (_instanceRecords.size() <= index) _instanceRecords.resize(index + 1);
    _instanceRecords[index] = InstanceRecord {
        .live = true,
        .mesh = instanceDesc.meshIndex,
        .instance = instanceAllocation,
        .staticFrameData = staticFrameData,
        .dynamicSlot = dynamicSlot,
        .ticket = ticket,
    };
    _pendingInstances.push_back(index);
    return index;
}

void Renderer::RemoveInstance(InstanceIndex instance) {
    std::lock_guard lock(_mutex);
    _removedInstances.push_back(instance);
}

void Renderer::RemoveMesh(MeshIndex mesh) {
    std::lock_guard lock(_mutex);
    _removedMeshes.push_back(mesh);
}

void Renderer::RemoveMaterial(ShaderIndex shader, MaterialIndex material) {
    std::lock_guard lock(_mutex);
    _removedMaterials.emplace_back(shader, material);
}

void Renderer::UploadWorldState(uint32_t frameIndex) {
    // This slot's fence has been waited on, so ranges freed two frames ago can be reused.
    _vertices->BeginFrame(frameIndex);
    _indices->BeginFrame(frameIndex);
    _meshMetadata->BeginFrame(frameIndex);
    _instances->BeginFrame(frameIndex);
    _staticInstanceFrameData->BeginFrame(frameIndex);
    for (auto& shaderPass : _shaderPasses) {
        shaderPass.materials->BeginFrame(frameIndex);
    }

    {
        std::lock_guard lock(_mutex);

        // Removals are applied before this frame is recorded, so it never references them. Their ranges are
        // freed now but only reused once the frames already in flight have finished.
        _processRemovalsLocked();
        _publishCompletedLocked();

        _numMeshSlots = _meshSlotHighWater;
        _activeInstancesStaging = _activeInstances;
        _numActiveInstances = static_cast<uint32_t>(_activeInstancesStaging.size());

        _meshToInstanceIndexOffsetsStaging.resize(_numMeshSlots);
        uint32_t instanceOffset = 0;
        for (uint32_t mesh = 0; mesh < _numMeshSlots; mesh++) {
            _meshToInstanceIndexOffsetsStaging[mesh] = instanceOffset;
            instanceOffset += _meshToActiveInstanceCount[mesh];
        }

        _shaderToMaxMeshCount = _shaderToLiveMeshCount;
        _shaderToCommandIndexOffsetsStaging.resize(_shaderToMaxMeshCount.size());
        uint32_t commandOffset = 0;
        for (size_t shader = 0; shader < _shaderToMaxMeshCount.size(); shader++) {
            _shaderToCommandIndexOffsetsStaging[shader] = commandOffset;
            commandOffset += _shaderToMaxMeshCount[shader];
        }

        _dynamicInstanceFrameDataStaging.resize(_dynamicSlots.size());
        for (size_t slot = 0; slot < _dynamicSlots.size(); slot++) {
            if (auto component = _dynamicSlots[slot].component.lock()) {
                auto node = component->GetNode();
                auto physicsComponent = node->GetComponent<PhysicsComponent>();
                if (physicsComponent != nullptr) {
                    _dynamicInstanceFrameDataStaging[slot].modelMat = physicsComponent->GetInterpolatedMatrix();
                } else {
                    _dynamicInstanceFrameDataStaging[slot].modelMat = node->GetMatrix();
                }
            }
        }
    }

    dvk::Binder perFrameBinder;
    bool didResizeAnyPerFrameBuffer = false;
    didResizeAnyPerFrameBuffer |= _maybeResizeDynamicInstanceFrameData(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeMeshToInstanceIndexOffsets(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeMeshInstanceCounts(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeVisibleInstanceIndices(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeShaderToCommandIndexOffsets(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeDrawCounts(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeCompactedCommands(frameIndex, perFrameBinder);
    didResizeAnyPerFrameBuffer |= _maybeResizeActiveInstanceIndices(frameIndex, perFrameBinder);
    if (didResizeAnyPerFrameBuffer) {
        perFrameBinder.Update(_perFrameBindSet[frameIndex]);
    }

    _computeMeshToInstanceIndexOffsets(frameIndex);
    _computeShaderToCommandIndexOffsets(frameIndex);

    if (!_activeInstancesStaging.empty()) {
        _activeInstanceIndices[frameIndex]->CopyToAndFlush(_activeInstancesStaging.data(), 0, _activeInstancesStaging.size() * sizeof(uint32_t));
    }
    if (!_dynamicInstanceFrameDataStaging.empty()) {
        _dynamicInstanceFrameData[frameIndex]->CopyToAndFlush(_dynamicInstanceFrameDataStaging.data(), 0, _dynamicInstanceFrameDataStaging.size() * sizeof(InstanceFrameData));
    }
}

UploadTicket Renderer::GetPublishedUploads() const {
    return _publishedUploads;
}

void Renderer::ComputeDrawCommands(
    dvk::CommandBuffer& commandBuffer,
    uint32_t frameIndex,
    BindingProvider* cameraProvider,
    bool cullNearPlane
) {
    commandBuffer.BeginRegion("[Mesh Renderer] Compute Draw Commands", glm::vec4(0.6f, 0.4f, 0.4f, 1.0f));

    // This runs once per view (each shadow cascade, then the main camera) into the same per-frame buffers, so the
    // previous view's pass 2 and draws must finish reading them before they are cleared and rewritten. Reads leave
    // nothing to make available, so an execution dependency is enough.
    commandBuffer.InsertBarriers()
        .AddBufferBarrier()
            .SetBuffer(_meshInstanceCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eNone)
            .SetStages(vk::PipelineStageFlagBits2::eComputeShader, vk::PipelineStageFlagBits2::eTransfer)
            .FinishBufferBarrier()
        .AddBufferBarrier()
            .SetBuffer(_drawCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eNone)
            .SetStages(vk::PipelineStageFlagBits2::eDrawIndirect, vk::PipelineStageFlagBits2::eTransfer)
            .FinishBufferBarrier()
        .AddBufferBarrier()
            .SetBuffer(_compactedCommands[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eNone)
            .SetStages(vk::PipelineStageFlagBits2::eDrawIndirect, vk::PipelineStageFlagBits2::eComputeShader)
            .FinishBufferBarrier()
        .AddBufferBarrier()
            .SetBuffer(_visibleInstanceIndices[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eNone)
            .SetStages(vk::PipelineStageFlagBits2::eVertexShader, vk::PipelineStageFlagBits2::eComputeShader)
            .FinishBufferBarrier()
        .FinishBarriers();

    commandBuffer.FillBuffer(_meshInstanceCounts[frameIndex], 0);
    commandBuffer.FillBuffer(_drawCounts[frameIndex], 0);

    commandBuffer.InsertBarriers()
        .AddBufferBarrier()
            .SetBuffer(_meshInstanceCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite)
            .SetStages(vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eComputeShader)
            .FinishBufferBarrier()
        .FinishBarriers();

    commandBuffer.SetComputePipeline(_computePass1Pipeline);
    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    cameraProvider->Bind(2, commandBuffer, frameIndex);
    commandBuffer.PushConstant(0, vk::ShaderStageFlagBits::eCompute, _numActiveInstances);
    commandBuffer.PushConstant(sizeof(uint32_t), vk::ShaderStageFlagBits::eCompute, static_cast<uint32_t>(cullNearPlane ? 1 : 0));
    commandBuffer.Dispatch((_numActiveInstances + 63) / 64);

    commandBuffer.InsertBarriers()
        .AddBufferBarrier()
            .SetBuffer(_meshInstanceCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eShaderStorageWrite, vk::AccessFlagBits2::eShaderStorageRead)
            .SetStages(vk::PipelineStageFlagBits2::eComputeShader, vk::PipelineStageFlagBits2::eComputeShader)
            .FinishBufferBarrier()
        .AddBufferBarrier()
            .SetBuffer(_visibleInstanceIndices[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eShaderStorageWrite, vk::AccessFlagBits2::eShaderStorageRead)
            .SetStages(vk::PipelineStageFlagBits2::eComputeShader, vk::PipelineStageFlagBits2::eVertexShader)
            .FinishBufferBarrier()
        .AddBufferBarrier()
            .SetBuffer(_drawCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite)
            .SetStages(vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eComputeShader)
            .FinishBufferBarrier()
        .FinishBarriers();

    commandBuffer.SetComputePipeline(_computePass2Pipeline);
    commandBuffer.PushConstant(0, vk::ShaderStageFlagBits::eCompute, _numMeshSlots);
    commandBuffer.Dispatch((_numMeshSlots + 63) / 64);

    commandBuffer.InsertBarriers()
        .AddBufferBarrier()
            .SetBuffer(_drawCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eShaderStorageWrite, vk::AccessFlagBits2::eIndirectCommandRead)
            .SetStages(vk::PipelineStageFlagBits2::eComputeShader, vk::PipelineStageFlagBits2::eDrawIndirect)
            .FinishBufferBarrier()
        .AddBufferBarrier()
            .SetBuffer(_compactedCommands[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eShaderStorageWrite, vk::AccessFlagBits2::eIndirectCommandRead)
            .SetStages(vk::PipelineStageFlagBits2::eComputeShader, vk::PipelineStageFlagBits2::eDrawIndirect)
            .FinishBufferBarrier()
        .FinishBarriers();

    commandBuffer.EndRegion();
}

void Renderer::DrawOpaque(
    dvk::CommandBuffer& commandBuffer,
    uint32_t frameIndex,
    BindingProvider* cameraProvider,
    BindingProvider* environmentProvider,
    BindingProvider* directionalLightProvider
) {
    if (_numOpaqueShaderPasses == 0) return;

    commandBuffer.SetGraphicsPipeline(_shaderPasses[0].pipeline);

    commandBuffer.SetVertexBuffer(0, _vertices->GetChunk(0));
    commandBuffer.SetIndexBuffer(_indices->GetChunk(0));

    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    cameraProvider->Bind(2, commandBuffer, frameIndex);
    environmentProvider->Bind(3, commandBuffer, frameIndex);
    directionalLightProvider->Bind(4, commandBuffer, frameIndex);
    commandBuffer.SetBindSet(5, _texturesSamplersBindSet);

    for (uint32_t i = 0; i < _shaderToMaxMeshCount.size(); i++) {
        const auto& shaderPass = _shaderPasses[i];
        if (shaderPass.isTransparent || _shaderToMaxMeshCount[i] == 0) continue;

        commandBuffer.SetGraphicsPipeline(shaderPass.pipeline);
        commandBuffer.SetBindSet(6, shaderPass.bindSet);
        commandBuffer.DrawIndexedIndirectCount(
            _compactedCommands[frameIndex],
            _drawCounts[frameIndex],
            _shaderToMaxMeshCount[i],
            _shaderToCommandIndexOffsetsStaging[i] * sizeof(VkDrawIndexedIndirectCommand),
            i * sizeof(uint32_t));
    }
}

void Renderer::DrawTransparent(
    dvk::CommandBuffer& commandBuffer,
    uint32_t frameIndex,
    BindingProvider* cameraProvider,
    BindingProvider* environmentProvider,
    BindingProvider* directionalLightProvider
) {
    if (_numTransparentShaderPasses == 0) return;

    commandBuffer.SetGraphicsPipeline(_shaderPasses[0].pipeline);

    commandBuffer.SetVertexBuffer(0, _vertices->GetChunk(0));
    commandBuffer.SetIndexBuffer(_indices->GetChunk(0));

    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    cameraProvider->Bind(2, commandBuffer, frameIndex);
    environmentProvider->Bind(3, commandBuffer, frameIndex);
    directionalLightProvider->Bind(4, commandBuffer, frameIndex);
    commandBuffer.SetBindSet(5, _texturesSamplersBindSet);

    for (uint32_t i = 0; i < _shaderToMaxMeshCount.size(); i++) {
        const auto& shaderPass = _shaderPasses[i];
        if (!shaderPass.isTransparent || _shaderToMaxMeshCount[i] == 0) continue;

        commandBuffer.SetGraphicsPipeline(shaderPass.pipeline);
        commandBuffer.SetBindSet(6, shaderPass.bindSet);
        commandBuffer.DrawIndexedIndirectCount(
            _compactedCommands[frameIndex],
            _drawCounts[frameIndex],
            _shaderToMaxMeshCount[i],
            _shaderToCommandIndexOffsetsStaging[i] * sizeof(VkDrawIndexedIndirectCommand),
            i * sizeof(uint32_t));
    }
}

void Renderer::DrawDepthOnly(
    dvk::CommandBuffer& commandBuffer,
    uint32_t frameIndex,
    BindingProvider* cameraProvider,
    bool forShadows
) {
    if (_numOpaqueShaderPasses == 0) return;

    commandBuffer.SetGraphicsPipeline(forShadows ? _shadowPipeline : _depthOnlyPipeline);

    commandBuffer.SetVertexBuffer(0, _vertices->GetChunk(0));
    commandBuffer.SetIndexBuffer(_indices->GetChunk(0));

    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    cameraProvider->Bind(2, commandBuffer, frameIndex);

    for (uint32_t i = 0; i < _shaderToMaxMeshCount.size(); i++) {
        const auto& shaderPass = _shaderPasses[i];
        if (shaderPass.isTransparent || _shaderToMaxMeshCount[i] == 0) continue;

        commandBuffer.DrawIndexedIndirectCount(
            _compactedCommands[frameIndex],
            _drawCounts[frameIndex],
            _shaderToMaxMeshCount[i],
            _shaderToCommandIndexOffsetsStaging[i] * sizeof(VkDrawIndexedIndirectCommand),
            i * sizeof(uint32_t));
    }
}

dvk::BindSetLayout::Ref Renderer::GetStaticBindSetLayout() const {
    return _staticBindSetLayout;
}

dvk::BindSetLayout::Ref Renderer::GetPerFrameBindSetLayout() const {
    return _perFrameBindSetLayout;
}

dvk::BindSetLayout::Ref Renderer::GetShaderPassBindSetLayout() const {
    return _shaderPassBindSetLayout;
}

dvk::BindSetLayout::Ref Renderer::GetTexturesBindSetLayout() const {
    return _texturesBindSetLayout;
}

void Renderer::_processRemovalsLocked() {
    // Dynamic instances whose component has been destroyed have nothing left to read a transform from.
    for (const auto& slot : _dynamicSlots) {
        if (slot.instance != NULL_INDEX && slot.component.expired()) {
            _removedInstances.push_back(slot.instance);
        }
    }

    for (InstanceIndex index : _removedInstances) {
        if (index >= _instanceRecords.size() || !_instanceRecords[index].live) continue;
        auto& record = _instanceRecords[index];

        if (record.published) {
            _deactivateLocked(index);
        } else {
            std::erase(_pendingInstances, index);
        }

        _instances->Free(record.instance);
        _staticInstanceFrameData->Free(record.staticFrameData);
        if (record.dynamicSlot != NULL_INDEX) {
            // Dynamic frame data is rewritten per frame in flight, so the slot can be reused immediately.
            _dynamicSlots[record.dynamicSlot] = {};
            _freeDynamicSlots.push_back(record.dynamicSlot);
        }
        _meshes[record.mesh].instanceRefs--;

        record = {};
    }
    _removedInstances.clear();

    for (auto [shader, material] : _removedMaterials) {
        if (shader >= _materials.size() || material >= _materials[shader].size()) continue;
        auto& record = _materials[shader][material];
        if (!record.live) continue;

        _shaderPasses[shader].materials->Free(record.allocation);
        record = {};
    }
    _removedMaterials.clear();

    for (MeshIndex index : _removedMeshes) {
        if (index >= _meshes.size() || !_meshes[index].live) continue;
        auto& record = _meshes[index];
        assert(record.instanceRefs == 0 && "Remove a mesh's instances before the mesh");

        _vertices->Free(record.vertices);
        _indices->Free(record.indices);
        _meshMetadata->Free(record.metadata);
        _shaderToLiveMeshCount[record.shader]--;

        record = {};
    }
    _removedMeshes.clear();
}

void Renderer::_publishCompletedLocked() {
    uint64_t completed = _uploadQueue.GetCompletedValue();

    std::erase_if(_pendingInstances, [&](InstanceIndex index) {
        if (_instanceRecords[index].ticket.value > completed) return false;
        _activateLocked(index);
        return true;
    });

    // Everything published is at or below this value, so the frame's wait on it is already satisfied.
    _publishedUploads = UploadTicket { completed };
}

void Renderer::_activateLocked(InstanceIndex index) {
    auto& record = _instanceRecords[index];
    record.published = true;
    record.activePosition = static_cast<uint32_t>(_activeInstances.size());
    _activeInstances.push_back(index);
    _meshToActiveInstanceCount[record.mesh]++;
}

void Renderer::_deactivateLocked(InstanceIndex index) {
    auto& record = _instanceRecords[index];

    uint32_t position = record.activePosition;
    InstanceIndex last = _activeInstances.back();
    _activeInstances[position] = last;
    _instanceRecords[last].activePosition = position;
    _activeInstances.pop_back();

    record.published = false;
    record.activePosition = NULL_INDEX;
    _meshToActiveInstanceCount[record.mesh]--;
}

bool Renderer::_maybeResizeDynamicInstanceFrameData(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _dynamicInstanceFrameData[frameIndex]->GetSize();
    vk::DeviceSize minSize = _dynamicInstanceFrameDataStaging.size() * sizeof(InstanceFrameData);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _dynamicInstanceFrameData[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Dynamic Instance Frame Data")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build();

    binder.BindBuffer(0)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeMeshToInstanceIndexOffsets(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _meshToInstanceIndexOffsets[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numMeshSlots * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _meshToInstanceIndexOffsets[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Mesh to Instance Index Offset")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build();

    binder.BindBuffer(1)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeMeshInstanceCounts(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _meshInstanceCounts[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numMeshSlots * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _meshInstanceCounts[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Mesh Instance Counts")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
        .SetSize(size)
        .Build();

    binder.BindBuffer(2)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeVisibleInstanceIndices(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _visibleInstanceIndices[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numActiveInstances * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _visibleInstanceIndices[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Visible Instance Indices")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .Build();

    binder.BindBuffer(3)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeShaderToCommandIndexOffsets(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _shaderToCommandIndexOffsets[frameIndex]->GetSize();
    vk::DeviceSize minSize = _shaderToCommandIndexOffsetsStaging.size() * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _shaderToCommandIndexOffsets[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Shader to Command Index Offset")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build();

    binder.BindBuffer(4)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeDrawCounts(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _drawCounts[frameIndex]->GetSize();
    vk::DeviceSize minSize = _shaderToMaxMeshCount.size() * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _drawCounts[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Draw Counts")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst)
        .SetSize(size)
        .Build();

    binder.BindBuffer(5)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeCompactedCommands(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _compactedCommands[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numMeshSlots * sizeof(VkDrawIndexedIndirectCommand);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _compactedCommands[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Compacted Commands")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer)
        .SetSize(size)
        .Build();

    binder.BindBuffer(6)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeActiveInstanceIndices(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _activeInstanceIndices[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numActiveInstances * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _activeInstanceIndices[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Active Instance Indices")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build();

    binder.BindBuffer(7)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

void Renderer::_computeMeshToInstanceIndexOffsets(uint32_t frameIndex) {
    if (_meshToInstanceIndexOffsetsStaging.empty()) return;
    _meshToInstanceIndexOffsets[frameIndex]->CopyToAndFlush(_meshToInstanceIndexOffsetsStaging.data(), 0, _meshToInstanceIndexOffsetsStaging.size() * sizeof(uint32_t));
}

void Renderer::_computeShaderToCommandIndexOffsets(uint32_t frameIndex) {
    if (_shaderToCommandIndexOffsetsStaging.empty()) return;
    _shaderToCommandIndexOffsets[frameIndex]->CopyToAndFlush(_shaderToCommandIndexOffsetsStaging.data(), 0, _shaderToCommandIndexOffsetsStaging.size() * sizeof(uint32_t));
}
