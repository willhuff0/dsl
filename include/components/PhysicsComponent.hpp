#pragma once

#include "Node.hpp"

class PhysicsComponent : public Component, public std::enable_shared_from_this<PhysicsComponent> {
    friend class Physics;
public:
    using Ref = std::shared_ptr<PhysicsComponent>;

    glm::mat4 GetInterpolatedMatrix() const;

protected:
    void _onEnable() override;
    void _onDisable() override;

private:
    glm::vec3 _interpolatedPosition;
    glm::quat _interpolatedRotation;
    glm::vec3 _previousPosition;
    glm::quat _previousRotation;
};