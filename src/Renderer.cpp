#include <Renderer.hpp>

#include "utils/Paths.hpp"
#include "components/InstanceComponent.hpp"

static constexpr uint64_t BUFFER_GROW_FACTOR = 2;
static constexpr uint32_t MAX_SHADER_PASSES = 64;
static constexpr uint32_t MAX_TEXTURES = 1024 * 64;
static constexpr uint32_t MAX_SAMPLERS = 128;
static constexpr uint32_t NUM_STORAGE_BUFFERS = 5 + (7 * FRAMES_IN_FLIGHT) + MAX_SHADER_PASSES;

void ShaderPass::SetNeedRebindMaterials() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        needRebindMaterials[i] = true;
    }
}

Renderer::Renderer()
    : _vertexArray(
        "[Mesh Renderer] Vertices",
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::PipelineStageFlagBits2::eVertexShader),
    _indexArray(
        "[Mesh Renderer] Indices",
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::PipelineStageFlagBits2::eVertexShader),
    _meshMetadata(
        "[Mesh Renderer] Mesh Metadata",
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits2::eComputeShader),
    _instances(
        "[Mesh Renderer] Instances",
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits2::eComputeShader),
    _staticInstanceFrameData(
        "[Mesh Renderer] Static Instance Frame Data",
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eVertexShader)
{
    _setNeedRebindMeshMetadata();
    _setNeedRebindInstances();
    _setNeedRebindStaticInstanceFrameData();

    _bindPool = dvk::BindPool::Create(GetDevice())
            .SetLabel("[Mesh Renderer]")
            .SetMaxSets(32)
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
            .FinishEntry(3)
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
            .Update(_perFrameBindSet[i]);
    }

    auto computePipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Compute")
        .AddPushConstantRange()
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .SetSize(sizeof(uint32_t))
            .FinishPushConstantRange()
        .AddBindSetLayout(_staticBindSetLayout)
        .AddBindSetLayout(_perFrameBindSetLayout)
        .AddBindSetLayout(GetGlobalBindSetLayout())
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
        .Build();
    dvk::Binder()
        .BindSampler(1)
            .SetArrayTarget(0, 1)
            .SetSampler(_defaultSampler)
            .FinishSampler()
        .Update(_texturesSamplersBindSet);
}

ShaderIndex Renderer::AddShader(const dvk::GraphicsPipeline::Ref& pipeline, bool isTransparent, const std::string& name) {
    ShaderIndex index = _shaderPasses.size();
    auto& shaderPass = _shaderPasses.emplace_back(ShaderPass {
        .pipeline = pipeline,
        .isTransparent = isTransparent,
        .materials = GrowingBuffer(
            "[Mesh Renderer] '" + name + "' Materials",
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::PipelineStageFlagBits2::eFragmentShader,
            BUFFER_START_SIZE),
        .bindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(),
            _bindPool,
            _shaderPassBindSetLayout,
            "[Mesh Renderer] '" + name + "'"),
    });
    shaderPass.SetNeedRebindMaterials();

    if (isTransparent) {
        _numTransparentShaderPasses++;
    } else {
        _numOpaqueShaderPasses++;
    }

    _shaderToMaxMeshCount.emplace_back();
    _shaderToCommandIndexOffsetsStaging.emplace_back();

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

MaterialIndex Renderer::AddMaterial(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, ShaderIndex shader, const void* src, uint64_t size) {
    auto& shaderPass = _shaderPasses.at(shader);

    if (shaderPass.materials.Push(commandBuffer, frameIndex, src, size)) {
        shaderPass.SetNeedRebindMaterials();
    }

    return shaderPass.numMaterials++;
}

MeshIndex Renderer::AddMesh(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, ShaderIndex shader, const MeshDesc& meshDesc) {
    MeshMetadata meshMetadata = {
        .indexCount = static_cast<uint32_t>(meshDesc.indices.size()),
        .firstIndex = _indexCount,
        .vertexOffset = _vertexCount,
        .shaderIndex = shader,
    };

    _indexCount += meshDesc.indices.size();
    _vertexCount += static_cast<int32_t>(meshDesc.vertices.size());

    _indexArray.Push(commandBuffer, frameIndex, meshDesc.indices.data(), meshDesc.indices.size() * sizeof(uint32_t));
    _vertexArray.Push(commandBuffer, frameIndex, meshDesc.vertices.data(), meshDesc.vertices.size() * sizeof(VertexDesc));

    if (_meshMetadata.Push(commandBuffer, frameIndex, &meshMetadata, sizeof(MeshMetadata))) {
        _setNeedRebindMeshMetadata();
    }

    _meshToMaxInstanceCount.emplace_back();
    _meshToInstanceIndexOffsetsStaging.emplace_back();

    _shaderToMaxMeshCount[shader]++;

    _needRecomputeCommandIndexOffsets = true;
    return _numMeshes++;
}

std::pair<InstanceIndex, std::optional<DynamicFrameDataIndex>> Renderer::AddInstance(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const std::shared_ptr<InstanceComponent>& component, const InstanceDesc& instanceDesc) {
    InstanceIndex index = _numStaticInstances + _numDynamicInstances;

    Instance instance {
      .meshIndex = instanceDesc.meshIndex,
      .materialIndex = instanceDesc.materialIndex,
      .isDynamic = instanceDesc.isDynamic ? vk::True : vk::False,
    };

    std::optional<DynamicFrameDataIndex> dynamicFrameDataIndex;
    if (instance.isDynamic) {
        InstanceIndex dynamicIndex = _numDynamicInstances++;
        _dynamicInstanceFrameDataStaging.emplace_back();
        _dynamicInstanceIndexToComponent.emplace_back(component);

        dynamicFrameDataIndex = instance.frameDataIndex = dynamicIndex;
    } else {
        InstanceIndex staticIndex = _numStaticInstances++;
        InstanceFrameData data {
            .modelMat = component->GetNode()->GetMatrix(),
        };
        if (_staticInstanceFrameData.Push(commandBuffer, frameIndex, &data, sizeof(InstanceFrameData))) {
            _setNeedRebindStaticInstanceFrameData();
        }

        instance.frameDataIndex = staticIndex;
    }

    if (_instances.Push(commandBuffer, frameIndex, &instance, sizeof(Instance))) {
        _setNeedRebindInstances();
    }

    _meshToMaxInstanceCount[instanceDesc.meshIndex]++;

    _needRecomputeInstanceIndexOffsets = true;
    return std::make_pair(index, dynamicFrameDataIndex);
}

void Renderer::Update(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    if (_vertexArray.GetBuffer() == nullptr) _vertexArray.Recreate(commandBuffer, frameIndex);
    if (_indexArray.GetBuffer() == nullptr) _indexArray.Recreate(commandBuffer, frameIndex);

    for (auto& shaderPass : _shaderPasses) {
        if (!shaderPass.needRebindMaterials[frameIndex] || shaderPass.materials.GetBuffer() == nullptr) continue;

        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(shaderPass.materials.GetBuffer())
                .FinishBuffer()
            .Update(shaderPass.bindSet[frameIndex]);

        shaderPass.needRebindMaterials[frameIndex] = false;
    }

    if (_needRebindMeshMetadata[frameIndex] ||
        _needRebindInstances[frameIndex] ||
        _needRebindStaticInstanceFrameData[frameIndex]
    ) {
        dvk::Binder staticBinder;
        if (_needRebindMeshMetadata[frameIndex]) {
            if (_meshMetadata.GetBuffer() == nullptr) _meshMetadata.Recreate(commandBuffer, frameIndex);
            staticBinder.BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshMetadata.GetBuffer())
                .FinishBuffer();
        }
        if (_needRebindInstances[frameIndex]) {
            if (_instances.GetBuffer() == nullptr) _instances.Recreate(commandBuffer, frameIndex);
            staticBinder.BindBuffer(1)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_instances.GetBuffer())
                .FinishBuffer();
        }
        if (_needRebindStaticInstanceFrameData[frameIndex]) {
            if (_staticInstanceFrameData.GetBuffer() == nullptr) _staticInstanceFrameData.Recreate(commandBuffer, frameIndex);
            staticBinder.BindBuffer(2)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_staticInstanceFrameData.GetBuffer())
                .FinishBuffer();
        }
        staticBinder.Update(_staticBindSet[frameIndex]);

        _needRebindMeshMetadata[frameIndex] = false;
        _needRebindInstances[frameIndex] = false;
        _needRebindStaticInstanceFrameData[frameIndex] = false;
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
    if (didResizeAnyPerFrameBuffer) {
        perFrameBinder.Update(_perFrameBindSet[frameIndex]);
    }

    _computeMeshToInstanceIndexOffsets(frameIndex);
    _computeShaderToCommandIndexOffsets(frameIndex);

    _writeDynamicInstanceFrameData(frameIndex);

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
    BindGlobals(commandBuffer, frameIndex, 2);
    commandBuffer.PushConstant(0, vk::ShaderStageFlagBits::eCompute, _numStaticInstances + _numDynamicInstances);
    commandBuffer.Dispatch((_numStaticInstances + _numDynamicInstances + 63) / 64);

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
    commandBuffer.PushConstant(0, vk::ShaderStageFlagBits::eCompute, _numMeshes);
    commandBuffer.Dispatch((_numMeshes + 63) / 64);

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
}

void Renderer::DrawOpaque(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    if (_numOpaqueShaderPasses == 0) return;

    commandBuffer.SetGraphicsPipeline(_shaderPasses[0].pipeline);

    commandBuffer.SetVertexBuffer(0, _vertexArray.GetBuffer());
    commandBuffer.SetIndexBuffer(_indexArray.GetBuffer());

    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    BindGlobals(commandBuffer, frameIndex, 2);
    commandBuffer.SetBindSet(3, _texturesSamplersBindSet);

    for (uint32_t i = 0; i < _shaderPasses.size(); i++) {
        const auto& shaderPass = _shaderPasses[i];
        if (shaderPass.isTransparent || shaderPass.materials.GetBuffer() == nullptr) continue;

        commandBuffer.SetGraphicsPipeline(shaderPass.pipeline);
        commandBuffer.SetBindSet(4, shaderPass.bindSet[frameIndex]);
        commandBuffer.DrawIndexedIndirectCount(
            _compactedCommands[frameIndex],
            _drawCounts[frameIndex],
            _shaderToMaxMeshCount[i],
            _shaderToCommandIndexOffsetsStaging[i] * sizeof(VkDrawIndexedIndirectCommand),
            i * sizeof(uint32_t));
    }
}

void Renderer::DrawTransparent(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    if (_numTransparentShaderPasses == 0) return;

    commandBuffer.SetGraphicsPipeline(_shaderPasses[0].pipeline);

    commandBuffer.SetVertexBuffer(0, _vertexArray.GetBuffer());
    commandBuffer.SetIndexBuffer(_indexArray.GetBuffer());

    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    BindGlobals(commandBuffer, frameIndex, 2);
    commandBuffer.SetBindSet(3, _texturesSamplersBindSet);

    for (uint32_t i = 0; i < _shaderPasses.size(); i++) {
        const auto& shaderPass = _shaderPasses[i];
        if (!shaderPass.isTransparent || shaderPass.materials.GetBuffer() == nullptr) continue;

        commandBuffer.SetGraphicsPipeline(shaderPass.pipeline);
        commandBuffer.SetBindSet(4, shaderPass.bindSet[frameIndex]);
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

void Renderer::_setNeedRebindMeshMetadata() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        _needRebindMeshMetadata[i] = true;
    }
}

void Renderer::_setNeedRebindInstances() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        _needRebindInstances[i] = true;
    }
}

void Renderer::_setNeedRebindStaticInstanceFrameData() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        _needRebindStaticInstanceFrameData[i] = true;
    }
}

bool Renderer::_maybeResizeDynamicInstanceFrameData(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _dynamicInstanceFrameData[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numDynamicInstances * sizeof(InstanceFrameData);
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
    vk::DeviceSize minSize = _numMeshes * sizeof(uint32_t);
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

    _needRecomputeInstanceIndexOffsets = true;

    return true;
}

bool Renderer::_maybeResizeMeshInstanceCounts(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _meshInstanceCounts[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numMeshes * sizeof(uint32_t);
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
    vk::DeviceSize minSize = (_numStaticInstances + _numDynamicInstances) * sizeof(uint32_t);
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
    vk::DeviceSize minSize = _shaderPasses.size() * sizeof(uint32_t);
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

    _needRecomputeCommandIndexOffsets = true;

    return true;
}

bool Renderer::_maybeResizeDrawCounts(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _drawCounts[frameIndex]->GetSize();
    vk::DeviceSize minSize = _shaderPasses.size() * sizeof(uint32_t);
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
    vk::DeviceSize minSize = _numMeshes * sizeof(VkDrawIndexedIndirectCommand);
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

    return false;
}

void Renderer::_computeMeshToInstanceIndexOffsets(uint32_t frameIndex) {
    uint32_t offset = 0;
    for (size_t i = 0; i < _meshToMaxInstanceCount.size(); i++) {
        _meshToInstanceIndexOffsetsStaging[i] = offset;
        offset += _meshToMaxInstanceCount[i];
    }

    _meshToInstanceIndexOffsets[frameIndex]->CopyToAndFlush(_meshToInstanceIndexOffsetsStaging.data(), 0, _meshToInstanceIndexOffsetsStaging.size() * sizeof(uint32_t));
}

void Renderer::_computeShaderToCommandIndexOffsets(uint32_t frameIndex) {
    uint32_t offset = 0;
    for (size_t i = 0; i < _shaderToMaxMeshCount.size(); i++) {
        _shaderToCommandIndexOffsetsStaging[i] = offset;
        offset += _shaderToMaxMeshCount[i];
    }

    _shaderToCommandIndexOffsets[frameIndex]->CopyToAndFlush(_shaderToCommandIndexOffsetsStaging.data(), 0, _shaderToCommandIndexOffsetsStaging.size() * sizeof(uint32_t));
}

void Renderer::_writeDynamicInstanceFrameData(uint32_t frameIndex) {
    std::erase_if(_dynamicInstanceIndexToComponent, [](const std::weak_ptr<InstanceComponent>& component) {
        return component.expired();
    });

    for (DynamicFrameDataIndex frameDataIndex = 0; frameDataIndex < _dynamicInstanceIndexToComponent.size(); frameDataIndex++) {
        const auto& component = _dynamicInstanceIndexToComponent[frameDataIndex];

        glm::mat4 modelMat = component.lock()->GetNode()->GetMatrix();

        auto& dynamicInstanceFrameData = _dynamicInstanceFrameDataStaging[frameDataIndex];
        dynamicInstanceFrameData.modelMat = modelMat;
    }

    _dynamicInstanceFrameData[frameIndex]->CopyToAndFlush(_dynamicInstanceFrameDataStaging.data(), 0, _dynamicInstanceFrameDataStaging.size() * sizeof(InstanceFrameData));
}
