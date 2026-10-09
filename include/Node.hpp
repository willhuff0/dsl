#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <vector>
#include <string>

class Component {
    friend class Node;
public:
    using Ref = std::shared_ptr<Component>;

    virtual ~Component() = default;

    [[nodiscard]] std::shared_ptr<Node> GetNode() const;
    [[nodiscard]] bool IsEnabled() const;

    void SetEnabled(bool isEnabled);

protected:
    virtual void _onEnterScene() { }
    virtual void _onExitScene() { }
    virtual void _onEnable() { }
    virtual void _onDisable() { }

private:
    std::weak_ptr<Node> _node;
    bool _isEnabled = true;
};

class Node : public std::enable_shared_from_this<Node> {
public:
    using Ref = std::shared_ptr<Node>;

    Node(std::string name, glm::vec3 pos, glm::quat rot, glm::vec3 scale);

    static Node::Ref Create(const std::string& name,
        glm::vec3 pos = glm::zero<glm::vec3>(),
        glm::quat rot = glm::identity<glm::quat>(),
        glm::vec3 scale = glm::one<glm::vec3>());

    [[nodiscard]] const std::string& GetName() const;
    void SetName(std::string name);

    [[nodiscard]] const Node::Ref& GetParent() const;

    [[nodiscard]] glm::vec3 GetLPos() const;
    [[nodiscard]] glm::quat GetLRot() const;
    [[nodiscard]] glm::vec3 GetLAnglesRad() const;
    [[nodiscard]] glm::vec3 GetLAnglesDeg() const;
    [[nodiscard]] glm::vec3 GetLScale() const;

    void SetLPos(glm::vec3 lp);
    void SetLRot(glm::quat lr);
    void SetLAnglesRad(glm::vec3 lar);
    void SetLAnglesDeg(glm::vec3 lad);
    void SetLScale(glm::vec3 ls);

    [[nodiscard]] glm::vec3 GetWPos() const;
    [[nodiscard]] glm::quat GetWRot() const;
    [[nodiscard]] glm::vec3 GetWAnglesRad() const;
    [[nodiscard]] glm::vec3 GetWAnglesDeg() const;
    [[nodiscard]] glm::vec3 GetWScale() const;

    [[nodiscard]] glm::vec3 GetWPos();
    [[nodiscard]] glm::quat GetWRot();
    [[nodiscard]] glm::vec3 GetWAnglesRad();
    [[nodiscard]] glm::vec3 GetWAnglesDeg();
    [[nodiscard]] glm::vec3 GetWScale();

    void SetWPos(glm::vec3 value);
    void SetWRot(glm::quat value);
    void SetWAnglesRad(glm::vec3 value);
    void SetWAnglesDeg(glm::vec3 wad);
    void SetWScale(glm::vec3 value);

    [[nodiscard]] glm::mat4 GetMatrix() const;
    [[nodiscard]] glm::mat4 GetMatrix();

    [[nodiscard]] glm::vec3 GetForwardVector() const;
    [[nodiscard]] glm::vec3 GetRightVector() const;
    [[nodiscard]] glm::vec3 GetUpVector() const;

    [[nodiscard]] glm::vec3 GetForwardVector();
    [[nodiscard]] glm::vec3 GetRightVector();
    [[nodiscard]] glm::vec3 GetUpVector();

    [[nodiscard]] bool IsEnabled() const;
    [[nodiscard]] bool IsFullyEnabled() const;
    void SetEnabled(bool isEnabled);

    [[nodiscard]] const std::vector<Node::Ref>& GetChildren() const;
    void AddChild(Node::Ref child);
    void RemoveChild(const Node::Ref& child);
    void RemoveAllChildren();

    [[nodiscard]] const std::vector<Component::Ref>& GetComponents() const;
    void AddComponent(Component::Ref component);
    void RemoveComponent(const Component::Ref& component);

    void EnterScene();
    void ExitScene();

private:
    bool _isEnabled = true;
    std::string _name;
    Node::Ref _parent;
    // TODO: use linked hash set
    std::vector<Node::Ref> _children;
    std::vector<Component::Ref> _components;

    glm::vec3 _lPos;
    glm::quat _lRot;
    glm::vec3 _lScale;

    bool _wDirty = false;
    glm::vec3 _wPos = glm::zero<glm::vec3>();
    glm::quat _wRot = glm::identity<glm::quat>();
    glm::vec3 _wScale = glm::one<glm::vec3>();

    void _markWDirty();
    void _updateWorldFromParent();

    void _enterScene(bool callOnEnable);
    void _exitScene(bool callOnDisable);

    void _onEnable();
    void _onDisable();
};
