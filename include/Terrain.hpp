#pragma once

#include "Utils/RNG.hpp"

#include <cstdint>
#include <utility>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <raylib-cpp.hpp>

#define TERRAIN_CHUNK_SIZE 32
#define TERRAIN_VIEW_DISTANCE_CHUNKS 4
#define TERRAIN_CHUNK_FACE_COUNT (TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 2)

struct GridPos {
    int x, z;

    bool operator==(const GridPos& other) const {
        return x == other.x && z == other.z;
    }
};
namespace std {
    template<> struct hash<GridPos> {
        std::size_t operator() (const GridPos& chunkPos) const {
            auto h1 = std::hash<int>{}(chunkPos.x);
            auto h2 = std::hash<int>{}(chunkPos.z);
            return h1 ^ (h2 << 1);
        }
    };
}

class Chunk {
public:
    Chunk(raylib::Mesh&& mesh, GridPos pos);
    ~Chunk();

    void Load();
    void Unload();
    void Draw(const Material& material);

private:
    bool isLoaded = true;
    raylib::Mesh mesh;
    raylib::Matrix transform;
};

class Terrain {
public:
    explicit Terrain(int seed);

    void Tick(Vector3 playerPos);
    void Draw();

private:
    void generateChunk(GridPos pos);

    int seed;

    raylib::Shader shader;
    raylib::Material material;
    raylib::Material redMaterial;

    std::unordered_map<GridPos, Chunk> chunks;
    std::unordered_set<GridPos> visibleChunks;

    GridPos lastPlayerPos;
};