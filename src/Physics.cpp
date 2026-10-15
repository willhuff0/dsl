#include <Physics.hpp>

#include <GLFW/glfw3.h>

static constexpr float TIME_STEP = 1.0f / 60.0f;
static constexpr float SUB_STEPS = 4;

Physics::Physics() {
    b3WorldDef worldDef = b3DefaultWorldDef();
    worldDef.gravity = {0.0f, -12.0f, 0.0f};
    _world = b3CreateWorld(&worldDef);

    b3BodyDef groundBodyDef = b3DefaultBodyDef();
    groundBodyDef.position = { 0.0f, -50.0f, 0.0f };

    b3BodyId groundId = b3CreateBody(_world, &groundBodyDef);

    b3BoxHull groundBox = b3MakeBoxHull(50.0f, 10.0f, 50.0f);

    b3ShapeDef groundShapeDef = b3DefaultShapeDef();
    b3CreateHullShape(groundId, &groundShapeDef, &groundBox.base);
}

void Physics::RegisterNode(const PhysicsComponent::Ref& component) {
    auto node = component->GetNode();

    b3BodyDef bodyDef = b3DefaultBodyDef();
    bodyDef.type = b3_dynamicBody;
    bodyDef.position = {node->GetWPos().x, node->GetWPos().y, node->GetWPos().z};
    bodyDef.rotation = {{node->GetWRot().x, node->GetWRot().y, node->GetWRot().z}, node->GetWRot().w};
    b3BodyId body = b3CreateBody(_world, &bodyDef);

    b3BoxHull dynamicBox = b3MakeCubeHull(1.0f);

    b3ShapeDef shapeDef = b3DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.baseMaterial.friction = 0.3f;

    b3CreateHullShape(body, &shapeDef, &dynamicBox.base);

    _bodies.emplace(component, body);
}

void Physics::UnregisterNode(PhysicsComponent::Ref component) {

}

void Physics::Tick() {
    double time = glfwGetTime();
    if (_lastTime < 0) {
        _lastTime = time;
        return;
    }
    double dt = time - _lastTime;
    _lastTime = time;

    bool didStep = false;
    _accumulator += dt;
    while (_accumulator > TIME_STEP) {
        b3World_Step(_world, TIME_STEP, SUB_STEPS);
        _accumulator -= TIME_STEP;
        didStep = true;
    }

    if (didStep) {
        for (const auto& [component, body] : _bodies) {
            b3Vec3 position = b3Body_GetPosition(body);
            b3Quat rotation = b3Body_GetRotation(body);

            auto node = component->GetNode();
            component->_previousPosition = node->GetWPos();
            component->_previousRotation = node->GetWRot();
            node->SetWPos({position.x, position.y, position.z});
            node->SetWRot({rotation.s, rotation.v.x, rotation.v.y, rotation.v.z});
        }
    }

    auto a = static_cast<float>(_accumulator / TIME_STEP);
    for (const auto& [component, body] : _bodies) {
        auto node = component->GetNode();

        component->_interpolatedPosition = component->_previousPosition * (1.0f - a) + node->GetWPos() * a;
        component->_interpolatedRotation = glm::slerp(component->_previousRotation, node->GetWRot(), a);
    }
}
