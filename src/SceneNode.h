// SceneNode.h
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <functional>

class SceneNode {
public:
    glm::mat4 localTransform;
    std::vector<SceneNode*> children;

    std::function<void(const glm::mat4&)> drawFunc;

    SceneNode() : localTransform(glm::mat4(1.0f)) {}

    void addChild(SceneNode* child) {
        children.push_back(child);
    }

    void draw(const glm::mat4& parentTransform) {
        glm::mat4 globalTransform = parentTransform * localTransform;

        if (drawFunc)
            drawFunc(globalTransform); // Pass the composed transform to shader

        for (SceneNode* child : children) {
            child->draw(globalTransform);
        }
    }

    glm::mat4 getGlobalTransform(const glm::mat4& parentTransform = glm::mat4(1.0f)) const {
        return parentTransform * localTransform;
    }

};
