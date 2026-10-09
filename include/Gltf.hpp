#pragma once

#include "Node.hpp"

#include <dvk/dvk.hpp>

#include <string>

class Gltf {
public:
    static Gltf Load(const std::string& path, const std::string& sceneName = "");

private:
    Gltf(std::string copyright, std::string generator);

    Node::Ref _root;
    std::string _copyright;
    std::string _generator;
};