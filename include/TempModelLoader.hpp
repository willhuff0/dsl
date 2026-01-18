#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <string>

struct Mesh {
    std::vector<glm::vec2> positions;
    std::vector<uint32_t> indices;

    static Mesh LoadFile(const std::string& path);
};