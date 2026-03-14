#include <Terrain.hpp>

#include "Provider.hpp"
#include "utils/Paths.hpp"
#include "TempImageLoader.hpp"
#include "TempModelLoader.hpp"

#include <string>

#include "dvk/CommandBuffer.hpp"

constexpr float METERS_PER_TEXEL = 1.0f;
constexpr uint32_t HEIGHTMAP_SIZE = 4096;
constexpr float MATERIAL_TILING = 0.125f;
constexpr float AMBIENT_INTENSITY = 2.0f;
constexpr float SNAPPING_INTERVAL = 1U << 5;

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
    for (uint32_t i = 0; i < 9; i++) {
        auto mesh = Mesh::LoadFile(TERRAIN_CLIPMAP_MESH_SHADER_PREFIX_PATH + std::to_string(i) + ".obj");
        vertexBuffer.push_back(dvk::Buffer::Create(GetDevice())
            .SetLabel("Terrain Clipmap Position Buffer " + std::to_string(i))
            .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
            .SetSize(mesh.positions.size() * sizeof(glm::vec2))
            .EnableWriteWithStaging()
            .Build());
        vertexBuffer[i]->WriteWithStaging(GetCommandEncoder(), mesh.positions.data());
        vertexCounts.push_back(mesh.positions.size());

        indexBuffer.push_back(dvk::Buffer::Create(GetDevice())
            .SetLabel("Terrain Clipmap Index Buffer " + std::to_string(i))
            .SetUsage(vk::BufferUsageFlagBits::eIndexBuffer)
            .SetSize(mesh.indices.size() * sizeof(uint32_t))
            .EnableWriteWithStaging()
            .Build());
        indexBuffer[i]->WriteWithStaging(GetCommandEncoder(), mesh.indices.data());
        indexCounts.push_back(mesh.indices.size());
    }

    heightMapView = dvk::TextureView::Create(GetDevice(), dvk::Texture::Create(GetDevice())
        .SetLabel("Terrain HeightMap")
        .SetUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled)
        .SetFormat(vk::Format::eR16Sfloat)
        .SetExtent({ HEIGHTMAP_SIZE, HEIGHTMAP_SIZE, 1 })
        .Build()).Build();
    normalMapView = dvk::TextureView::Create(GetDevice(), dvk::Texture::Create(GetDevice())
       .SetLabel("Terrain NormalMap")
       .SetUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled)
       .SetFormat(vk::Format::eR16G16B16A16Sfloat)
       .SetExtent({ HEIGHTMAP_SIZE, HEIGHTMAP_SIZE, 1 })
       .Build()).Build();

    generatorBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Terrain Generator")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eStorageImage)
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eStorageImage)
            .SetStage(vk::ShaderStageFlagBits::eCompute)
            .FinishEntry()
        .Build();

    generatorBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), generatorBindSetLayout, "Terrain Generator");
    dvk::Binder()
        .BindTexture(0)
            .SetTextureType(vk::DescriptorType::eStorageImage)
            .SetImageLayout(vk::ImageLayout::eGeneral)
            .SetTextureView(heightMapView)
            .FinishTexture()
        .BindTexture(1)
            .SetTextureType(vk::DescriptorType::eStorageImage)
            .SetImageLayout(vk::ImageLayout::eGeneral)
            .SetTextureView(normalMapView)
            .FinishTexture()
        .Update(generatorBindSet);

    generatorPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Terrain Generator")
        .AddBindSetLayout(generatorBindSetLayout)
        .Build();

    generatorPipelineWatcher = HotReload::CreateAndWatch(TERRAIN_GENERATOR_SHADER_PATH, [this]() {
        auto generatorShader = dvk::Shader::LoadFile(GetDevice(), TERRAIN_GENERATOR_SHADER_PATH, "Terrain Generator");
        generatorPositionsPipeline = dvk::ComputePipeline::Create(GetDevice(), generatorPipelineLayout)
            .SetLabel("Terrain Positions Generator")
            .SetStage(generatorShader, "GeneratePositions").FinishStage()
            .Build();
        generatorNormalsPipeline = dvk::ComputePipeline::Create(GetDevice(), generatorPipelineLayout)
            .SetLabel("Terrain Normals Generator")
            .SetStage(generatorShader, "GenerateNormals")
                .SetConstant(0, METERS_PER_TEXEL)
                .FinishStage()
            .Build();

        GetCommandEncoder().ExecuteAndWait([&] (const vk::raii::CommandBuffer& commandBuffer) {
            heightMapView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *generatorPositionsPipeline->GetPipeline());
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *generatorPositionsPipeline->GetLayout()->GetPipelineLayout(), 0, *generatorBindSet->GetDescriptorSet(), nullptr);
            commandBuffer.dispatch(HEIGHTMAP_SIZE / 32, HEIGHTMAP_SIZE / 32, 1);

            normalMapView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *generatorNormalsPipeline->GetPipeline());
            commandBuffer.dispatch(HEIGHTMAP_SIZE / 32, HEIGHTMAP_SIZE / 32, 1);

            heightMapView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);
            normalMapView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);
        });
    });

    graphicsBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Terrain Graphics")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eVertex)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(2)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(3)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry(4)
        .Build();

    sampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("Terrain Material Sampler")
        .SetAddressMode(vk::SamplerAddressMode::eRepeat)
        .SetAnisotropy(false, 1.0f)
        .SetLodBounds(0.0f, 32.0f)
        .SetBorderColor(vk::BorderColor::eFloatTransparentBlack)
        .Build();
    albedoTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain Albedo", LoadImage(TERRAIN_ALBEDO_TEXTURE_PATH, 4, Data::Image::ColorSpace::SRGB))).Build();
    metallicTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain Metallic", LoadImage(TERRAIN_METALLIC_TEXTURE_PATH, 1))).Build();
    roughnessTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain Roughness", LoadImage(TERRAIN_ROUGHNESS_TEXTURE_PATH, 1))).Build();
    aoTextureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage("Terrain AO", LoadImage(TERRAIN_AO_TEXTURE_PATH, 1))).Build();

    graphicsBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), graphicsBindSetLayout, "Terrain Graphics");
    dvk::Binder()
        .BindTexture(0).SetTextureView(heightMapView).FinishTexture()
        .BindTexture(1).SetTextureView(normalMapView).FinishTexture()
        .BindSampler(2).SetSampler(sampler).FinishSampler()
        .BindTexture(3).SetTextureView(albedoTextureView).FinishTexture()
        .BindTexture(4).SetTextureView(metallicTextureView).FinishTexture()
        .BindTexture(5).SetTextureView(roughnessTextureView).FinishTexture()
        .BindTexture(6).SetTextureView(aoTextureView).FinishTexture()
        .Update(graphicsBindSet);

    graphicsPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Terrain Graphics")
        .AddBindSetLayout(GetGlobalBindSetLayout())
        .AddBindSetLayout(graphicsBindSetLayout)
        .AddPushConstantRange()
            .SetStage(vk::ShaderStageFlagBits::eVertex)
            .SetSize(sizeof(LodConstants))
            .FinishPushConstantRange()
        .Build();

    graphicsPipelineWatcher = HotReload::CreateAndWatch(TERRAIN_SHADER_PATH, [this]() {
        auto graphicsShader = dvk::Shader::LoadFile(GetDevice(), TERRAIN_SHADER_PATH, "Terrain Graphics");
        graphicsPipeline = dvk::GraphicsPipeline::Create(GetDevice(), graphicsPipelineLayout)
            .SetLabel("Terrain Graphics")
            .SetVertexInput({ vk::VertexInputBindingDescription {
                .binding = 0,
                .stride = sizeof(glm::vec2),
                .inputRate = vk::VertexInputRate::eVertex,
            }}, {{ vk::VertexInputAttributeDescription {
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32Sfloat,
                .offset = 0,
            }}})
            .AddStage(vk::ShaderStageFlagBits::eVertex, graphicsShader, "vs_main")
                .SetConstant(0, HEIGHTMAP_SIZE)
                .SetConstant(1, METERS_PER_TEXEL)
                .FinishStage()
            .AddStage(vk::ShaderStageFlagBits::eFragment, graphicsShader, "fs_main")
                .SetConstant(2, MATERIAL_TILING)
                .SetConstant(3, AMBIENT_INTENSITY)
                .FinishStage()
            .SetSingleAttachment(GetRenderTextureFormat())
            .EnableDepth(GetDepthFormat())
            .EnableMultisample(GetMultisampleCount())
            .Build();
        graphicsWireframePipeline = dvk::GraphicsPipeline::Create(GetDevice(), graphicsPipelineLayout)
            .SetLabel("Terrain Graphics Wireframe")
            .SetVertexInput({ vk::VertexInputBindingDescription {
                .binding = 0,
                .stride = sizeof(glm::vec2),
                .inputRate = vk::VertexInputRate::eVertex,
            }}, {{ vk::VertexInputAttributeDescription {
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32Sfloat,
                .offset = 0,
            }}})
            .AddStage(vk::ShaderStageFlagBits::eVertex, graphicsShader, "vs_main")
                .SetConstant(0, HEIGHTMAP_SIZE)
                .SetConstant(1, METERS_PER_TEXEL)
                .FinishStage()
            .AddStage(vk::ShaderStageFlagBits::eFragment, graphicsShader, "fs_main")
                .SetConstant(2, MATERIAL_TILING)
                .SetConstant(3, AMBIENT_INTENSITY)
                .SetConstant(4, true)
                .FinishStage()
            .SetSingleAttachment(GetRenderTextureFormat())
            .EnableDepth(GetDepthFormat())
            .EnableMultisample(GetMultisampleCount())
            .Build();
    });

    auto graphicsNormalsShader = dvk::Shader::LoadFile(GetDevice(), TERRAIN_NORMALS_SHADER_PATH, "Terrain Graphics Normals");
    normalsPipeline = dvk::GraphicsPipeline::Create(GetDevice(), graphicsPipelineLayout)
        .SetLabel("Terrain Graphics Normals")
        .SetVertexInput({ vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = sizeof(glm::vec2),
            .inputRate = vk::VertexInputRate::eVertex,
        }}, {{ vk::VertexInputAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = 0,
        }}})
        .SetTopology(vk::PrimitiveTopology::ePointList)
        .AddStage(vk::ShaderStageFlagBits::eVertex, graphicsNormalsShader, "vs_main")
            .SetConstant(0, HEIGHTMAP_SIZE)
            .SetConstant(1, METERS_PER_TEXEL)
            .FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eGeometry, graphicsNormalsShader, "gs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, graphicsNormalsShader, "fs_main").FinishStage()
        .SetSingleAttachment(GetRenderTextureFormat())
        .EnableDepth(GetDepthFormat())
        .EnableMultisample(GetMultisampleCount())
        .Build();
}

void Terrain::Draw(GLFWwindow* window, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    auto toggleWireframeKey = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    if (toggleWireframeKey && !toggleWireframeKeyHeld) {
        useWireframe = !useWireframe;
    }
    toggleWireframeKeyHeld = toggleWireframeKey;

    auto toggleNormalsKey = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    if (toggleNormalsKey && !toggleNormalsKeyHeld) {
        useNormals = !useNormals;
    }
    toggleNormalsKeyHeld = toggleNormalsKey;

    auto& pipeline = useWireframe ? graphicsWireframePipeline : graphicsPipeline;

    constexpr std::array lodRanges = {
        24.0f, 44.0f, 80.0f, 144.0f, 256.0f, 448.0f, 768.0f, 1280.0f, 2048.0f,
    };
    // constexpr std::array lodRanges = {
    //     64.0f, 128.0f, 256.0f, 512.0f, 1024.0f, 2048.0f,
    // };

    for (uint32_t i = 0; i < 9; i++) {
        commandBuffer.SetGraphicsPipeline(pipeline);
        commandBuffer.PushConstant(0, vk::ShaderStageFlagBits::eVertex, LodConstants {
            .lod = i,
            .range = lodRanges[i],
        });
        commandBuffer.SetVertexBuffer(0, vertexBuffer[i]);
        commandBuffer.SetIndexBuffer(indexBuffer[i], vk::IndexType::eUint32);
        commandBuffer.SetBindSet(1, graphicsBindSet);
        commandBuffer.DrawIndexed(indexCounts[i]);

        if (useNormals) {
            commandBuffer.SetGraphicsPipeline(normalsPipeline);
            commandBuffer.Draw(indexCounts[i]);
        }
    }
}
