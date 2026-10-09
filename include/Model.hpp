#pragma once

#include "Node.hpp"
#include "Renderer.hpp"

#include <string>

struct Model {
    static Model LoadFile(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const std::string& path);
    static void FreeShaders();

    Node::Ref GetRoot() const;

private:
    explicit Model(Node::Ref root);

    Node::Ref _root;
};
