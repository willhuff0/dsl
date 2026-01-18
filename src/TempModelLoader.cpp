#include <TempModelLoader.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stdexcept>

Mesh Mesh::LoadFile(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.c_str(), 0);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr) {
        throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
    }

    const aiMesh* mesh = scene->mMeshes[0];

    std::vector<glm::vec2> positions;
    std::vector<uint32_t> indices;

    positions.reserve(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        positions.emplace_back(mesh->mVertices[i].x, mesh->mVertices[i].z);
    }

    for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for(unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.emplace_back(face.mIndices[j]);
        }
    }

    return Mesh { std::move(positions), std::move(indices) };
}
