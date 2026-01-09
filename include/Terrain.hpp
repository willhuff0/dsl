#pragma once

#include <dvk/dvk.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

struct TerrainUniforms {
    float tiling;
    float ambientIntensity;
};

struct ChunkUniforms {
    glm::mat4 modelMatrix;
};

struct GridPos {
    int x, z;

    bool operator==(const GridPos& other) const {
        return x == other.x && z == other.z;
    }
};
template<> struct std::hash<GridPos> {
    std::size_t operator() (const GridPos& chunkPos) const noexcept {
        auto h1 = std::hash<int>{}(chunkPos.x);
        auto h2 = std::hash<int>{}(chunkPos.z);
        return h1 ^ (h2 << 1);
    }
};

class Chunk {
public:
    Chunk(GridPos pos, const dvk::BindPool::Ref& bindPool, const dvk::BindSetLayout::Ref& bindSetLayout,
          const std::vector<glm::vec3>& positions,
          const std::vector<glm::vec3>& normals,
          const std::vector<uint32_t>& indices
    );

    void Draw(const dvk::PipelineLayout::Ref& pipelineLayout, const vk::raii::CommandBuffer& commandBuffer) const;

private:
    dvk::Buffer::Ref positionBuffer;
    dvk::Buffer::Ref normalBuffer;
    dvk::Buffer::Ref indexBuffer;
    dvk::BindSet::Ref bindSet;
};

class Terrain {
public:
    explicit Terrain(int seed);

    void Tick(glm::vec3 playerPos);
    void Draw(const vk::raii::CommandBuffer& commandBuffer);

private:
    void generateChunk(GridPos pos);

    int seed;

    dvk::BindSetLayout::Ref chunkBindSetLayout;
    dvk::BindSet::Ref bindSet;
    dvk::PipelineLayout::Ref shaderPipelineLayout;
    dvk::GraphicsPipeline::Ref shaderPipeline;

    dvk::Buffer::Ref uniformBuffer;

    dvk::TextureView::Ref albedoTextureView;
    dvk::TextureView::Ref metallicTextureView;
    dvk::TextureView::Ref roughnessTextureView;
    dvk::TextureView::Ref aoTextureView;
    dvk::Sampler::Ref sampler;

    dvk::BindPool::Ref chunkBindPool;

    std::unordered_map<GridPos, Chunk> chunks;
    std::unordered_set<GridPos> visibleChunks;
};