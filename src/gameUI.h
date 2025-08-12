#pragma once
#include <GLFW/glfw3.h>

namespace UI {
    void Init();
    void Shutdown();

    bool Button(GLFWwindow* window, float x, float y, float w, float h, bool enabled = true);
    void Text(GLFWwindow* window, float x, float y, float w, float h, const char* text);

}
