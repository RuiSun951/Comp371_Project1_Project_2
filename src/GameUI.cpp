#include "GameUI.h"
#include <GL/glew.h>
#include <cstdio>

// ------- internal state -------
namespace {
    GLuint uiProg = 0;
    GLuint uiVAO  = 0;
    GLuint uiVBO  = 0;

    // Super small shaders (solid color, no textures)
    const char* UI_VERT = R"(#version 330 core
    layout (location = 0) in vec2 aPos;       // pixel coords (x,y)
    uniform mat4 uProj;                       // orthographic
    void main() {
        gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    })";

    const char* UI_FRAG = R"(#version 330 core
    out vec4 FragColor;
    uniform vec4 uColor;
    void main() { FragColor = uColor; }
    )";

    GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = GL_FALSE;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, 1024, nullptr, log);
            fprintf(stderr, "[UI] Shader compile error:\n%s\n", log);
        }
        return s;
    }

    GLuint link(GLuint vs, GLuint fs) {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        GLint ok = GL_FALSE;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(p, 1024, nullptr, log);
            fprintf(stderr, "[UI] Program link error:\n%s\n", log);
        }
        return p;
    }

    // Build a column-major orthographic matrix (OpenGL style)
    // Origin at top-left: left=0, right=fbw, top=0, bottom=fbh, near=-1, far=1
    void makeOrtho(float left, float right, float top, float bottom, float znear, float zfar, float out[16]) {
        const float rl = right - left;
        const float tb = top   - bottom;   // note: top < bottom to flip Y
        const float fn = zfar  - znear;

        // Column-major
        out[0] =  2.0f / rl; out[4] = 0.0f;        out[8]  = 0.0f;       out[12] = -(right + left) / rl;
        out[1] =  0.0f;      out[5] = 2.0f / tb;   out[9]  = 0.0f;       out[13] = -(top + bottom) / tb;
        out[2] =  0.0f;      out[6] = 0.0f;        out[10] = -2.0f / fn; out[14] = -(zfar + znear) / fn;
        out[3] =  0.0f;      out[7] = 0.0f;        out[11] = 0.0f;       out[15] = 1.0f;
    }

    bool prevMouseDown = false;
}

namespace UI {

void Init() {
    GLuint vs = compile(GL_VERTEX_SHADER,   UI_VERT);
    GLuint fs = compile(GL_FRAGMENT_SHADER, UI_FRAG);
    uiProg = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    // Reserve space for 4 vertices (x,y)
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Shutdown() {
    if (uiVBO) glDeleteBuffers(1, &uiVBO);
    if (uiVAO) glDeleteVertexArrays(1, &uiVAO);
    if (uiProg) glDeleteProgram(uiProg);
    uiVBO = uiVAO = uiProg = 0;
}

bool Button(GLFWwindow* window, float x, float y, float w, float h, bool enabled) {
    // Framebuffer size in actual pixels (important for HiDPI)
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);

    // Mouse pos in window coords → convert to framebuffer pixels
    double mx, my; glfwGetCursorPos(window, &mx, &my);
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    float sx = ww ? (float)fbw / (float)ww : 1.0f;
    float sy = wh ? (float)fbh / (float)wh : 1.0f;
    mx *= sx; my *= sy;

    bool hover = (mx >= x && mx <= x + w && my >= y && my <= y + h);

    // Ortho projection with origin at top-left
    float proj[16];
    makeOrtho(0.0f, (float)fbw, 0.0f, (float)fbh, -1.0f, 1.0f, proj);

    // Quad vertices (x,y) as TRIANGLE_FAN
    float verts[8] = { x, y,  x + w, y,  x + w, y + h,  x, y + h };

    // Colors
    const float base[4]   = enabled
        ? (hover ? (float[4]){0.25f, 0.45f, 0.90f, 1.0f} : (float[4]){0.18f, 0.28f, 0.60f, 1.0f})
        : (float[4]){0.35f, 0.35f, 0.35f, 1.0f};
    const float border[4] = hover ? (float[4]){1,1,1,1} : (float[4]){0,0,0,1};

    glUseProgram(uiProg);
    GLint locP = glGetUniformLocation(uiProg, "uProj");
    GLint locC = glGetUniformLocation(uiProg, "uColor");
    glUniformMatrix4fv(locP, 1, GL_FALSE, proj);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    // Filled rect
    glUniform4fv(locC, 1, base);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Outline
    glUniform4fv(locC, 1, border);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // Edge-triggered click
    bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool clicked = (enabled && hover && down && !prevMouseDown);
    prevMouseDown = down;
    return clicked;
}

} // namespace UI
