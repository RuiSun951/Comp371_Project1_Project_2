#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

enum CameraMode { FIRST_PERSON, THIRD_PERSON };

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float speed;
    float mouseSensitivity;
    float zoom;
    CameraMode mode;

    Camera(glm::vec3 startPos, glm::vec3 upDir, float startYaw, float startPitch, CameraMode mode = FIRST_PERSON)
        : front(glm::vec3(0.0f, 0.0f, -1.0f)), speed(2.5f), mouseSensitivity(0.1f), zoom(45.0f), mode(mode) {
        position = startPos;
        worldUp = upDir;
        yaw = startYaw;
        pitch = startPitch;
        updateCameraVectors();
    }

    glm::mat4 getViewMatrix() {
        if (mode == FIRST_PERSON)
            return glm::lookAt(position, position + front, up);
        else
            return glm::lookAt(position - front * 5.0f + glm::vec3(0.0f, 2.0f, 0.0f), position, up);
    }

    void processKeyboard(GLFWwindow* window, float deltaTime) {
        float velocity = speed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            velocity *= 2.0f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += right * velocity;
    }

    void processMouseMovement(float xoffset, float yoffset) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        updateCameraVectors();
    }

    void toggleMode() {
        if (mode == FIRST_PERSON)
            mode = THIRD_PERSON;
        else
            mode = FIRST_PERSON;
    }

private:
    void updateCameraVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};

#endif
