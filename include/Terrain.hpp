#pragma once

#include "HotReload.hpp"

#include <dvk/dvk.hpp>

#include <GLFW/glfw3.h>

struct LodConstants {
    uint32_t lod;
    float range;
};

class Terrain {
public:
    Terrain();

    void Draw(GLFWwindow* window, const vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex);

private:
    std::vector<dvk::Buffer::Ref> vertexBuffer;
    std::vector<dvk::Buffer::Ref> indexBuffer;
    dvk::TextureView::Ref heightMapView;
    dvk::TextureView::Ref normalMapView;

    Watcher generatorPipelineWatcher;
    dvk::PipelineLayout::Ref generatorPipelineLayout;
    dvk::ComputePipeline::Ref generatorPositionsPipeline;
    dvk::ComputePipeline::Ref generatorNormalsPipeline;
    dvk::BindSetLayout::Ref generatorBindSetLayout;
    dvk::BindSet::Ref generatorBindSet;

    dvk::Sampler::Ref sampler;
    dvk::TextureView::Ref albedoTextureView;
    dvk::TextureView::Ref metallicTextureView;
    dvk::TextureView::Ref roughnessTextureView;
    dvk::TextureView::Ref aoTextureView;

    Watcher graphicsPipelineWatcher;
    dvk::PipelineLayout::Ref graphicsPipelineLayout;
    dvk::GraphicsPipeline::Ref graphicsPipeline;
    dvk::GraphicsPipeline::Ref graphicsWireframePipeline;
    dvk::BindSetLayout::Ref graphicsBindSetLayout;
    dvk::BindSet::Ref graphicsBindSet;

    dvk::GraphicsPipeline::Ref normalsPipeline;

    std::vector<uint32_t> vertexCounts;
    std::vector<uint32_t> indexCounts;

    bool toggleWireframeKeyHeld = false;
    bool useWireframe = false;

    bool toggleNormalsKeyHeld = false;
    bool useNormals = false;
};