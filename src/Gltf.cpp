#include <Gltf.hpp>

#include "Provider.hpp"
#include "HotReload.hpp"
#include "Renderer.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

constexpr auto OPAQUE_SHADER_PATH = "";

namespace {
    class GltfShaders {
    public:
        static GltfShaders& Get() {
            static GltfShaders inst;
            return inst;
        }

        dvk::GraphicsPipeline::Ref& GetOpaque() {
            return _opaque;
        }

    private:
        GltfShaders() {
            auto layout = dvk::PipelineLayout::Create(GetDevice())
                    .SetLabel("[Gltf]")
                    .AddBindSetLayout(GetRenderer().GetStaticBindSetLayout())
                    .AddBindSetLayout(GetRenderer().GetPerFrameBindSetLayout())
                    .AddBindSetLayout(GetGlobalBindSetLayout())
                    .AddBindSetLayout(GetRenderer().GetShaderPassBindSetLayout())
                    .AddBindSetLayout(GetRenderer().GetTexturesBindSetLayout())
                    .Build();

            _opaqueWatcher = HotReload::CreateAndWatch(OPAQUE_SHADER_PATH, [this, layout]() {
                auto shader = dvk::Shader::LoadFile(GetDevice(), OPAQUE_SHADER_PATH, "[Gltf] Opaque");
                _opaque = dvk::GraphicsPipeline::Create(GetDevice(), layout)
                        .SetLabel("[Gltf] Opaque")
                        .SetVertexInput(RENDERER_VERTEX_BINDINGS, RENDERER_VERTEX_ATTRIBUTES)
                        .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "VertexMain").FinishStage()
                        .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "FragmentMain").FinishStage()
                        .SetSingleAttachment(GetRenderTextureFormat())
                        .EnableDepth(GetDepthFormat())
                        .EnableMultisample(GetMultisampleCount())
                        .Build();
            });
        }

        Watcher _opaqueWatcher;
        dvk::GraphicsPipeline::Ref _opaque;
    };
}

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
}
