#pragma once

#include "BindingProvider.hpp"
#include "Provider.hpp"

#include <glm/glm.hpp>
#include <dvk/dvk.hpp>

#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct alignas(16) CameraUniforms {
    glm::mat4 viewProjMat;
    glm::mat4 projMat;
    glm::mat4 viewMat;
    glm::vec3 viewPos;
};

class FlyCamera : public BindingProvider {
public:
    FlyCamera(GLFWwindow* window, glm::vec3 startPos);

    void Tick();
    void UpdateUniforms(uint32_t frameIndex);

    [[nodiscard]] glm::vec3 GetPos() const;
    [[nodiscard]] glm::mat4 GetProjMat() const;
    [[nodiscard]] glm::mat4 GetViewMat() const;
    [[nodiscard]] float GetZNear() const;
    [[nodiscard]] float GetZFar() const;
    [[nodiscard]] float GetFovRad() const;
    [[nodiscard]] float GetAspectRatio() const;

    dvk::BindSetLayout::Ref GetLayout() override;
    void Bind(uint32_t binding, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) override;

private:
    GLFWwindow* window;
    double lastTime = 0.0;

    glm::vec3 pos;
    float pitch = 0.0f;
    float yaw = -90.0f;

    float speed = 5.0f;

    glm::mat4 projMat;
    glm::mat4 viewMat;
    float aspectRatio;

    dvk::BindSetLayout::Ref _bindSetLayout;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _uniformBuffer;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> _bindSet;

    void tick();
};
