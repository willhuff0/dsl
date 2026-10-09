#pragma once

#include <DataImage.hpp>

#include "Provider.hpp"
#include "ShaderHotReload.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <dvk/dvk.hpp>

struct CubeMapCaptureUniforms {
    glm::mat4 viewProjMatrix;
};

struct ConvolutionUniforms {
    float roughness;
    float resolution;
};

class CubeMapConverter;

// Mirrors the Dart `CubeMap`: owns a cube-map texture plus its metadata and can
// derive irradiance / radiance maps from itself using a shared CubeMapConverter.
class CubeMap {
public:
    CubeMap() = default;
    CubeMap(dvk::Texture::Ref texture, uint32_t faceSize, uint32_t mipLevelCount);

    static CubeMap FromEquirectangular(CubeMapConverter& converter, const dvk::Texture::Ref& equirectangularMap, const std::string& label = "Environment CubeMap");

    [[nodiscard]] CubeMap GenerateIrradianceMap(CubeMapConverter& converter, const std::string& label = "Irradiance CubeMap") const;
    [[nodiscard]] CubeMap GenerateRadianceMap(CubeMapConverter& converter, const std::string& label = "Radiance CubeMap") const;

    [[nodiscard]] const dvk::Texture::Ref& GetTexture() const { return texture; }
    [[nodiscard]] uint32_t GetFaceSize() const { return faceSize; }
    [[nodiscard]] uint32_t GetMipLevelCount() const { return mipLevelCount; }

    [[nodiscard]] dvk::TextureView::Ref CreateCubeView(const std::string& label = "CubeMap Cube View") const;

private:
    dvk::Texture::Ref texture;
    uint32_t faceSize = 0;
    uint32_t mipLevelCount = 1;
};

// Shared machinery (pipelines, per-face capture uniforms, sampler) used to render
// into cube-map faces. Equivalent to the Dart `CubeMap` static shader/framebuffer state.
class CubeMapConverter {
public:
    CubeMapConverter();

    CubeMap ConvertEquirectangularToCubeMap(const dvk::Texture::Ref& equirectangularMap, const std::string& label);
    CubeMap GenerateIrradianceMap(const CubeMap& cubeMap, const std::string& label);
    CubeMap GenerateRadianceMap(const CubeMap& cubeMap, const std::string& label);

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
    void generateCubeMipmaps(const dvk::Texture::Ref& target, uint32_t faceSize, uint32_t mipLevelCount);
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

    Skybox(CubeMap environmentMap, CubeMap irradianceMap, CubeMap radianceMap, dvk::Texture::Ref brdfLut);

    static Skybox FromEquirectangularMap(const dvk::Texture::Ref& equirectangularMap);
    static Skybox FromEquirectangularMap(const Data::Image& image);

    void Draw(dvk::CommandBuffer& commandBuffer);

    [[nodiscard]] dvk::Texture::Ref GetEnvironmentMap() const;
    [[nodiscard]] dvk::Texture::Ref GetIrradianceMap() const;
    [[nodiscard]] dvk::Texture::Ref GetRadianceMap() const;
    [[nodiscard]] dvk::Texture::Ref GetBrdfLut() const;

private:
    CubeMap environmentMap;
    CubeMap irradianceMap;
    CubeMap radianceMap;
    dvk::Texture::Ref brdfLut;

    Watcher graphicsPipelineWatcher;
    dvk::GraphicsPipeline::Ref graphicsPipeline;
    dvk::Buffer::Ref positionBuffer;
};
