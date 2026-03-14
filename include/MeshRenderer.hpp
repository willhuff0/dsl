#pragma once

#include "utils/GrowingBuffer.hpp"

#include <dvk/dvk.hpp>

#include <unordered_map>

struct Material {
    uint64_t albedoIndex;
    uint64_t metallicIndex;
    uint64_t roughnessIndex;
    uint64_t aoIndex;
    uint64_t normalIndex;
};

struct Mesh {
    uint64_t positionBufferOffset;
    uint64_t normalBufferOffset;
    uint64_t tangentBufferOffset;
    uint64_t uvBufferOffset;
    uint64_t indexBufferOffset;

    uint32_t indexCount;

    uint32_t materialId;

    std::unordered_map<uint32_t, uint32_t> instanceIndices;
    uint32_t nextInstanceId = 0;
};

struct MeshInstance {

};

class MeshRenderer {
public:
    MeshRenderer();

    uint32_t LoadShader(const std::string& path);
    uint32_t LoadMesh(const std::string& path, uint32_t materialId);

    void UpdateBindings();

    void Draw(dvk::CommandBuffer& commandBuffer);

private:
    GrowingBuffer positionBuffer;
    GrowingBuffer normalBuffer;
    GrowingBuffer tangentBuffer;
    GrowingBuffer uvBuffer;
    GrowingBuffer indexBuffer;

    GrowingBuffer indirectArgumentBuffer;

    GrowingBuffer instanceBuffer;

    dvk::BindSet::Ref bindSet;
    dvk::GraphicsPipeline::Ref graphicsPipeline;

    std::vector<Material> materials;
    std::vector<Mesh> meshes;
};
