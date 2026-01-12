#include <Terrain.hpp>

#include "Provider.hpp"
#include "utils/Paths.hpp"

constexpr float METERS_PER_TEXEL = 0.5f;
constexpr uint32_t HEIGHTMAP_SIZE = 4096;

struct Vertex {
    float x, z;

    static std::vector<vk::VertexInputBindingDescription> GetBindingDescription() { return {
        vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex,
        },
    }; }
    static std::vector<vk::VertexInputAttributeDescription> GetAttributeDescription() { return {
        vk::VertexInputAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = 0,
        },
    }; }
};

Terrain::Terrain() {
    vertexBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Terrain Clipmap Position Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .SetSize(0)
        .EnableWriteWithStaging()
        .Build();

    heightmapView = dvk::TextureView::Create(GetDevice(), dvk::Texture::Create(GetDevice())
        .SetLabel("Terrain Heightmap")
        .SetUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled)
        .SetFormat(vk::Format::eR16Sfloat)
        .SetExtent({ HEIGHTMAP_SIZE, HEIGHTMAP_SIZE, 1 })
        .Build()).Build();
    GetCommandEncoder().ExecuteOneTime([&] (const vk::raii::CommandBuffer& commandBuffer) {
        heightmapView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    });

    // auto generatorBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
    //     .SetLabel("Terrain Generator")
    //     .AddEntry(0)
    //         .SetType(vk::DescriptorType::eStorageImage)
    //         .SetStage(vk::ShaderStageFlagBits::eCompute)
    //         .FinishEntry()
    //     .Build();
    //
    // auto generatorPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
    //     .SetLabel("Terrain Generator")
    //     .AddBindSetLayout(generatorBindSetLayout)
    //     .Build();
    //
    // auto generatorShader = dvk::Shader::LoadFile(GetDevice(), TERRAIN_GENERATOR_SHADER_PATH, "Terrain Generator");
    // generatorPipeline = dvk::ComputePipeline::Create(GetDevice(), generatorPipelineLayout)
    //     .SetLabel("Terrain Generator")
    //     .SetStage(generatorShader, "cs_main").FinishStage()
    //     .Build();
    //
    // generatorBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), generatorBindSetLayout, "Terrain Generator");
    // dvk::Binder()
    //     .BindTexture(0)
    //         .SetTextureType(vk::DescriptorType::eStorageImage)
    //         .SetImageLayout(vk::ImageLayout::eGeneral)
    //         .SetTextureView(heightmapView)
    //         .FinishTexture()
    //     .Update(generatorBindSet);

    // GetCommandEncoder().ExecuteOneTime([&] (const vk::raii::CommandBuffer& commandBuffer) {
    //     heightmapView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    //
    //     commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *generatorPipeline->GetPipeline());
    //     commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *generatorPipeline->GetLayout()->GetPipelineLayout(), 0, *generatorBindSet->GetDescriptorSet(), nullptr);
    //     commandBuffer.dispatch(HEIGHTMAP_SIZE / 32, HEIGHTMAP_SIZE / 32, 1);
    // });
}
