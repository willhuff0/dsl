#include <TerrainOLD.hpp>

#include <Provider.hpp>
#include <deps/FastNoiseLite.h>
#include <TempImageLoader.hpp>
#include <utils/Paths.hpp>

constexpr int viewDistance = 12;
constexpr float cellSize = 1.0f;
constexpr int chunkSize = 32;
constexpr int indexCount = chunkSize * chunkSize * 6;

Chunk::Chunk(GridPos pos, const dvk::BindPool::Ref& bindPool, const dvk::BindSetLayout::Ref& bindSetLayout,
             const std::vector<glm::vec3>& positions,
             const std::vector<glm::vec3>& normals,
             const std::vector<uint32_t>& indices) {
    std::string chunkName = "Chunk (" + std::to_string(pos.x) + "," + std::to_string(pos.z) + ")";

    positionBuffer = dvk::Buffer::Create(GetDevice())
            .SetLabel(chunkName + " Position Buffer")
            .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
            .SetSize(positions.size() * sizeof(glm::vec3))
            .EnableWriteWithStaging()
            .Build();
    positionBuffer->WriteWithStaging(GetCommandEncoder(), positions.data());

    normalBuffer = dvk::Buffer::Create(GetDevice())
            .SetLabel(chunkName + " Normal Buffer")
            .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
            .SetSize(normals.size() * sizeof(glm::vec3))
            .EnableWriteWithStaging()
            .Build();
    normalBuffer->WriteWithStaging(GetCommandEncoder(), normals.data());

    indexBuffer = dvk::Buffer::Create(GetDevice())
            .SetLabel(chunkName + " Index Buffer")
            .SetUsage(vk::BufferUsageFlagBits::eIndexBuffer)
            .SetSize(indices.size() * sizeof(uint32_t))
            .EnableWriteWithStaging()
            .Build();
    indexBuffer->WriteWithStaging(GetCommandEncoder(), indices.data());

    auto translation = glm::translate(glm::identity<glm::mat4>(), glm::vec3(
        static_cast<float>(pos.x * chunkSize) * cellSize,
        0.0f,
        static_cast<float>(-pos.z * chunkSize) * cellSize
    ));

    auto uniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel(chunkName + " Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(ChunkUniforms))
        .EnableWriteWithStaging()
        .Build();
    ChunkUniforms uniforms {
        .modelMatrix = glm::translate(glm::identity<glm::mat4>(), glm::vec3(
            static_cast<float>(pos.x * chunkSize) * cellSize,
            0.0f,
            static_cast<float>(-pos.z * chunkSize) * cellSize
        )),
    };
    uniformBuffer->WriteWithStaging(GetCommandEncoder(), &uniforms);

    bindSet = dvk::BindSet::Create(GetDevice(), bindPool, bindSetLayout, chunkName + " Bind Set");
    // TODO: Need to recycle bind sets because they are finite in amount. Should replace with draw indirect

    dvk::Binder()
        .BindBuffer(0)
            .SetBuffer(uniformBuffer)
            .FinishBuffer()
        .Update(bindSet);
}

void Chunk::Draw(const dvk::PipelineLayout::Ref& pipelineLayout, const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindIndexBuffer(indexBuffer->GetBuffer(), 0, vk::IndexType::eUint32);
    commandBuffer.bindVertexBuffers(0, *positionBuffer->GetBuffer(), { 0 });
    commandBuffer.bindVertexBuffers(1, *normalBuffer->GetBuffer(), { 0 });
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout->GetPipelineLayout(), 2, *bindSet->GetDescriptorSet(), nullptr);
    commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
}

Terrain::Terrain(int seed) : seed(seed) {
    chunkBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Terrain Chunk")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eVertex)
            .FinishEntry()
        .Build();

    auto terrainBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Terrain")
        .AddEntry(0).SetType(vk::DescriptorType::eUniformBuffer).SetStage(vk::ShaderStageFlagBits::eFragment).FinishEntry()
        .AddEntry(1).SetType(vk::DescriptorType::eSampledImage).SetStage(vk::ShaderStageFlagBits::eFragment).FinishEntry()
        .AddEntry(2).SetType(vk::DescriptorType::eSampledImage).SetStage(vk::ShaderStageFlagBits::eFragment).FinishEntry()
        .AddEntry(3).SetType(vk::DescriptorType::eSampledImage).SetStage(vk::ShaderStageFlagBits::eFragment).FinishEntry()
        .AddEntry(4).SetType(vk::DescriptorType::eSampledImage).SetStage(vk::ShaderStageFlagBits::eFragment).FinishEntry()
        .AddEntry(5).SetType(vk::DescriptorType::eSampler).SetStage(vk::ShaderStageFlagBits::eFragment).FinishEntry()
        .Build();

    uniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Terrain Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(TerrainUniforms))
        .EnableWriteWithStaging()
        .Build();
    TerrainUniforms terrainUniforms {
        .tiling = 0.125f,
        .ambientIntensity = 2.0f,
    };
    uniformBuffer->WriteWithStaging(GetCommandEncoder(), &terrainUniforms);

    albedoTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain Albedo", LoadImage(TERRAIN_ALBEDO_TEXTURE_PATH, 4, Data::Image::ColorSpace::SRGB))).Build();
    metallicTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain Metallic", LoadImage(TERRAIN_METALLIC_TEXTURE_PATH, 1))).Build();
    roughnessTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain Roughness", LoadImage(TERRAIN_ROUGHNESS_TEXTURE_PATH, 1))).Build();
    aoTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain AO", LoadImage(TERRAIN_AO_TEXTURE_PATH, 1))).Build();
    sampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("Terrain Sampler")
        .SetAddressMode(vk::SamplerAddressMode::eRepeat)
        .Build();

    bindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), terrainBindSetLayout, "Terrain");

    dvk::Binder()
        .BindBuffer(0).SetBuffer(uniformBuffer).FinishBuffer()
        .BindTexture(1).SetTextureView(albedoTextureView).FinishTexture()
        .BindTexture(2).SetTextureView(metallicTextureView).FinishTexture()
        .BindTexture(3).SetTextureView(roughnessTextureView).FinishTexture()
        .BindTexture(4).SetTextureView(aoTextureView).FinishTexture()
        .BindSampler(5).SetSampler(sampler).FinishSampler()
        .Update(bindSet);

    shaderPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Terrain")
        .AddBindSetLayout(GetGlobalBindSetLayout())
        .AddBindSetLayout(terrainBindSetLayout)
        .AddBindSetLayout(chunkBindSetLayout)
        .Build();

    auto shader = dvk::Shader::LoadFile(GetDevice(), TERRAIN_SHADER_PATH, "Terrain");

    shaderPipeline = dvk::GraphicsPipeline::Create(GetDevice(), shaderPipelineLayout)
        .SetLabel("Terrain")
        .SetVertexInput({
            vk::VertexInputBindingDescription {
                .binding = 0,
                .stride = sizeof(glm::vec3),
                .inputRate = vk::VertexInputRate::eVertex,
            },
            vk::VertexInputBindingDescription {
                .binding = 1,
                .stride = sizeof(glm::vec3),
                .inputRate = vk::VertexInputRate::eVertex,
            },
        }, {
            vk::VertexInputAttributeDescription {
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = 0,
            },
            vk::VertexInputAttributeDescription {
                .location = 1,
                .binding = 1,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = 0,
            },
        })
        .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "fs_main").FinishStage()
        .EnableDepth(GetDepthFormat())
        .EnableMultisample(GetMultisampleCount())
        .SetSingleAttachment(GetRenderTextureFormat())
        .Build();

    chunkBindPool = dvk::BindPool::Create(GetDevice())
        .SetLabel("Terrain Chunks")
        .SetMaxSets(viewDistance * viewDistance)
        .AddEntry(vk::DescriptorType::eUniformBuffer, viewDistance * viewDistance)
        .Build();
}

void Terrain::Tick(glm::vec3 playerPos) {
    GridPos playerChunkPos = {
        .x = (int)std::floor(playerPos.x / (chunkSize * cellSize)),
        .z = (int)std::floor(-playerPos.z / (chunkSize * cellSize)),
    };

    std::unordered_set<GridPos> chunksToUnload(visibleChunks);
    std::unordered_set<GridPos> chunksToLoad;
    for (int cx = -viewDistance; cx < viewDistance; cx++) {
        for (int cz = -viewDistance; cz < viewDistance; cz++) {
            if (cx * cx + cz * cz > viewDistance * viewDistance) continue;

            int x = cx + playerChunkPos.x;
            int z = cz + playerChunkPos.z;

            auto iter = chunksToUnload.find({ x, z });
            if (iter == chunksToUnload.end()) {
                chunksToLoad.emplace(x, z);
            } else {
                chunksToUnload.erase(iter);
            }
        }
    }

    for (const auto& chunkPos: chunksToUnload) {
        visibleChunks.erase(chunkPos);

        //chunks.at(chunkPos).Unload();
    }
    for (const auto& chunkPos: chunksToLoad) {
        visibleChunks.emplace(chunkPos);

        auto iter = chunks.find(chunkPos);
        if (iter == chunks.end()) {
            generateChunk(chunkPos);
        } else {
            //iter->second.Load();
        }
    }
}

void Terrain::Draw(const vk::raii::CommandBuffer& commandBuffer) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *shaderPipeline->GetPipeline());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *shaderPipelineLayout->GetPipelineLayout(), 1, *bindSet->GetDescriptorSet(), nullptr);
    for (const auto& chunkPos : visibleChunks) {
        chunks.at(chunkPos).Draw(shaderPipelineLayout, commandBuffer);
    }
}

void Terrain::generateChunk(GridPos pos) {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    noise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    noise.SetFractalOctaves(6);
    noise.SetFrequency(0.0015);
    noise.SetSeed(seed);

    const int heightsStride = chunkSize + 3;
    float heights[heightsStride][heightsStride];

    for (int localX = -1; localX < heightsStride - 1; localX++) {
        for (int localZ = -1; localZ < heightsStride - 1; localZ++) {
            float worldX = (float)(pos.x * chunkSize + localX) * cellSize;
            float worldZ = (float)(-pos.z * chunkSize - localZ) * cellSize;

            heights[localX + 1][localZ + 1] = noise.GetNoise(worldX, worldZ) * 24.0f;
        }
    }

    const int meshStride = chunkSize + 1;
    const int vertexCount = meshStride * meshStride;

    std::vector<glm::vec3> positions;
    positions.reserve(vertexCount);
    for (int localX = 0; localX < meshStride; localX++) {
        for (int localZ = 0; localZ < meshStride; localZ++) {
            const auto& height = heights[localX + 1][localZ + 1];
            positions.emplace_back((float)localX * cellSize, height, (float)-localZ * cellSize);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    for (int localX = 0; localX < chunkSize; localX++) {
        for (int localZ = 0; localZ < chunkSize; localZ++) {
            unsigned short i0 = (localX + 0) * meshStride + (localZ + 0);
            unsigned short i1 = (localX + 1) * meshStride + (localZ + 0);
            unsigned short i2 = (localX + 1) * meshStride + (localZ + 1);
            unsigned short i3 = (localX + 0) * meshStride + (localZ + 1);

            indices.emplace_back(i0);
            indices.emplace_back(i1);
            indices.emplace_back(i2);
            indices.emplace_back(i2);
            indices.emplace_back(i3);
            indices.emplace_back(i0);
        }
    }

    std::vector<glm::vec3> normals;
    normals.resize(vertexCount);
    for (int localX = -1; localX < heightsStride - 2; localX++) {
        for (int localZ = -1; localZ < heightsStride - 2; localZ++) {
            const auto& v0 = glm::vec3((float)(localX + 0) * cellSize, heights[localX + 1][localZ + 1], (float)(-localZ + 0) * cellSize);
            const auto& v1 = glm::vec3((float)(localX + 1) * cellSize, heights[localX + 2][localZ + 1], (float)(-localZ + 0) * cellSize);
            const auto& v2 = glm::vec3((float)(localX + 1) * cellSize, heights[localX + 2][localZ + 2], (float)(-localZ - 1) * cellSize);
            const auto& v3 = glm::vec3((float)(localX + 0) * cellSize, heights[localX + 1][localZ + 2], (float)(-localZ - 1) * cellSize);

            glm::vec3 n1 = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            if (localX >= 0 && localZ >= 0) normals[(localX + 0) * meshStride + (localZ + 0)] += n1;
            if (localX + 1 < meshStride && localZ >= 0) normals[(localX + 1) * meshStride + (localZ + 0)] += n1;
            if (localX + 1 < meshStride && localZ + 1 < meshStride) normals[(localX + 1) * meshStride + (localZ + 1)] += n1;

            glm::vec3 n2 = glm::normalize(glm::cross(v3 - v2, v0 - v2));
            if (localX + 1 < meshStride && localZ + 1 < meshStride) normals[(localX + 1) * meshStride + (localZ + 1)] += n2;
            if (localX >= 0 && localZ + 1 < meshStride) normals[(localX + 0) * meshStride + (localZ + 1)] += n2;
            if (localX >= 0 && localZ >= 0) normals[(localX + 0) * meshStride + (localZ + 0)] += n2;
        }
    }

    for (auto& normal: normals) {
        normal = glm::normalize(normal);
    }

    chunks.emplace(std::piecewise_construct, std::forward_as_tuple(pos), std::forward_as_tuple(pos, chunkBindPool, chunkBindSetLayout, positions, normals, indices));
}