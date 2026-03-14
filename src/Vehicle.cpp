#include <Vehicle.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

static ShaderInstance loadShader(const std::string& id) {

}

Vehicle::Vehicle() {
    std::ifstream partManifestFile(VEHICLE_PART_MANIFEST_PATH);
    if (!partManifestFile.is_open()) throw std::runtime_error("Failed to open vehicle part manifest!");
    nlohmann::json partManifestJson = nlohmann::json::parse(partManifestFile);

    for (auto& partJson : partManifestJson.at("parts")) {
        std::string partId = partJson.at("id");
        std::string displayName = partJson.at("display-name");
        float health = partJson.at("health");
        std::string shaderId = partJson.value("shader", "opaque");

        if (partTypes.contains(partId)) throw std::runtime_error("Duplicate part id found!");
        {
            auto iter = shaderInstances.find(shaderId);
            if (iter == shaderInstances.end()) {
                shaderInstances[shaderId] = loadShader(shaderId);
            }
        }"Vehicle Instance Buffer (" + partId + ")"

        partTypes[partId] = {
            .displayName = displayName,
            .health = health,
            .shaderId = shaderId,
            .indirectBufferOffset = ,
            .instanceBuffer = VehicleGrowingBuffer(vk::BufferUsageFlagBits::eVertexBuffer),
            .instanceCount = 0,
        };
    }
}
