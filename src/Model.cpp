#include <Model.hpp>

#include "TempImageLoader.hpp"
#include "components/InstanceComponent.hpp"
#include "utils/Paths.hpp"
#include "ShaderHotReload.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>

#include <stdexcept>
#include <filesystem>
#include <ranges>
#include <utility>
#include <vector>

static constexpr uint32_t IMPORT_FLAGS =
    aiProcess_Triangulate |
    aiProcess_GenNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_JoinIdenticalVertices |
    aiProcess_OptimizeMeshes |
    aiProcess_OptimizeGraph |
    aiProcess_ImproveCacheLocality |
    aiProcess_FindInvalidData |
    aiProcess_FindDegenerates;

using AiTextureIndex = uint32_t;
using AiMaterialIndex = uint32_t;
using AiMeshIndex = uint32_t;

namespace {
    struct ModelLoadingContext {
        dvk::CommandBuffer& commandBuffer;
        uint32_t frameIndex;
        const aiScene* scene;
        const std::filesystem::path& dir;
        std::unordered_map<AiTextureIndex, TextureIndex>& embeddedTextures;
        std::unordered_map<std::string, TextureIndex>& externalTextures;
        std::unordered_map<AiMaterialIndex, std::pair<MaterialIndex, ShaderIndex>>& materials;
        std::unordered_map<AiMeshIndex, MeshIndex>& meshes;
        ShaderIndex opaqueShader;
        ShaderIndex opaqueDoubleSidedShader;
        ShaderIndex transparentShader;
    };

    struct Material {
        glm::vec4 baseColorFactor;
        TextureIndex baseColorTexture;
        uint32_t baseColorUvIndex;

        TextureIndex metallicTexture;
        uint32_t metallicUvIndex;
        float metallicFactor;

        TextureIndex roughnessTexture;
        uint32_t roughnessUvIndex;
        float roughnessFactor;

        TextureIndex normalTexture;
        uint32_t normalUvIndex;
        float normalScale;

        TextureIndex occlusionTexture;
        uint32_t occlusionUvIndex;
        float occlusionStrength;

        TextureIndex emissiveTexture;
        uint32_t emissiveUvIndex;
        glm::vec3 emissiveFactor;

        float alphaCutoff;
    };
}

Node::Ref Model::GetRoot() const {
    return _root;
}

Model::Model(Node::Ref root) : _root(std::move(root)) { }

ModelLoader::ModelLoader(
    BindingProvider* cameraProvider,
    BindingProvider* environmentProvider,
    BindingProvider* directionalLightProvider
) {
    auto layout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("[Model]")
        .AddBindSetLayout(GetRenderer().GetStaticBindSetLayout())
        .AddBindSetLayout(GetRenderer().GetPerFrameBindSetLayout())
        .AddBindSetLayout(cameraProvider->GetLayout())
        .AddBindSetLayout(environmentProvider->GetLayout())
        .AddBindSetLayout(directionalLightProvider->GetLayout())
        .AddBindSetLayout(GetRenderer().GetTexturesBindSetLayout())
        .AddBindSetLayout(GetRenderer().GetShaderPassBindSetLayout())
        .Build();

    _opaqueWatcher = ShaderHotReload::CreateAndWatch(0, MODEL_SHADER_PATH, [this, layout](uint32_t frameIndex, const auto& path) {
        auto shader = dvk::Shader::LoadFile(GetDevice(), path, "[Model] Opaque");
        auto opaquePipeline = dvk::GraphicsPipeline::Create(GetDevice(), layout)
            .SetLabel("[Model] Opaque")
            .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
            .SetRasterizer()
                .SetCullMode(vk::CullModeFlagBits::eBack)
                .FinishRasterizer()
            .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "VertexMain").FinishStage()
            .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "FragmentMain")
                .SetConstant(0, false)
                .SetConstant(0, false)
                .FinishStage()
            .SetSingleAttachment(GetRenderTextureFormat())
            .EnableDepth(GetDepthFormat())
            .EnableMultisample(GetMultisampleCount())
            .Build();
        auto opaqueDoubleSidedPipeline = dvk::GraphicsPipeline::Create(GetDevice(), layout)
            .SetLabel("[Model] Opaque Double-Sided")
            .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
            .SetRasterizer()
                .SetCullMode(vk::CullModeFlagBits::eNone)
                .FinishRasterizer()
            .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "VertexMain").FinishStage()
            .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "FragmentMain")
                .SetConstant(0, true)
                .SetConstant(1, false)
                .FinishStage()
            .SetSingleAttachment(GetRenderTextureFormat())
            .EnableDepth(GetDepthFormat())
            .EnableMultisample(GetMultisampleCount())
            .Build();
        auto transparentPipeline = dvk::GraphicsPipeline::Create(GetDevice(), layout)
            .SetLabel("[Model] Transparent")
            .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
            .SetRasterizer()
                .SetCullMode(vk::CullModeFlagBits::eNone)
                .FinishRasterizer()
            .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "VertexMain").FinishStage()
            .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "FragmentMain")
                .SetConstant(0, true)
                .SetConstant(1, true)
                .FinishStage()
            .AddAttachment()
                .SetFormat(GetRenderTextureFormat())
                .EnableBlending()
                .SetColorBlend(vk::BlendOp::eAdd, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha)
                .SetAlphaBlend(vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero)
                .FinishAttachment()
            .SetDepth()
                .SetFormat(GetDepthFormat())
                .EnableDepthTest()
                .FinishDepth()
            .EnableMultisample(GetMultisampleCount())
            .Build();

        if (_opaque == NULL_INDEX) {
            _opaque = GetRenderer().AddShader(opaquePipeline, false, "[Model] Opaque");
        } else {
            GetRenderer().SetShaderPipeline(frameIndex, _opaque, opaquePipeline);
        }

        if (_opaqueDoubleSided == NULL_INDEX) {
            _opaqueDoubleSided = GetRenderer().AddShader(opaqueDoubleSidedPipeline, false, "[Model] Opaque Double-Sided");
        } else {
            GetRenderer().SetShaderPipeline(frameIndex, _opaqueDoubleSided, opaqueDoubleSidedPipeline);
        }

        if (_transparent == NULL_INDEX) {
            _transparent = GetRenderer().AddShader(transparentPipeline, true, "[Model] Transparent");
        } else {
            GetRenderer().SetShaderPipeline(frameIndex, _transparent, transparentPipeline);
        }
    });
}

static Data::Image getImageFromUncompressed(
    const unsigned char *data,
    unsigned int width,
    unsigned int height,
    int channels
) {
    Data::Image image;

    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.bytesPerSubpixel = 1;
    image.channels = channels;

    size_t size = width * height * image.channels * image.bytesPerSubpixel;

    auto* rgbaData = new unsigned char[size];

    // Convert from ARGB to RGBA
    for (unsigned int i = 0; i < width * height; i++) {
        unsigned int srcIdx = i * 4;
        unsigned int dstIdx = i * channels;

        unsigned char alpha = data[srcIdx + 0];
        unsigned char red   = data[srcIdx + 1];
        unsigned char green = data[srcIdx + 2];
        unsigned char blue  = data[srcIdx + 3];

        rgbaData[dstIdx + 0] = red;
        if (channels > 1) rgbaData[dstIdx + 1] = green;
        if (channels > 2) rgbaData[dstIdx + 2] = blue;
        if (channels > 3) rgbaData[dstIdx + 3] = alpha;
    }

    image.data = std::shared_ptr<unsigned char>(rgbaData, [](const unsigned char* ptr) { delete[] ptr; });

    image.size = size;
    image.compressionMode = Data::Image::CompressionMode::Uncompressed;
    image.colorSpace = Data::Image::ColorSpace::Linear;

    return image;
}

static TextureIndex loadEmbeddedTexture(
    ModelLoadingContext& context,
    const std::string& label,
    AiTextureIndex aiTextureIndex,
    int desiredChannelCount,
    Data::Image::ColorSpace colorSpace
) {
    {
        auto textureIter = context.embeddedTextures.find(aiTextureIndex);
        if (textureIter != context.embeddedTextures.end()) return textureIter->second;
    }

    const aiTexture* texture = context.scene->mTextures[aiTextureIndex];
    Data::Image image;
    if (texture->mHeight == 0) {
        // Compressed
        image = LoadImage(
            reinterpret_cast<unsigned char*>(texture->pcData),
            static_cast<int>(texture->mWidth),
            desiredChannelCount,
            colorSpace);
    } else {
        // Uncompressed
        image = getImageFromUncompressed(
            reinterpret_cast<unsigned char*>(texture->pcData),
            texture->mWidth,
            texture->mHeight,
            desiredChannelCount);
    }

    auto textureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage(label, image, true)).Build();
    auto rendererTextureIndex = GetRenderer().AddTexture(textureView);
    context.embeddedTextures.emplace(aiTextureIndex, rendererTextureIndex);
    return rendererTextureIndex;
}

static TextureIndex loadExternalTexture(
    ModelLoadingContext& context,
    const std::string& label,
    const std::string& path,
    int desiredChannelCount,
    Data::Image::ColorSpace colorSpace
) {
    {
        auto textureIter = context.externalTextures.find(path);
        if (textureIter != context.externalTextures.end()) return textureIter->second;
    }

    auto image = LoadImage(
        path,
        desiredChannelCount,
        colorSpace);

    auto textureView = dvk::TextureView::Create(GetDevice(), CreateTextureFromImage(label, image, true)).Build();
    auto rendererTextureIndex = GetRenderer().AddTexture(textureView);
    context.externalTextures.emplace(path, rendererTextureIndex);
    return rendererTextureIndex;
}

static TextureIndex loadMaterialTexture(
    ModelLoadingContext& context,
    const std::string& label,
    aiTextureType aiType,
    const aiMaterial* aiMaterial,
    int desiredChannelCount,
    Data::Image::ColorSpace colorSpace
) {
    if (aiMaterial->GetTextureCount(aiType) == 0) {
        return NULL_INDEX;
    }

    aiString path;
    if (aiMaterial->GetTexture(aiType, 0, &path) != aiReturn_SUCCESS) {
        return NULL_INDEX;
    }

    if (path.data[0] == '*') {
        return loadEmbeddedTexture(context, label, std::stol(&path.data[1]), desiredChannelCount, colorSpace);
    } else {
        std::filesystem::path absolutePath(context.dir);
        absolutePath.append(path.C_Str());
        return loadExternalTexture(context, label, absolutePath.string(), desiredChannelCount, colorSpace);
    }
}

static std::pair<MaterialIndex, ShaderIndex> loadMaterial(
    ModelLoadingContext& context,
    AiMaterialIndex aiMaterialIndex
) {
    {
        auto materialIter = context.materials.find(aiMaterialIndex);
        if (materialIter != context.materials.end()) return materialIter->second;
    }

    aiMaterial* material = context.scene->mMaterials[aiMaterialIndex];

    aiColor4D baseColorFactor(1.0f, 1.0f, 1.0f, 1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    aiColor3D emissiveFactor(1.0f, 1.0f, 1.0f);
    material->Get(AI_MATKEY_BASE_COLOR, baseColorFactor);
    material->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor);
    material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor);
    material->Get(AI_MATKEY_TEXBLEND(aiTextureType_NORMALS, 0), normalScale);
    material->Get(AI_MATKEY_TEXBLEND(aiTextureType_AMBIENT_OCCLUSION, 0), occlusionStrength);
    material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveFactor);

    bool isDoubleSided = false;
    material->Get(AI_MATKEY_TWOSIDED, isDoubleSided);

    float alphaCutoff = 0.5f;
    material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);

    bool isTransparent = false;
    aiString aiAlphaMode;
    material->Get(AI_MATKEY_GLTF_ALPHAMODE, aiAlphaMode);
    std::string alphaMode(aiAlphaMode.C_Str());
    if (alphaMode == "OPAQUE") {
        alphaCutoff = 2.0f;
    } else if (alphaMode == "BLEND") {
        isTransparent = true;
    }

    auto baseColor = loadMaterialTexture(context, "[Model] Base Color " + std::to_string(aiMaterialIndex), aiTextureType_BASE_COLOR, material, 0, Data::Image::ColorSpace::SRGB);
    auto metallic = loadMaterialTexture(context, "[Model] Metallic " + std::to_string(aiMaterialIndex), aiTextureType_METALNESS, material, 0, Data::Image::ColorSpace::Linear);
    auto roughness = loadMaterialTexture(context, "[Model] Roughness " + std::to_string(aiMaterialIndex), aiTextureType_DIFFUSE_ROUGHNESS, material, 0, Data::Image::ColorSpace::Linear);
    auto normal = loadMaterialTexture(context, "[Model] Normal " + std::to_string(aiMaterialIndex), aiTextureType_NORMALS, material, 0, Data::Image::ColorSpace::Linear);
    auto occlusion = loadMaterialTexture(context, "[Model] Occlusion " + std::to_string(aiMaterialIndex), aiTextureType_LIGHTMAP, material, 0, Data::Image::ColorSpace::Linear);
    auto emissive = loadMaterialTexture(context, "[Model] Emissive " + std::to_string(aiMaterialIndex), aiTextureType_EMISSIVE, material, 0, Data::Image::ColorSpace::SRGB);

    int baseColorUvIndex = 0;
    int metallicUvIndex = 0;
    int roughnessUvIndex = 0;
    int normalUvIndex = 0;
    int occlusionUvIndex = 0;
    int emissiveUvIndex = 0;
    material->Get(AI_MATKEY_UVWSRC(aiTextureType_BASE_COLOR, 0), baseColorUvIndex);
    material->Get(AI_MATKEY_UVWSRC(aiTextureType_METALNESS, 0), metallicUvIndex);
    material->Get(AI_MATKEY_UVWSRC(aiTextureType_DIFFUSE_ROUGHNESS, 0), roughnessUvIndex);
    material->Get(AI_MATKEY_UVWSRC(aiTextureType_NORMALS, 0), normalUvIndex);
    material->Get(AI_MATKEY_UVWSRC(aiTextureType_AMBIENT, 0), occlusionUvIndex);
    material->Get(AI_MATKEY_UVWSRC(aiTextureType_EMISSIVE, 0), emissiveUvIndex);

    Material materialDesc {
        .baseColorFactor = glm::vec4(baseColorFactor.r, baseColorFactor.g, baseColorFactor.b, baseColorFactor.a),
        .baseColorTexture = baseColor,
        .baseColorUvIndex = static_cast<uint32_t>(baseColorUvIndex),
        .metallicTexture = metallic,
        .metallicUvIndex = static_cast<uint32_t>(metallicUvIndex),
        .metallicFactor = metallicFactor,
        .roughnessTexture = roughness,
        .roughnessUvIndex = static_cast<uint32_t>(roughnessUvIndex),
        .roughnessFactor = roughnessFactor,
        .normalTexture = normal,
        .normalUvIndex = static_cast<uint32_t>(normalUvIndex),
        .normalScale = normalScale,
        .occlusionTexture = occlusion,
        .occlusionUvIndex = static_cast<uint32_t>(occlusionUvIndex),
        .occlusionStrength = occlusionStrength,
        .emissiveTexture = emissive,
        .emissiveUvIndex = static_cast<uint32_t>(emissiveUvIndex),
        .emissiveFactor = glm::vec3(emissiveFactor.r, emissiveFactor.g, emissiveFactor.b),
        .alphaCutoff = alphaCutoff,
    };

    ShaderIndex shader = isTransparent ? context.transparentShader : isDoubleSided ? context.opaqueDoubleSidedShader : context.opaqueShader;
    MaterialIndex rendererMaterialIndex = GetRenderer().AddMaterial(context.commandBuffer, context.frameIndex, shader, &materialDesc, sizeof(Material));
    auto cachePair = std::make_pair(rendererMaterialIndex, shader);
    context.materials.emplace(aiMaterialIndex, cachePair);
    return cachePair;
}

static std::pair<MeshIndex, MaterialIndex> loadMesh(
    ModelLoadingContext& context,
    AiMeshIndex aiMeshIndex
) {
    const aiMesh* mesh = context.scene->mMeshes[aiMeshIndex];
    auto [rendererMaterialIndex, rendererShaderIndex] = loadMaterial(context, mesh->mMaterialIndex);

    {
        auto meshIter = context.meshes.find(aiMeshIndex);
        if (meshIter != context.meshes.end()) return std::make_pair(meshIter->second, rendererMaterialIndex);
    }

    MeshDesc desc;
    desc.indices.reserve(mesh->mNumFaces * 3);
    desc.vertices.reserve(mesh->mNumVertices);

    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        desc.indices.emplace_back(face.mIndices[0]);
        desc.indices.emplace_back(face.mIndices[1]);
        desc.indices.emplace_back(face.mIndices[2]);
    }

    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        VertexDesc vertex{};

        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        if (mesh->mTangents != nullptr && mesh->mBitangents != nullptr) {
            glm::vec3 tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            glm::vec3 bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);

            glm::vec3 expectedBitangent = glm::cross(vertex.normal, tangent);
            float handedness = glm::dot(expectedBitangent, bitangent) < 0.0f ? -1.0f : 1.0f;

            vertex.tangent = glm::vec4(tangent, handedness);
        } else {
            vertex.tangent = glm::zero<glm::vec4>();
        }

        if (mesh->mTextureCoords[0]) {
            vertex.uv0 = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.uv0 = glm::zero<glm::vec2>();
        }
        if (mesh->mTextureCoords[1]) {
            vertex.uv1 = glm::vec2(mesh->mTextureCoords[1][i].x, mesh->mTextureCoords[1][i].y);
        } else {
            vertex.uv1 = glm::zero<glm::vec2>();
        }

        desc.vertices.emplace_back(vertex);
    }

    MeshIndex rendererMeshIndex = GetRenderer().AddMesh(context.commandBuffer, context.frameIndex, rendererShaderIndex, desc);
    context.meshes.emplace(aiMeshIndex, rendererMeshIndex);
    return std::make_pair(rendererMeshIndex, rendererMaterialIndex);
}

static Node::Ref loadNode(ModelLoadingContext& context, const aiNode* node) {
    aiVector3D aiPosition;
    aiQuaternion aiRotation;
    aiVector3D aiScale;
    node->mTransformation.Decompose(aiScale, aiRotation, aiPosition);
    glm::vec3 position(aiPosition.x, aiPosition.y, aiPosition.z);
    glm::quat rotation(aiRotation.w, aiRotation.x, aiRotation.y, aiRotation.z);
    glm::vec3 scale(aiScale.x, aiScale.y, aiScale.z);

    auto result = Node::Create(node->mName.C_Str(), position, rotation, scale);

    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        auto aiMeshIndex = node->mMeshes[i];
        auto [rendererMeshIndex, rendererMaterialIndex] = loadMesh(context, aiMeshIndex);

        auto component = std::make_shared<InstanceComponent>();
        result->AddComponent(component);

        InstanceDesc desc {
            .meshIndex = rendererMeshIndex,
            .materialIndex = rendererMaterialIndex,
            .isDynamic = true,
        };
        GetRenderer().AddInstance(context.commandBuffer, context.frameIndex, component, desc);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        const aiNode* child = node->mChildren[i];
        result->AddChild(loadNode(context, child));
    }

    return result;
}

Model ModelLoader::LoadFile(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.c_str(), IMPORT_FLAGS);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr) {
        throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
    }

    std::filesystem::path dir = std::filesystem::path(path).parent_path();

    std::unordered_map<AiTextureIndex, TextureIndex> embeddedTextures;
    std::unordered_map<std::string, TextureIndex> externalTextures;
    std::unordered_map<AiMaterialIndex, std::pair<MaterialIndex, ShaderIndex>> materials;
    std::unordered_map<AiMeshIndex, MeshIndex> meshes;

    embeddedTextures.reserve(scene->mNumTextures);
    materials.reserve(scene->mNumMaterials);
    meshes.reserve(scene->mNumMeshes);

    ModelLoadingContext context {
        .commandBuffer = commandBuffer,
        .frameIndex = frameIndex,
        .scene = scene,
        .dir = dir,
        .embeddedTextures = embeddedTextures,
        .externalTextures = externalTextures,
        .materials = materials,
        .meshes = meshes,
        .opaqueShader = _opaque,
        .opaqueDoubleSidedShader = _opaqueDoubleSided,
        .transparentShader = _transparent,
    };

    return Model(loadNode(context, scene->mRootNode));
}