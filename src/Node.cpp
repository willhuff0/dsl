#include <Node.hpp>

#include <stdexcept>

std::shared_ptr<Node> Component::GetNode() const {
    return std::shared_ptr<Node>(_node);
}

bool Component::IsEnabled() const {
    return _isEnabled;
}

void Component::SetEnabled(bool isEnabled) {
    if (_isEnabled == isEnabled) return;
    _isEnabled = isEnabled;
    if (!_node.lock()->IsFullyEnabled()) return;
    if (_isEnabled) {
        _onEnable();
    } else {
        _onDisable();
    }
}

Node::Node(std::string name, glm::vec3 pos, glm::quat rot, glm::vec3 scale)
    : _name(std::move(name)), _lPos(pos), _lRot(rot), _lScale(scale) { }

Node::Ref Node::Create(const std::string& name, glm::vec3 pos, glm::quat rot, glm::vec3 scale) {
    return std::make_shared<Node>(name, pos, rot, scale);
}

const std::string& Node::GetName() const {
    return _name;
}

void Node::SetName(std::string name) {
    _name = std::move(name);
}

const Node::Ref& Node::GetParent() const {
    return _parent;
}

glm::vec3 Node::GetLPos() const { return _lPos; }
glm::quat Node::GetLRot() const { return _lRot; }
glm::vec3 Node::GetLAnglesRad() const { return glm::eulerAngles(_lRot); }
glm::vec3 Node::GetLAnglesDeg() const { return glm::degrees(glm::eulerAngles(_lRot)); }
glm::vec3 Node::GetLScale() const { return _lScale; }

void Node::SetLPos(glm::vec3 lp) {
    _lPos = lp;
    _markWDirty();
}
void Node::SetLRot(glm::quat lr) {
    _lRot = lr;
    _markWDirty();
}
void Node::SetLAnglesRad(glm::vec3 la) { SetLRot(glm::quat(la)); }
void Node::SetLAnglesDeg(glm::vec3 la) { SetLRot(glm::quat(glm::radians(la))); }
void Node::SetLScale(glm::vec3 ls) {
    _lScale = ls;
    _markWDirty();
}

glm::vec3 Node::GetWPos() const {
    if (_parent == nullptr) return _lPos;
    if (_wDirty) {
        auto scaledLPos = _lPos * _parent->GetWScale();
        auto rotatedLPos = inverse(_parent->GetWRot()) * scaledLPos;
        return _parent->GetWPos() + rotatedLPos;
    }
    return _wPos;
}
glm::quat Node::GetWRot() const {
    if (_parent == nullptr) return _lRot;
    if (_wDirty) {
        return _parent->GetWRot() * _lRot;
    }
    return _wRot;
}
glm::vec3 Node::GetWAnglesRad() const { return glm::eulerAngles(GetWRot()); }
glm::vec3 Node::GetWAnglesDeg() const { return glm::degrees(glm::eulerAngles(GetWRot())); }
glm::vec3 Node::GetWScale() const {
    if (_parent == nullptr) return _lScale;
    if (_wDirty) {
        return _parent->GetWScale() * _lScale;
    }
    return _wScale;
}

glm::vec3 Node::GetWPos() {
    if (_parent == nullptr) return _lPos;
    if (_wDirty) {
        _updateWorldFromParent();
        _wDirty = false;
    }
    return _wPos;
}
glm::quat Node::GetWRot() {
    if (_parent == nullptr) return _lRot;
    if (_wDirty) {
        _updateWorldFromParent();
        _wDirty = false;
    }
    return _wRot;
}
glm::vec3 Node::GetWAnglesRad() { return glm::eulerAngles(GetWRot()); }
glm::vec3 Node::GetWAnglesDeg() { return glm::degrees(glm::eulerAngles(GetWRot())); }
glm::vec3 Node::GetWScale() {
    if (_parent == nullptr) return _lScale;
    if (_wDirty) {
        _updateWorldFromParent();
        _wDirty = false;
    }
    return _wScale;
}

void Node::SetWPos(glm::vec3 value) {
    _wPos = value;
    if (_parent == nullptr) {
        _lPos = value;
    } else {
        glm::vec3 rotatedLPos = value - _parent->GetWPos();
        glm::vec3 scaledLPos = _parent->GetWRot() * rotatedLPos;
        _lPos = scaledLPos / _parent->GetWScale();
    }
    _markWDirty();
    _wDirty = false;
}
void Node::SetWRot(glm::quat value) {
    _wRot = value;
    if (_parent == nullptr) {
        _lRot = value;
    } else {
        _lRot = inverse(_parent->GetWRot()) * value;
    }
    _markWDirty();
    _wDirty = false;
}
void Node::SetWAnglesRad(glm::vec3 value) { SetWRot(glm::quat(value)); }
void Node::SetWAnglesDeg(glm::vec3 value) { SetWRot(glm::quat(glm::radians(value))); }
void Node::SetWScale(glm::vec3 value) {
    _wScale = value;
    if (_parent == nullptr) {
        _lScale = value;
    } else {
        _lScale = value / _parent->GetLScale();
    }
    _markWDirty();
    _wDirty = false;
}

glm::mat4 Node::GetMatrix() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), GetWPos());
    glm::mat4 rotation = glm::mat4_cast(GetWRot());
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), GetWScale());

    return translation * rotation * scale;
}
glm::mat4 Node::GetMatrix() {
    if (_wDirty) {
        _updateWorldFromParent();
        _wDirty = false;
    }

    glm::mat4 translation = glm::translate(glm::mat4(1.0f), _wPos);
    glm::mat4 rotation = glm::mat4_cast(_wRot);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), _wScale);

    return translation * rotation * scale;
}

glm::vec3 Node::GetForwardVector() const {
    return inverse(GetWRot()) * glm::vec3(0.0f, 0.0f, 1.0f);
}
glm::vec3 Node::GetRightVector() const {
    return inverse(GetWRot()) * glm::vec3(1.0f, 0.0f, 0.0f);
}
glm::vec3 Node::GetUpVector() const {
    return inverse(GetWRot()) * glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 Node::GetForwardVector() {
    return inverse(GetWRot()) * glm::vec3(0.0f, 0.0f, 1.0f);
}
glm::vec3 Node::GetRightVector() {
    return inverse(GetWRot()) * glm::vec3(1.0f, 0.0f, 0.0f);
}
glm::vec3 Node::GetUpVector() {
    return inverse(GetWRot()) * glm::vec3(0.0f, 1.0f, 0.0f);
}

bool Node::IsEnabled() const { return _isEnabled; }
bool Node::IsFullyEnabled() const {
    if (_parent == nullptr) return _isEnabled;
    return _isEnabled && _parent->IsFullyEnabled();
}
void Node::SetEnabled(bool isEnabled) {
    if (_isEnabled == isEnabled) return;
    _isEnabled = isEnabled;
    if (IsFullyEnabled()) {
        _onEnable();
    } else {
        _onDisable();
    }
}

const std::vector<Node::Ref>& Node::GetChildren() const { return _children; }
void Node::AddChild(Node::Ref child) {
    if (child->_parent != nullptr) child->_parent->RemoveChild(child);
    child->_parent = shared_from_this();
    child->_markWDirty();
    _children.emplace_back(std::move(child));
}
void Node::RemoveChild(const Node::Ref& child) {
    std::erase_if(_children, [&](const Node::Ref& c) {
        return c == child;
    });
}
void Node::RemoveAllChildren() {
    for (const auto& child : _children) {
        child->_parent = nullptr;
    }
    _children.clear();
}

const std::vector<Component::Ref>& Node::GetComponents() const { return _components; }
void Node::AddComponent(Component::Ref component) {
    auto componentNode = component->_node.lock();
    if (componentNode != nullptr) componentNode->RemoveComponent(component);

    component->_node = weak_from_this();
    _components.emplace_back(std::move(component));
}
void Node::RemoveComponent(const Component::Ref& component) {
    std::erase_if(_components, [&](const Component::Ref& c) {
        return c == component;
    });
}

void Node::EnterScene() {
    bool callOnEnable = IsFullyEnabled();
    _enterScene(callOnEnable);
}

void Node::ExitScene() {
    bool callOnDisable = IsFullyEnabled();
    _exitScene(callOnDisable);
}

void Node::_enterScene(bool callOnEnable) {
    for (const auto& child : _children) {
        child->_enterScene(callOnEnable);
    }
    for (const auto& component : _components) {
        component->_onEnterScene();
        if (callOnEnable && component->IsEnabled()) {
            component->_onEnable();
        }
    }
}

void Node::_exitScene(bool callOnDisable) {
    for (const auto& child : _children) {
        child->_exitScene(callOnDisable);
    }
    for (const auto& component : _components) {
        if (callOnDisable && component->IsEnabled()) {
            component->_onDisable();
        }
        component->_onEnterScene();
    }
}

void Node::_onEnable() {
    for (const auto& child : _children) {
        child->_onEnable();
    }
    for (const auto& component : _components) {
        if (component->IsEnabled()) {
            component->_onEnable();
        }
    }
}

void Node::_onDisable() {
    for (const auto& child : _children) {
        child->_onDisable();
    }
    for (const auto& component : _components) {
        if (component->IsEnabled()) {
            component->_onDisable();
        }
    }
}

void Node::_markWDirty() {
    _wDirty = true;
    for (const auto& child : _children) {
        child->_markWDirty();
    }
}

void Node::_updateWorldFromParent() {
    auto scaledLPos = _lPos * _parent->GetWScale();
    auto rotatedLPos = inverse(_parent->GetWRot()) * scaledLPos;
    _wPos = _parent->GetWPos() + rotatedLPos;
    _wRot = _parent->GetWRot() * _lRot;
    _wScale = _parent->GetWScale() * _lScale;
}