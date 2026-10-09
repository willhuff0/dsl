#include <Gltf.hpp>

#include "Provider.hpp"
#include "HotReload.hpp"
#include "Renderer.hpp"
#include "Utils/Paths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <queue>
#include <cmath>

// namespace {
//     class GltfShaders {
//     public:
//         static GltfShaders& Get() {
//             static GltfShaders inst;
//             return inst;
//         }
//
//         dvk::GraphicsPipeline::Ref& GetOpaque() {
//             return _opaque;
//         }
//
//     private:
//         GltfShaders() {
//             auto layout = dvk::PipelineLayout::Create(GetDevice())
//                     .SetLabel("[Gltf]")
//                     .AddBindSetLayout(GetRenderer().GetStaticBindSetLayout())
//                     .AddBindSetLayout(GetRenderer().GetPerFrameBindSetLayout())
//                     .AddBindSetLayout(GetGlobalBindSetLayout())
//                     .AddBindSetLayout(GetRenderer().GetShaderPassBindSetLayout())
//                     .AddBindSetLayout(GetRenderer().GetTexturesBindSetLayout())
//                     .Build();
//
//             _opaqueWatcher = HotReload::CreateAndWatch(GLTF_OPAQUE_SHADER_PATH, [this, layout]() {
//                 auto shader = dvk::Shader::LoadFile(GetDevice(), GLTF_OPAQUE_SHADER_PATH, "[Gltf] Opaque");
//                 _opaque = dvk::GraphicsPipeline::Create(GetDevice(), layout)
//                         .SetLabel("[Gltf] Opaque")
//                         .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
//                         .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "VertexMain").FinishStage()
//                         .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "FragmentMain").FinishStage()
//                         .SetSingleAttachment(GetRenderTextureFormat())
//                         .EnableDepth(GetDepthFormat())
//                         .EnableMultisample(GetMultisampleCount())
//                         .Build();
//             });
//         }
//
//         Watcher _opaqueWatcher;
//         dvk::GraphicsPipeline::Ref _opaque;
//     };
// }

static const std::unordered_map<std::string, uint32_t> attributeSizes = {
    { "POSITION", 3 },
    { "NORMAL", 3 },
    { "TANGENT", 4 },
    { "TEXCOORD_0", 2 },
    { "TEXCOORD_1", 2 },
};

static const std::unordered_map<std::string, uint32_t> componentSizes = {
    { "SCALAR", 1 },
    { "VEC2", 2 },
    { "VEC3", 3 },
    { "VEC4", 4 },
    { "MAT2", 4 },
    { "MAT3", 9 },
    { "MAT4", 16 },
};

static std::vector<char> loadBytes(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to load bytes.");
    }
    std::streamsize fileSize = static_cast<std::streamsize>(std::filesystem::file_size(path.c_str()));
    std::vector<char> buffer(fileSize);
    if (!file.read(buffer.data(), fileSize)) {
        throw std::runtime_error("Failed to load bytes.");
    }
    return buffer;
}

uint32_t readUint32(const char* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

static glm::vec3 jsonToVec3(const nlohmann::json& list, glm::vec3 def) {
    if (list.empty()) return def;
    return {list[0], list[1], list[2]};
}
static glm::vec4 jsonToVec4(const nlohmann::json& list, glm::vec4 def) {
    if (list.empty()) return def;
    return {list[0], list[1], list[2], list[3]};
}
static glm::quat jsonToQuat(const nlohmann::json& list, glm::quat def) {
    if (list.empty()) return def;
    return {list[0], list[1], list[2], list[3]};
}

static void decomposeMat4(const std::vector<float>& m, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale) {
    translation.x = m[12];
    translation.y = m[13];
    translation.z = m[14];

    scale.x = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
    scale.y = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
    scale.z = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);

    glm::mat3 rotationMat;
    rotationMat[0] = glm::vec3(m[0], m[1], m[2]) / scale.x;
    rotationMat[1] = glm::vec3(m[4], m[5], m[6]) / scale.x;
    rotationMat[2] = glm::vec3(m[8], m[9], m[10]) / scale.x;
    rotation = glm::quat_cast(rotationMat);
}

Gltf Gltf::Load(const std::string& path, const std::string& sceneName) {
    std::filesystem::path fsPath(path);
    std::string directory = fsPath.parent_path().string();
    std::string extension = fsPath.extension().string();

    std::optional<std::vector<char>> glbBytes;
    std::optional<std::span<char>> glbBuffer;

    nlohmann::json json;
    if (extension == ".glb") {
        glbBytes = loadBytes(path);
        auto& bytes = glbBytes.value();

        size_t offset = 0;

        uint32_t magic = readUint32(bytes.data() + offset);
        offset += 4;
        if (magic != 0x46546C67) {
            throw std::runtime_error("Tried to load a GLTF '" + path + "' which is malformed (incorrect magic number).");
        }

        uint32_t version = readUint32(bytes.data() + offset);
        offset += 4;
        if (version != 2) {
            throw std::runtime_error("Tried to load a GLTF '" + path + "' with unsupported GLB container version '" + std::to_string(version) + "'. Use version '2'.");
        }

        uint32_t fileLength = readUint32(bytes.data() + offset);
        offset += 4;

        uint32_t jsonChunkLength = readUint32(bytes.data() + offset);
        offset += 4;

        uint32_t jsonChunkType = readUint32(bytes.data() + offset);
        offset += 4;
        if (jsonChunkType != 0x4E4F534A) {
            throw std::runtime_error("Tried to load a GLTF '" + path + "' which is malformed (first chunk is not json).");
        }

        std::span jsonChunkData(bytes.data() + offset, offset + jsonChunkLength);
        offset += jsonChunkLength;
        json = nlohmann::json::parse(jsonChunkData);

        if (offset < fileLength - 1) {
            // binary chunk present

            uint32_t binaryChunkLength = readUint32(bytes.data() + offset);
            offset += 4;

            uint32_t binaryChunkType = readUint32(bytes.data() + offset);
            offset += 4;
            if (binaryChunkType != 0x004E4942) {
                throw std::runtime_error("Tried to load a GLTF '" + path + "' which is malformed (second chunk is not binary).");
            }

            glbBuffer = std::span(bytes.data() + offset, offset + binaryChunkLength);
        }
    } else if (extension == '.gltf') {
        std::ifstream file(path);
        json = nlohmann::json::parse(file);
    } else {
        throw std::runtime_error("A file path with an unsupported extension '" + path + "' was passed to the GLTF loader.");
    }

    const auto& asset = json["asset"];
    auto gltfVersion = asset["gltfVersion"].get<std::string>();
    if (gltfVersion != "2.0") {
        throw std::runtime_error("Tried to load a GLTF '" + path + "' with unsupported version '" + gltfVersion + "'. Use version '2.0'.");
    }
    auto copyright = asset.value("copyright", "");
    auto generator = asset.value("generator", "");

    const auto& scenes = json["scenes"];
    uint32_t sceneIndex = 0;
    if (!sceneName.empty()) {
        for (uint32_t i = 0; i < scenes.size(); i++) {
            const auto& scene = scenes[i];
            auto nameIter = scene.find("name");
            if (nameIter == scene.end() || *nameIter != sceneName) continue;
            sceneIndex = i;
            break;
        }
    }
    const auto& scene = scenes[sceneIndex];

    auto rootName = scene.value("name", fsPath.stem().string());

    auto& rootNameIndices = scene["nodes"];

    auto& nodes = json["nodes"];
    auto& meshes = json["meshes"];
    auto& accessors = json["accessors"];
    auto& bufferViews = json["bufferViews"];
    auto& buffers = json["buffers"];
    auto& images = json["images"];
    auto& samplers = json["samplers"];
    auto& textures = json["textures"];
    auto& materials = json["materials"];

    std::unordered_map<uint32_t, std::span<char>> bufferIndexToBytes;

    std::queue<std::pair<int, int>> nodeQueue;
    for (const auto& rootNodeIndex : rootNameIndices) {
        nodeQueue.emplace(rootNodeIndex, -1);
    }
    while (!nodeQueue.empty()) {
        auto [index, parent] = nodeQueue.front();
        nodeQueue.pop();

        auto& node = nodes[index];
        std::string name = node.value("name", "_" + std::to_string(index));

        auto childrenIndicesIter = node.find("children");
        if (childrenIndicesIter != node.end()) {
            for (const auto& childIndex : childrenIndicesIter.value()) {
                nodeQueue.emplace(childIndex, index);
            }
        }

        glm::vec3 translation = jsonToVec3(node["translation"], glm::zero<glm::vec3>());
        glm::quat rotation = jsonToQuat(node["rotation"], glm::identity<glm::quat>());
        glm::vec3 scale = jsonToVec3(node["scale"], glm::one<glm::vec3>());
        auto matrixIter = node.find("matrix");
        if (matrixIter != node.end()) {
            auto matrix = matrixIter.value().get<std::vector<float>>();
            decomposeMat4(matrix, translation, rotation, scale);
        }

        auto result = Node::Create(name, translation, rotation, scale);

        auto meshIndexIter = node.find("mesh");
        if (meshIndexIter != node.end()) {
            uint32_t meshIndex = meshIndexIter.value();
            auto& mesh = meshes[meshIndex];
            auto& primitives = mesh["primitives"];
            for (auto& primitive : primitives) {
                auto& attributes = primitive["attributes"];
                for (auto& [attribute, accessorIndex] : attributes.items()) {
                    auto attributeSizeIter = attributeSizes.find(attribute);
                    if (attributeSizeIter == attributeSizes.end()) {
                        throw std::runtime_error("Skipping an unsupported vertex attribute type '" + attribute + "' in '" + path + "'.");
                    }
                    uint32_t attributeSize = attributeSizeIter->second;

                    auto& accessor = accessors[accessorIndex];
                    uint32_t bufferViewIndex = accessor["bufferView"].get<uint32_t>();
                    uint32_t byteOffset = accessor.value("byteOffset", 0);
                    auto componentSizeIter = componentSizes.find(accessor["type"]);
                    uint32_t componentSize;
                    if (componentSizeIter == componentSizes.end()) {
                        componentSize = attributeSize;
                    } else {
                        componentSize = componentSizeIter->second;
                    }
                    auto& componentType = accessor["componentType"];
                    bool normalized = accessor.value("normalized", false);
                    uint32_t count = accessor["count"].get<uint32_t>();
                    

                    if (attribute == "POSITION") {

                    } else if (attribute == "NORMAL") {

                    } else if (attribute == "TANGENT") {

                    } else if (attribute == "TEXCOORD_0") {

                    } else if (attribute == "TEXCOORD_1") {
                    }
                }
            }
        }
    }
}
