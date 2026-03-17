#pragma once

#include <DataImage.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <dvk/dvk.hpp>

#include "Provider.hpp"

struct CubeMapCaptureUniforms {
    glm::mat4 viewProjMatrix;
};

struct ConvolutionUniforms {
    float roughness;
};

class CubeMapConverter {
public:
    CubeMapConverter();

    dvk::Texture::Ref ConvertEquirectangularToCubeMap(const dvk::Texture::Ref& equirectangularMap, const std::string& label = "CubeMapConverter ConvertEquirectangularToCubeMap Result");
    dvk::Texture::Ref GenerateIrradianceMap(const dvk::Texture::Ref& cubeMap, const std::string& label = "CubeMapConverter Irradiance Map");
    dvk::Texture::Ref GenerateRadianceMap(const dvk::Texture::Ref& cubeMap, const std::string& label = "CubeMapConverter Radiance Map");

private:
    dvk::GraphicsPipeline::Ref equirectangularToCubeMapShaderPipeline;
    dvk::GraphicsPipeline::Ref irradianceShaderPipeline;
    dvk::GraphicsPipeline::Ref radianceShaderPipeline;
    dvk::BindSetLayout::Ref perFaceBindSetLayout;
    dvk::BindSetLayout::Ref equirectangularToCubeMapPerMapBindSetLayout;
    dvk::BindSet::Ref equirectangularToCubeMapPerMapBindSet;
    dvk::BindSetLayout::Ref convolutionPerMapBindSetLayout;
    dvk::BindSet::Ref convolutionPerMapBindSet;
    dvk::Buffer::Ref positionBuffer;
    dvk::Buffer::Ref convolutionUniformBuffer;
    dvk::Sampler::Ref sampler;
    dvk::Buffer::Ref faceUniformBuffer;
    dvk::BindSet::Refs<6> faceBindSets;

    void capture(const dvk::BindSet::Ref& perMapBindSet, const dvk::Texture::Ref& target, dvk::GraphicsPipeline::Ref graphicsPipeline, int mipMap = 0, bool transitionLayoutStart = true, bool transitionLayoutEnd = true);
};

class EnvBrdfLutGenerator {
public:
    EnvBrdfLutGenerator();

    dvk::Texture::Ref GenerateBrdfLut(const std::string& label = "Environment BRDF LUT");

private:
    dvk::GraphicsPipeline::Ref graphicsPipeline;
};

struct SkyboxUniforms {
    glm::mat4 projMatrix;
    glm::mat4 viewMatrix;
};

class Skybox {
public:
    static int GetRadianceMipMapCount();

    explicit Skybox(dvk::Texture::Ref cubeMap, dvk::Texture::Ref irradianceMap, dvk::Texture::Ref radianceMap, dvk::Texture::Ref brdfLut);

    static Skybox FromEquirectangularMap(const dvk::Texture::Ref& equirectangularMap);
    static Skybox FromEquirectangularMap(const Data::Image& image);

    void Draw(glm::mat4 projMatrix, glm::mat4 viewMatrix, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);

    [[nodiscard]] dvk::Texture::Ref GetIrradianceMap() const;
    [[nodiscard]] dvk::Texture::Ref GetRadianceMap() const;
    [[nodiscard]] dvk::Texture::Ref GetBrdfLut() const;

private:
    dvk::GraphicsPipeline::Ref graphicsPipeline;
    dvk::BindSetLayout::Ref bindSetLayout;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> uniformBuffer;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> bindSet;
    dvk::Sampler::Ref sampler;
    dvk::TextureView::Ref cubeMapView;
    dvk::Texture::Ref irradianceMap;
    dvk::Texture::Ref radianceMap;
    dvk::Texture::Ref brdfLut;
    dvk::Buffer::Ref positionBuffer;
};
