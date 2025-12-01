#include "Terrain.hpp"
#include "Utils/Paths.hpp"

#include <stdexcept>
#include <cmath>
#include <vector>
#include <raylib-cpp/raymath.hpp>
#include <Deps/FastNoiseLite.h>

//#region Chunk

Chunk::Chunk(raylib::Mesh&& mesh, GridPos pos) : mesh(std::forward<raylib::Mesh>(mesh)) {
    transform = MatrixTranslate(
        (float)(pos.x * TERRAIN_CHUNK_SIZE) * TERRAIN_CELL_SIZE,
        0.0f,
        (float)(pos.z * TERRAIN_CHUNK_SIZE) * TERRAIN_CELL_SIZE
    );
}

Chunk::~Chunk() {
    //UnloadMesh(mesh);
    //Unload();

    //delete[] mesh.vertices;
}

void Chunk::Load() {
    if (isLoaded) return;
    isLoaded = true;
    //UploadMesh(&this->mesh, false);
}

void Chunk::Unload() {
    if (!isLoaded) return;
    isLoaded = false;
    //UnloadMesh(mesh);
}

void Chunk::Draw(const Material& material) {
    if (!isLoaded) throw std::runtime_error("Tried to draw an unloaded chunk!");
    mesh.Draw(material, transform);
}

//#endregion

Terrain::Terrain(int seed)
    : seed(seed),
      shader(TERRAIN_SHADER_PATHS),
      albedoTexture(TERRAIN_ALBEDO_TEXTURE_PATH),
      metallicTexture(TERRAIN_METALLIC_TEXTURE_PATH),
      roughnessTexture(TERRAIN_ROUGHNESS_TEXTURE_PATH),
      aoTexture(TERRAIN_AO_TEXTURE_PATH) {
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "u_viewPos");

    shader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(shader, "u_albedoTexture");
    shader.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(shader, "u_metallicTexture");
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "u_roughnessTexture");
    shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(shader, "u_aoTexture");

    int tilingLoc = GetShaderLocation(shader, "u_tiling");
    float tiling = 0.25f;
    SetShaderValue(shader, tilingLoc, &tiling, SHADER_UNIFORM_FLOAT);

    int ambientLoc = GetShaderLocation(shader, "u_ambient");
    float ambient = 0.03f;
    SetShaderValue(shader, ambientLoc, &ambient, SHADER_UNIFORM_FLOAT);

    material.SetShader(shader);
    material.SetTexture(MATERIAL_MAP_ALBEDO, albedoTexture);
    material.SetTexture(MATERIAL_MAP_METALNESS, metallicTexture);
    material.SetTexture(MATERIAL_MAP_ROUGHNESS, roughnessTexture);
    material.SetTexture(MATERIAL_MAP_OCCLUSION, aoTexture);
}

void Terrain::Tick(Vector3 playerPos) {
    float cameraPos[3] = { playerPos.x, playerPos.y, playerPos.z };
    SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

    GridPos playerChunkPos = {
        .x = (int)std::floor(playerPos.x / TERRAIN_CHUNK_SIZE),
        .z = (int)std::floor(playerPos.z / TERRAIN_CHUNK_SIZE),
    };

    std::unordered_set<GridPos> chunksToUnload(visibleChunks);
    std::unordered_set<GridPos> chunksToLoad;
    for (int cx = -TERRAIN_VIEW_DISTANCE_CHUNKS; cx < TERRAIN_VIEW_DISTANCE_CHUNKS; cx++) {
        for (int cz = -TERRAIN_VIEW_DISTANCE_CHUNKS; cz < TERRAIN_VIEW_DISTANCE_CHUNKS; cz++) {
            if (cx * cx + cz * cz > TERRAIN_VIEW_DISTANCE_CHUNKS * TERRAIN_VIEW_DISTANCE_CHUNKS) continue;

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

        chunks.at(chunkPos).Unload();
    }
    for (const auto& chunkPos: chunksToLoad) {
        visibleChunks.emplace(chunkPos);

        auto iter = chunks.find(chunkPos);
        if (iter == chunks.end()) {
            generateChunk(chunkPos);
        } else {
            iter->second.Load();
        }
    }
}

void Terrain::Draw() {
    for (const auto& chunkPos : visibleChunks) {
        chunks.at(chunkPos).Draw(material);
    }
}

void Terrain::generateChunk(GridPos pos) {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    noise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    noise.SetSeed(seed);

    const int generationStride = TERRAIN_CHUNK_SIZE + 3;
    std::vector<raylib::Vector3> generatedVertices(generationStride * generationStride);

    for (int localX = 0; localX < generationStride; localX++) {
        for (int localZ = 0; localZ < generationStride; localZ++) {
            float worldX = (float)(pos.x * TERRAIN_CHUNK_SIZE + (localX - 1)) * TERRAIN_CELL_SIZE;
            float worldZ = (float)(pos.z * TERRAIN_CHUNK_SIZE + (localZ - 1)) * TERRAIN_CELL_SIZE;

            float height = noise.GetNoise(worldX, worldZ) * 6.0f;

            generatedVertices[localZ * generationStride + localX] = raylib::Vector3{
                (float)(localX - 1) * TERRAIN_CELL_SIZE,
                height,
                (float)(localZ - 1) * TERRAIN_CELL_SIZE
            };
        }
    }

    raylib::Mesh mesh;
    const int meshStride = TERRAIN_CHUNK_SIZE + 1;
    mesh.vertexCount = meshStride * meshStride;
    mesh.triangleCount = TERRAIN_CHUNK_FACE_COUNT;
    mesh.vertices = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = (float*)RL_CALLOC(mesh.vertexCount * 3, sizeof(float));
    mesh.indices = (unsigned short*)RL_MALLOC(TERRAIN_CHUNK_FACE_COUNT * 3 * sizeof(unsigned short));

    for (int localX = 0; localX < meshStride; localX++) {
        for (int localZ = 0; localZ < meshStride; localZ++) {
            const auto& v = generatedVertices[(localZ + 1) * generationStride + (localX + 1)];
            int vertexOffset = (localZ * meshStride + localX) * 3;
            mesh.vertices[vertexOffset + 0] = v.x;
            mesh.vertices[vertexOffset + 1] = v.y;
            mesh.vertices[vertexOffset + 2] = v.z;
        }
    }

    for (int localX = 0; localX < TERRAIN_CHUNK_SIZE; localX++) {
        for (int localZ = 0; localZ < TERRAIN_CHUNK_SIZE; localZ++) {
            int indexOffset = (localZ * TERRAIN_CHUNK_SIZE + localX) * 6;

            unsigned short i0 = (localZ + 0) * meshStride + (localX + 0);
            unsigned short i1 = (localZ + 0) * meshStride + (localX + 1);
            unsigned short i2 = (localZ + 1) * meshStride + (localX + 1);
            unsigned short i3 = (localZ + 1) * meshStride + (localX + 0);

            mesh.indices[indexOffset + 0] = i2;
            mesh.indices[indexOffset + 1] = i1;
            mesh.indices[indexOffset + 2] = i0;
            mesh.indices[indexOffset + 3] = i0;
            mesh.indices[indexOffset + 4] = i3;
            mesh.indices[indexOffset + 5] = i2;

            const auto& v0 = generatedVertices[(localZ + 1) * generationStride + (localX + 1)];
            const auto& v1 = generatedVertices[(localZ + 1) * generationStride + (localX + 2)];
            const auto& v2 = generatedVertices[(localZ + 2) * generationStride + (localX + 2)];
            const auto& v3 = generatedVertices[(localZ + 2) * generationStride + (localX + 1)];

            Vector3 n1 = (v2 - v0).CrossProduct(v1 - v0).Normalize();
            mesh.normals[i0 * 3 + 0] += n1.x; mesh.normals[i0 * 3 + 1] += n1.y; mesh.normals[i0 * 3 + 2] += n1.z;
            mesh.normals[i1 * 3 + 0] += n1.x; mesh.normals[i1 * 3 + 1] += n1.y; mesh.normals[i1 * 3 + 2] += n1.z;
            mesh.normals[i2 * 3 + 0] += n1.x; mesh.normals[i2 * 3 + 1] += n1.y; mesh.normals[i2 * 3 + 2] += n1.z;

            Vector3 n2 = (v3 - v0).CrossProduct(v2 - v0).Normalize();
            mesh.normals[i0 * 3 + 0] += n2.x; mesh.normals[i0 * 3 + 1] += n2.y; mesh.normals[i0 * 3 + 2] += n2.z;
            mesh.normals[i2 * 3 + 0] += n2.x; mesh.normals[i2 * 3 + 1] += n2.y; mesh.normals[i2 * 3 + 2] += n2.z;
            mesh.normals[i3 * 3 + 0] += n2.x; mesh.normals[i3 * 3 + 1] += n2.y; mesh.normals[i3 * 3 + 2] += n2.z;
        }
    }

    for (int i = 0; i < mesh.vertexCount; i++) {
        raylib::Vector3 normal = { mesh.normals[i * 3 + 0], mesh.normals[i * 3 + 1], mesh.normals[i * 3 + 2] };
        normal = normal.Normalize();
        mesh.normals[i * 3 + 0] = normal.x;
        mesh.normals[i * 3 + 1] = normal.y;
        mesh.normals[i * 3 + 2] = normal.z;
    }

    UploadMesh(&mesh, false);

    chunks.emplace(std::piecewise_construct, std::forward_as_tuple(pos), std::forward_as_tuple(std::move(mesh), pos));
}