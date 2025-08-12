#pragma once
#include <GLFW/glfw3.h>

namespace UI {
    // Call once after an OpenGL context is current
    void Init();
    // Call once on shutdown
    void Shutdown();

    // Draw a rectangle button at (x,y) with size (w,h) in *framebuffer pixels*
    // Returns true iff the left mouse button was clicked inside it this frame.
    bool Button(GLFWwindow* window, float x, float y, float w, float h, bool enabled = true);
}
