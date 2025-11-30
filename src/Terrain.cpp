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
        (float)(pos.x * TERRAIN_CHUNK_SIZE),
        0.0f,
        (float)(pos.z * TERRAIN_CHUNK_SIZE)
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

Terrain::Terrain(int seed) : seed(seed), shader(TERRAIN_SHADER_PATHS) {
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    int ambientLoc = GetShaderLocation(shader, "ambient");
    float ambientValue[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    SetShaderValue(shader, ambientLoc, ambientValue, SHADER_UNIFORM_VEC4);

    material.shader = shader;

    redMaterial.maps[MATERIAL_MAP_ALBEDO].color = RED;
}

void Terrain::Tick(Vector3 playerPos) {
    float cameraPos[3] = { playerPos.x, playerPos.y, playerPos.z };
    SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

    GridPos playerChunkPos = {
        .x = (int)std::floor(playerPos.x / TERRAIN_CHUNK_SIZE),
        .z = (int)std::floor(playerPos.z / TERRAIN_CHUNK_SIZE),
    };

    lastPlayerPos = playerChunkPos;

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
        chunks.at(chunkPos).Draw(lastPlayerPos == chunkPos ? redMaterial : material);
    }
}

void Terrain::generateChunk(GridPos pos) {
    raylib::Mesh mesh;

    mesh.vertexCount = (TERRAIN_CHUNK_SIZE + 1) * (TERRAIN_CHUNK_SIZE + 1);
    mesh.triangleCount = TERRAIN_CHUNK_FACE_COUNT;
    mesh.vertices = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = (float*)RL_CALLOC(mesh.vertexCount * 3, sizeof(float));
    mesh.indices = (unsigned short*)RL_MALLOC(TERRAIN_CHUNK_FACE_COUNT * 3 * sizeof(unsigned short));

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    noise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    noise.SetSeed(seed);

    const int stride = TERRAIN_CHUNK_SIZE + 1;

    // Generate Vertices
    for (int localX = 0; localX <= TERRAIN_CHUNK_SIZE; localX++) {
        for (int localZ = 0; localZ <= TERRAIN_CHUNK_SIZE; localZ++) {
            int x = pos.x * TERRAIN_CHUNK_SIZE + localX;
            int z = pos.z * TERRAIN_CHUNK_SIZE + localZ;

            float height = noise.GetNoise((float) x, (float) z) * 6;

            int vertexOffset = (localX * stride + localZ) * 3;
            mesh.vertices[vertexOffset + 0] = (float) localX;
            mesh.vertices[vertexOffset + 1] = (float) height;
            mesh.vertices[vertexOffset + 2] = (float) localZ;
        }
    }

    // Generate Indices and Normals
    for (int localX = 0; localX < TERRAIN_CHUNK_SIZE; localX++) {
        for (int localZ = 0; localZ < TERRAIN_CHUNK_SIZE; localZ++) {
            int indexOffset = (localZ * TERRAIN_CHUNK_SIZE + localX) * 6;

            unsigned short i0 = (localZ + 0) * stride + (localX + 0);
            unsigned short i1 = (localZ + 0) * stride + (localX + 1);
            unsigned short i2 = (localZ + 1) * stride + (localX + 1);
            unsigned short i3 = (localZ + 1) * stride + (localX + 0);

            // Triangle 1
            mesh.indices[indexOffset + 0] = i0;
            mesh.indices[indexOffset + 1] = i1;
            mesh.indices[indexOffset + 2] = i2;

            // Triangle 2
            mesh.indices[indexOffset + 3] = i2;
            mesh.indices[indexOffset + 4] = i3;
            mesh.indices[indexOffset + 5] = i0;

            // Add face normals
            raylib::Vector3 v0 = { mesh.vertices[i0 * 3 + 0], mesh.vertices[i0 * 3 + 1], mesh.vertices[i0 * 3 + 2] };
            raylib::Vector3 v1 = { mesh.vertices[i1 * 3 + 0], mesh.vertices[i1 * 3 + 1], mesh.vertices[i1 * 3 + 2] };
            raylib::Vector3 v2 = { mesh.vertices[i2 * 3 + 0], mesh.vertices[i2 * 3 + 1], mesh.vertices[i2 * 3 + 2] };
            raylib::Vector3 v3 = { mesh.vertices[i3 * 3 + 0], mesh.vertices[i3 * 3 + 1], mesh.vertices[i3 * 3 + 2] };

            Vector3 n1 = (v1 - v0).CrossProduct(v2 - v0).Normalize();
            mesh.normals[i0 * 3 + 0] += n1.x; mesh.normals[i0 * 3 + 1] += n1.y; mesh.normals[i0 * 3 + 2] += n1.z;
            mesh.normals[i1 * 3 + 0] += n1.x; mesh.normals[i1 * 3 + 1] += n1.y; mesh.normals[i1 * 3 + 2] += n1.z;
            mesh.normals[i2 * 3 + 0] += n1.x; mesh.normals[i2 * 3 + 1] += n1.y; mesh.normals[i2 * 3 + 2] += n1.z;

            Vector3 n2 = (v3 - v2).CrossProduct(v0 - v2).Normalize();
            mesh.normals[i2 * 3 + 0] += n2.x; mesh.normals[i2 * 3 + 1] += n2.y; mesh.normals[i2 * 3 + 2] += n2.z;
            mesh.normals[i3 * 3 + 0] += n2.x; mesh.normals[i3 * 3 + 1] += n2.y; mesh.normals[i3 * 3 + 2] += n2.z;
            mesh.normals[i0 * 3 + 0] += n2.x; mesh.normals[i0 * 3 + 1] += n2.y; mesh.normals[i0 * 3 + 2] += n2.z;
        }
    }

    // Normalize normals
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
