#pragma once

#include "Node.hpp"
#include "Renderer.hpp"

#include <string>

class ModelLoader;

class Model {
    friend class ModelLoader;
public:
    Node::Ref GetRoot() const;

private:
    explicit Model(Node::Ref root);

    Node::Ref _root;
};

class ModelLoader {
public:
    ModelLoader(
        BindingProvider* cameraProvider,
        BindingProvider* environmentProvider,
        BindingProvider* directionalLightProvider);

    Model LoadFile(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const std::string& path);

private:
    Watcher _opaqueWatcher;
    ShaderIndex _opaque = NULL_INDEX;
    ShaderIndex _opaqueDoubleSided = NULL_INDEX;
    ShaderIndex _transparent = NULL_INDEX;
};
