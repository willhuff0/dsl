#include <MeshRenderer.hpp>

#include <Provider.hpp>

static constexpr uint64_t BufferStartSize = 1024;

MeshRenderer::MeshRenderer()
    : positionBuffer(
          "Mesh Renderer Position Buffer",
          vk::BufferUsageFlagBits::eVertexBuffer,
          vk::PipelineStageFlagBits2::eVertexShader,
          BufferStartSize
      ),
      normalBuffer(
          "Mesh Renderer Normal Buffer",
          vk::BufferUsageFlagBits::eVertexBuffer,
          vk::PipelineStageFlagBits2::eVertexShader,
          BufferStartSize
      ),
      tangentBuffer(
          "Mesh Renderer Tangent Buffer",
          vk::BufferUsageFlagBits::eVertexBuffer,
          vk::PipelineStageFlagBits2::eVertexShader,
          BufferStartSize
      ),
      uvBuffer(
          "Mesh Renderer UV Buffer",
          vk::BufferUsageFlagBits::eVertexBuffer,
          vk::PipelineStageFlagBits2::eVertexShader,
          BufferStartSize
      ),
      indexBuffer(
          "Mesh Renderer Index Buffer",
          vk::BufferUsageFlagBits::eIndexBuffer,
          vk::PipelineStageFlagBits2::eVertexShader,
          BufferStartSize
      ),
      indirectArgumentBuffer(
          "Mesh Renderer Indirect Argument Buffer",
          vk::BufferUsageFlagBits::eIndirectBuffer,
          vk::PipelineStageFlagBits2::eDrawIndirect,
          BufferStartSize
      ),
      instanceBuffer(
          "Mesh Renderer Instance Buffer",
          vk::BufferUsageFlagBits::eUniformBuffer,
          vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader,
          BufferStartSize
      ) {
}

void MeshRenderer::UpdateBindings() {
    auto bindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Mesh Renderer")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry(5)
        .Build();



}

uint32_t MeshRenderer::LoadMesh(const std::string& path, uint32_t materialId) {
    // TODO
    return meshes.size() - 1;
}

void MeshRenderer::Draw(dvk::CommandBuffer& commandBuffer) {
    commandBuffer.SetGraphicsPipeline(graphicsPipeline);
    commandBuffer.SetBindSet(1, bindSet);
    commandBuffer.DrawIndexedIndirect(indirectArgumentBuffer.GetBuffer(), meshes.size());
}
