#pragma once

#include "components/PhysicsComponent.hpp"

#include <box3d/box3d.h>

#include <unordered_map>

class Physics {
public:
    Physics();

    void RegisterNode(const PhysicsComponent::Ref& component);
    void UnregisterNode(PhysicsComponent::Ref component);

    void Tick();

private:
    b3WorldId _world;
    std::unordered_map<PhysicsComponent::Ref, b3BodyId> _bodies;

    double _lastTime = -1.0;
    double _accumulator = 0.0;
};
