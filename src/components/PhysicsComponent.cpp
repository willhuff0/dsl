#include <components/PhysicsComponent.hpp>

#include "Physics.hpp"
#include "Provider.hpp"

void PhysicsComponent::_onEnable() {
    GetPhysics().RegisterNode(shared_from_this());
    _previousPosition = GetNode()->GetWPos();
    _previousRotation = GetNode()->GetWRot();
}

void PhysicsComponent::_onDisable() {
    GetPhysics().UnregisterNode(shared_from_this());
}

glm::mat4 PhysicsComponent::GetInterpolatedMatrix() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), _interpolatedPosition);
    glm::mat4 rotation = glm::mat4_cast(_interpolatedRotation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), GetNode()->GetWScale());

    return translation * rotation * scale;
}
