#pragma once

#include <dvk/dvk.hpp>

class Terrain {
public:
    Terrain();

private:
    dvk::Buffer::Ref vertexBuffer;
    dvk::TextureView::Ref heightmapView;

    dvk::ComputePipeline::Ref generatorPipeline;
    dvk::BindSet::Ref generatorBindSet;

    dvk::GraphicsPipeline::Ref graphicsPipeline;
};