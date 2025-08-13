#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <cmath>

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
    float speed=1.0f;
    float mouseSensitivity = 0.06f;
    float zoom;
    CameraMode mode;

    // Tuning
    float smoothingHz    = 10.0f;     // 8–20 is typical
    float mouseDeadzone  = 0.05f;
    float mouseMaxStep   = 60.0f;     // clamp large spikes

    // Smoothing state
    glm::vec2 smoothedDelta = glm::vec2(0.0f);

    // Re-arm mouse deltas after cursor mode/focus changes
    void resetMouse() {
        firstMouse = true;
        smoothedDelta = glm::vec2(0.0f);
    }

    Camera(glm::vec3 startPos, glm::vec3 upDir, float startYaw, float startPitch, CameraMode mode = FIRST_PERSON)
    : front(glm::vec3(0.0f, 0.0f, -1.0f)), speed(2.5f), zoom(45.0f), mode(mode) 
    {
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

    void update(GLFWwindow* window, float deltaTime) {
        processKeyboard(window, deltaTime);

        // Only read mouse when cursor is captured and window is focused
        if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED ||
            glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE) {
            resetMouse();
            return;
        }

        int ww = 0, wh = 0;
        glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        double xpos = 0.0, ypos = 0.0;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (firstMouse) {
            glfwSetCursorPos(window, cx, cy);
            lastMouseX = cx;
            lastMouseY = cy;
            firstMouse = false;
            return; // skip one frame to avoid a spike
        }

        float dx = static_cast<float>(xpos - cx);
        float dy = static_cast<float>(cy - ypos); // Y reversed

        // Recenter every frame so (xpos,ypos) is always relative to center
        glfwSetCursorPos(window, cx, cy);

        // deadzone to kill tiny jitter
        if (std::fabs(dx) < mouseDeadzone) dx = 0.0f;
        if (std::fabs(dy) < mouseDeadzone) dy = 0.0f;

        // clamp spikes (e.g., when the window regains focus)
        dx = glm::clamp(dx, -mouseMaxStep, mouseMaxStep);
        dy = glm::clamp(dy, -mouseMaxStep, mouseMaxStep);

        // time-aware smoothing that snaps to zero when there's no input
        glm::vec2 raw(dx, dy);

        // if raw is zero (after deadzone), kill the tail immediately
        if (raw.x == 0.0f && raw.y == 0.0f) {
            smoothedDelta = glm::vec2(0.0f);
        } else {
            float alpha = 1.0f - std::exp(-smoothingHz * deltaTime); // 0..1
            smoothedDelta = glm::mix(smoothedDelta, raw, alpha);
            if (glm::length(smoothedDelta) < 0.001f) smoothedDelta = glm::vec2(0.0f);
        }
        processMouseMovement(smoothedDelta.x, smoothedDelta.y);

        //this code segment above is to prevent camera drift when the mouse is not moving, per previous implementation
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

        yaw -= xoffset;
        pitch += yoffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        updateCameraVectors();
    }

    void toggleMode() {
        if (mode == FIRST_PERSON){
            // Switch to TPP: back up and raise the camera
            position = position - front * 1.0f + glm::vec3(0.0f, 1.0f, 0.0f);
            mode = THIRD_PERSON;
        }
        else{
            // Switch to FPP: reset position and orientation
            position = position + front * 1.0f - glm::vec3(0.0f, 1.0f, 0.0f);
            mode = FIRST_PERSON;
        }

    }
    
    glm::vec3 getPosition() const {
        return position;
    }
    glm::vec3 getFront() const {
        return front;
    }


private:
    double lastMouseX = 400.0, lastMouseY = 300.0;
    bool firstMouse = true;

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
