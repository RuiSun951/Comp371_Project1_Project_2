#pragma once
#include <GLFW/glfw3.h>

namespace UI {
    void Init();
    void Shutdown();

    bool Button(GLFWwindow* window, float x, float y, float w, float h, bool enabled = true);
    void Text(GLFWwindow* window, float x, float y, const char* msg, float r=1, float g=1, float b=1, float scale=1.0f);

}
