#include "Terrain.hpp"

#include <stdexcept>
#include <raymath.h>
#include <Deps/FastNoiseLite.h>

//#region Chunk

Chunk::Chunk(Mesh mesh, GridPos pos) : mesh(mesh) {
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
    DrawMesh(mesh, material, transform);
}

//#endregion

Terrain::Terrain(int seed) : seed(seed) {
    material = LoadMaterialDefault();
}

void Terrain::Tick(Vector3 playerPos) {
    GridPos playerChunkPos = {
        .x = (int)playerPos.x / TERRAIN_CHUNK_SIZE,
        .z = (int)playerPos.z / TERRAIN_CHUNK_SIZE,
    };

    std::unordered_set<GridPos> chunksToUnload(visibleChunks);
    std::unordered_set<GridPos> chunksToLoad;
    for (int x = playerChunkPos.x - TERRAIN_VIEW_DISTANCE_CHUNKS; x < playerChunkPos.x + TERRAIN_VIEW_DISTANCE_CHUNKS; x++) {
        for (int z = playerChunkPos.z - TERRAIN_VIEW_DISTANCE_CHUNKS; z < playerChunkPos.z + TERRAIN_VIEW_DISTANCE_CHUNKS; z++) {
            if (x * x + z * z > TERRAIN_VIEW_DISTANCE_CHUNKS * TERRAIN_VIEW_DISTANCE_CHUNKS) continue;

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
    Mesh mesh = {0};

    mesh.vertexCount = (TERRAIN_CHUNK_SIZE + 1) * (TERRAIN_CHUNK_SIZE + 1);
    mesh.triangleCount = TERRAIN_CHUNK_FACE_COUNT;
    mesh.vertices = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
    mesh.indices = (unsigned short*)RL_MALLOC(TERRAIN_CHUNK_FACE_COUNT * 3 * sizeof(unsigned short));

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    noise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    noise.SetSeed(seed);

    // Generate Vertices and Indices
    for (int localX = 0; localX <= TERRAIN_CHUNK_SIZE; localX++) {
        for (int localZ = 0; localZ <= TERRAIN_CHUNK_SIZE; localZ++) {
            int x = pos.x * TERRAIN_CHUNK_SIZE + localX;
            int z = pos.z * TERRAIN_CHUNK_SIZE + localZ;

            float height = noise.GetNoise((float) x, (float) z);

            int vertexOffset = (localX * TERRAIN_CHUNK_SIZE + localZ) * 3;
            mesh.vertices[vertexOffset + 0] = (float) x;
            mesh.vertices[vertexOffset + 1] = (float) height;
            mesh.vertices[vertexOffset + 2] = (float) z;

            if (localX != TERRAIN_CHUNK_SIZE && localZ != TERRAIN_CHUNK_SIZE) {
                int indexOffset = (localX * TERRAIN_CHUNK_SIZE + localZ) * 6;

                mesh.indices[indexOffset + 0] = (localX + 0) * TERRAIN_CHUNK_SIZE + (localZ + 0);
                mesh.indices[indexOffset + 1] = (localX + 1) * TERRAIN_CHUNK_SIZE + (localZ + 0);
                mesh.indices[indexOffset + 2] = (localX + 1) * TERRAIN_CHUNK_SIZE + (localZ + 1);

                mesh.indices[indexOffset + 3] = (localX + 1) * TERRAIN_CHUNK_SIZE + (localZ + 1);
                mesh.indices[indexOffset + 4] = (localX + 0) * TERRAIN_CHUNK_SIZE + (localZ + 1);
                mesh.indices[indexOffset + 5] = (localX + 0) * TERRAIN_CHUNK_SIZE + (localZ + 0);
            }
        }
    }

    // Generate Normals
    for (int localX = 0; localX < TERRAIN_CHUNK_SIZE; localX++) {
        for (int localZ = 0; localZ < TERRAIN_CHUNK_SIZE; localZ++) {
            {
                int v0Offset = ((localX + 0) * TERRAIN_CHUNK_SIZE + (localZ + 0)) * 3;
                int v1Offset = ((localX + 1) * TERRAIN_CHUNK_SIZE + (localZ + 0)) * 3;
                int v2Offset = ((localX + 1) * TERRAIN_CHUNK_SIZE + (localZ + 1)) * 3;

                Vector3 v0 = { mesh.vertices[v0Offset + 0], mesh.vertices[v0Offset + 1], mesh.vertices[v0Offset + 2] };
                Vector3 v1 = { mesh.vertices[v1Offset + 0], mesh.vertices[v1Offset + 1], mesh.vertices[v1Offset + 2] };
                Vector3 v2 = { mesh.vertices[v2Offset + 0], mesh.vertices[v2Offset + 1], mesh.vertices[v2Offset + 2] };
                Vector3 normal = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v2, v0), Vector3Subtract(v1, v0)));

                mesh.normals[v0Offset + 0] = normal.x;
                mesh.normals[v0Offset + 1] = normal.y;
                mesh.normals[v0Offset + 2] = normal.z;

                mesh.normals[v1Offset + 0] = normal.x;
                mesh.normals[v1Offset + 1] = normal.y;
                mesh.normals[v1Offset + 2] = normal.z;

                mesh.normals[v2Offset + 0] = normal.x;
                mesh.normals[v2Offset + 1] = normal.y;
                mesh.normals[v2Offset + 2] = normal.z;
            }

            {
                int v0Offset = ((localX + 1) * TERRAIN_CHUNK_SIZE + (localZ + 1)) * 3;
                int v1Offset = ((localX + 0) * TERRAIN_CHUNK_SIZE + (localZ + 1)) * 3;
                int v2Offset = ((localX + 0) * TERRAIN_CHUNK_SIZE + (localZ + 0)) * 3;

                Vector3 v0 = { mesh.vertices[v0Offset + 0], mesh.vertices[v0Offset + 1], mesh.vertices[v0Offset + 2] };
                Vector3 v1 = { mesh.vertices[v1Offset + 0], mesh.vertices[v1Offset + 1], mesh.vertices[v1Offset + 2] };
                Vector3 v2 = { mesh.vertices[v2Offset + 0], mesh.vertices[v2Offset + 1], mesh.vertices[v2Offset + 2] };
                Vector3 normal = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v2, v0), Vector3Subtract(v1, v0)));

                mesh.normals[v0Offset + 0] = normal.x;
                mesh.normals[v0Offset + 1] = normal.y;
                mesh.normals[v0Offset + 2] = normal.z;

                mesh.normals[v1Offset + 0] = normal.x;
                mesh.normals[v1Offset + 1] = normal.y;
                mesh.normals[v1Offset + 2] = normal.z;

                mesh.normals[v2Offset + 0] = normal.x;
                mesh.normals[v2Offset + 1] = normal.y;
                mesh.normals[v2Offset + 2] = normal.z;
            }
        }
    }

    //UploadMesh(&mesh, false);

    mesh = GenMeshCube(10, 10, 10);

    chunks.emplace(std::piecewise_construct, std::forward_as_tuple(pos), std::forward_as_tuple(mesh, pos));
}
