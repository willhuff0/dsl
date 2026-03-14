#pragma once

#include "utils/GrowingBuffer.hpp"

#include <dvk/dvk.hpp>
#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

struct PartType {
    std::string displayName;
    float health;
    std::string shaderId;

    uint32_t indirectBufferOffset;
    GrowingBuffer instanceBuffer;
    uint32_t instanceCount;
};

struct PartInstance {
public:

private:
};

class ShaderInstance {
public:
    ShaderInstance();

    uint32_t AddDraw();

private:
    GrowingBuffer indirectBuffer;
    uint32_t drawCount;
};

class Vehicle {
public:
    Vehicle();

private:
    std::unordered_map<std::string, ShaderInstance> shaderInstances;
    std::unordered_map<std::string, PartType> partTypes;
    std::unordered_map<glm::ivec3, PartInstance> partInstances;
};
