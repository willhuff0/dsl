#include <Renderer.hpp>

#include "utils/Paths.hpp"

static constexpr uint64_t BUFFER_START_SIZE = 1024;
static constexpr uint64_t BUFFER_GROW_FACTOR = 2;
static constexpr uint32_t MAX_SHADER_PASSES = 64;
static constexpr uint32_t MAX_TEXTURES = 128;

void ShaderPass::SetNeedRebindMaterials() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        needRebindMaterials[i] = true;
    }
}

Renderer::Renderer()
    : _vertexArray(
        "[Mesh Renderer] Vertices",
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::PipelineStageFlagBits2::eVertexShader,
        BUFFER_START_SIZE),
    _indexArray(
        "[Mesh Renderer] Indices",
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::PipelineStageFlagBits2::eVertexShader,
        BUFFER_START_SIZE),
    _meshMetadata(
        "[Mesh Renderer] Mesh Metadata",
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits2::eComputeShader,
        BUFFER_START_SIZE),
    _instances(
        "[Mesh Renderer] Instances",
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits2::eComputeShader,
        BUFFER_START_SIZE)
{
    _setNeedRebindMeshMetadata();
    _setNeedRebindInstances();

    _bindPool = dvk::BindPool::Create(GetDevice())
            .SetLabel("[Mesh Renderer]")
            .SetMaxSets(32)
            .AddEntry(vk::DescriptorType::eStorageBuffer, 4 + (6 * FRAMES_IN_FLIGHT) + MAX_SHADER_PASSES)
            .AddEntry(vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURES)
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
        .SetLabel("[Mesh Renderer] Textures")
        .SetFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
        .AddEntry(0)
            .SetType(vk::DescriptorType::eCombinedImageSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .SetCount(MAX_TEXTURES)
            .SetFlags(vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind)
            .FinishEntry()
        .Build();
    _texturesBindSet = dvk::BindSet::Create(GetDevice(), _bindPool, _texturesBindSetLayout, "[Mesh Renderer] Textures");

    _staticBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Static")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eStorageBuffer)
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry(2)
        .Build();
    _staticBindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), _bindPool, _staticBindSetLayout, "[Mesh Renderer] Static");

    _meshToInstanceIndexOffsets = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Mesh to Instance Index Offset")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(BUFFER_START_SIZE)
        .EnablePersistentMapping()
        .AllowSequentialWrite()
        .Build<FRAMES_IN_FLIGHT>();
    _meshInstanceCounts = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Mesh to Instance Count")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
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
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer)
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
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry(6)
        .Build();
    _perFrameBindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), _bindPool, _perFrameBindSetLayout, "[Mesh Renderer] Per Frame");
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshToInstanceIndexOffsets[i])
                .FinishBuffer()
            .BindBuffer(1)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshInstanceCounts[i])
                .FinishBuffer()
            .BindBuffer(2)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_visibleInstanceIndices[i])
                .FinishBuffer()
            .BindBuffer(3)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_shaderToCommandIndexOffsets[i])
                .FinishBuffer()
            .BindBuffer(4)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_drawCounts[i])
                .FinishBuffer()
            .BindBuffer(5)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_compactedCommands[i])
                .FinishBuffer()
            .Update(_perFrameBindSet[i]);
    }

    auto computePipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Compute")
        .AddBindSetLayout(_staticBindSetLayout)
        .AddBindSetLayout(_perFrameBindSetLayout)
        .AddBindSetLayout(GetGlobalBindSetLayout())
        .Build();
    _computePass1Watcher = HotReload::CreateAndWatch(RENDERER_COMPUTE_PASS1_SHADER_PATH, [this, computePipelineLayout]() {
        auto computePass1Shader = dvk::Shader::LoadFile(GetDevice(), RENDERER_COMPUTE_PASS1_SHADER_PATH, "[Mesh Renderer] Compute Pass 1");
        this->_computePass1Pipeline = dvk::ComputePipeline::Create(GetDevice(), computePipelineLayout)
            .SetLabel("[Mesh Renderer] Compute Pass 1")
            .SetStage(computePass1Shader, "CullInstances").FinishStage()
            .Build();
    });
    _computePass2Watcher = HotReload::CreateAndWatch(RENDERER_COMPUTE_PASS2_SHADER_PATH, [this, computePipelineLayout]() {
    auto computePass2Shader = dvk::Shader::LoadFile(GetDevice(), RENDERER_COMPUTE_PASS2_SHADER_PATH, "[Mesh Renderer] Compute Pass 2");
        this->_computePass2Pipeline = dvk::ComputePipeline::Create(GetDevice(), computePipelineLayout)
            .SetLabel("[Mesh Renderer] Compute Pass 2")
            .SetStage(computePass2Shader, "CompactCommands").FinishStage()
            .Build();
    });
}

ShaderIndex Renderer::AddShader(const dvk::GraphicsPipeline::Ref& pipeline, const std::string& name) {
    ShaderIndex index = _shaderPasses.size();
    auto& shaderPass = _shaderPasses.emplace_back(ShaderPass {
        .pipeline = pipeline,
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
    return index;
}

TextureIndex Renderer::AddTexture(const dvk::TextureView::Ref& textureView, const dvk::Sampler::Ref& sampler) {
    TextureIndex index = _textures.size();
    _textures.emplace_back(textureView, sampler);

    dvk::Binder()
        .BindTexture(0)
            .SetArrayTarget(index, 1)
            .SetTextureType(vk::DescriptorType::eCombinedImageSampler)
            .SetImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .SetTextureView(textureView)
            .SetCombinedSampler(sampler)
            .FinishTexture()
        .Update(_texturesBindSet);

    return index;
}

MaterialIndex Renderer::AddMaterial(uint32_t frameIndex, ShaderIndex shader, const void* src, uint64_t size) {
    auto& shaderPass = _shaderPasses.at(shader);

    if (shaderPass.materials.Push(frameIndex, src, size)) {
        shaderPass.SetNeedRebindMaterials();
    }

    return shaderPass.numMaterials++;
}

MeshIndex Renderer::AddMesh(uint32_t frameIndex, ShaderIndex shader, const MeshDesc& meshDesc) {
    MeshMetadata meshMetadata = {
        .indexCount = static_cast<uint32_t>(meshDesc.indices.size()),
        .firstIndex = static_cast<uint32_t>(_indexArray.GetSize()),
        .vertexOffset = static_cast<int32_t>(_vertexArray.GetSize()),
        .shaderIndex = shader,
    };

    _indexArray.Push(frameIndex, meshDesc.indices.data(), meshDesc.indices.size() * sizeof(uint32_t));
    _vertexArray.Push(frameIndex, meshDesc.vertices.data(), meshDesc.vertices.size() * sizeof(VertexDesc));

    if (_meshMetadata.Push(frameIndex, &meshMetadata, sizeof(MeshMetadata))) {
        _setNeedRebindMeshMetadata();
    }

    _needRecomputeCommandIndexOffsets = true;
    return _numMeshes++;
}

InstanceIndex Renderer::AddInstance(uint32_t frameIndex, const Instance& instance) {
    if (_instances.Push(frameIndex, &instance, sizeof(Instance))) {
        _setNeedRebindInstances();
    }

    _needRecomputeInstanceIndexOffsets = true;
    return _numInstances++;
}

void Renderer::Draw(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    for (auto& shaderPass : _shaderPasses) {
        if (!shaderPass.needRebindMaterials[frameIndex]) continue;

        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(shaderPass.materials.GetBuffer())
                .FinishBuffer()
            .Update(shaderPass.bindSet[frameIndex]);

        shaderPass.needRebindMaterials[frameIndex] = false;
    }

    if (_needRebindMeshMetadata[frameIndex] || _needRebindInstances[frameIndex]) {
        dvk::Binder staticBinder;
        if (_needRebindMeshMetadata[frameIndex]) {
            staticBinder.BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_meshMetadata.GetBuffer())
                .FinishBuffer();
        }
        if (_needRebindInstances[frameIndex]) {
            staticBinder.BindBuffer(1)
                .SetBufferType(vk::DescriptorType::eStorageBuffer)
                .SetBuffer(_instances.GetBuffer())
                .FinishBuffer();
        }
        staticBinder.Update(_staticBindSet[frameIndex]);

        _needRebindMeshMetadata[frameIndex] = false;
        _needRebindInstances[frameIndex] = false;
    }

    dvk::Binder perFrameBinder;
    bool didResizeAnyPerFrameBuffer = false;
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

    commandBuffer.FillBuffer(_meshInstanceCounts[frameIndex], 0);
    commandBuffer.FillBuffer(_drawCounts[frameIndex], 0);

    commandBuffer.InsertBarriers()
        .AddBufferBarrier()
            .SetBuffer(_meshInstanceCounts[frameIndex])
            .SetAccess(vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite)
            .SetStages(vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eComputeShader)
            .FinishBufferBarrier()
        .FinishBarriers();

    commandBuffer.SetBindSet(0, _staticBindSet[frameIndex]);
    commandBuffer.SetBindSet(1, _perFrameBindSet[frameIndex]);
    BindGlobals(commandBuffer, frameIndex, 2);

    commandBuffer.SetComputePipeline(_computePass1Pipeline);
    commandBuffer.PushConstant(0, vk::ShaderStageFlagBits::eCompute, _numInstances);
    commandBuffer.Dispatch(_numInstances / 64);

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
    commandBuffer.Dispatch(_numMeshes / 64);

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

    commandBuffer.SetVertexBuffer(0, _vertexArray.GetBuffer());
    commandBuffer.SetIndexBuffer(_indexArray.GetBuffer());

    commandBuffer.SetBindSet(4, _texturesBindSet);

    for (uint32_t i = 0; i < _shaderPasses.size(); i++) {
        const auto& shaderPass = _shaderPasses[i];

        commandBuffer.SetGraphicsPipeline(shaderPass.pipeline);
        commandBuffer.SetBindSet(3, shaderPass.bindSet[frameIndex]);
        commandBuffer.DrawIndexedIndirectCount(
            _compactedCommands[frameIndex],
            _drawCounts[frameIndex],
            _shaderToMaxMeshCount[i],
            _shaderToCommandIndexOffsets[i] * sizeof(VkDrawIndexedIndirectCommand),
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

    binder.BindBuffer(0)
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
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .Build();

    binder.BindBuffer(1)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeVisibleInstanceIndices(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _visibleInstanceIndices[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numInstances * sizeof(uint32_t);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _visibleInstanceIndices[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Visible Instance Indices")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer)
        .SetSize(size)
        .Build();

    binder.BindBuffer(2)
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

    binder.BindBuffer(3)
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
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer)
        .SetSize(size)
        .Build();

    binder.BindBuffer(4)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return true;
}

bool Renderer::_maybeResizeCompactedCommands(uint32_t frameIndex, dvk::Binder& binder) {
    vk::DeviceSize size = _compactedCommands[frameIndex]->GetSize();
    vk::DeviceSize minSize = _numInstances * sizeof(VkDrawIndexedIndirectCommand);
    if (size >= minSize) return false;

    do {
        size *= BUFFER_GROW_FACTOR;
    } while (size < minSize);

    const auto& newBuffer = _compactedCommands[frameIndex] = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Mesh Renderer] Compacted Commands")
        .SetUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer)
        .SetSize(size)
        .Build();

    binder.BindBuffer(5)
        .SetBufferType(vk::DescriptorType::eStorageBuffer)
        .SetBuffer(newBuffer)
        .FinishBuffer();

    return false;
}

void Renderer::_computeMeshToInstanceIndexOffsets(uint32_t frameIndex) {
    uint32_t offset = 0;
    for (size_t i = 0; i < _meshIndexToMaxInstanceCount.size(); i++) {
        _shaderToCommandIndexOffsetsStaging[i] = offset;
        offset += _meshIndexToMaxInstanceCount[i];
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
