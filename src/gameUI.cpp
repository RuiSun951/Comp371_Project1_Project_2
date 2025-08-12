#define GLEW_STATIC 1
#include <GL/glew.h>
#include "gameUI.h"
#include <cstdio>
#include <vector>
#include "../stb/stb_easy_font.h"   // tiny built‑in bitmap font


// ------- internal state -------
namespace {
    GLuint uiProg = 0;
    GLuint uiVAO  = 0;
    GLuint uiVBO  = 0;

    // Super small shaders (solid color, no textures)
    const char* UI_VERT = R"(#version 330 core
        layout (location = 0) in vec2 aPos;
        uniform mat4 uProj;
        void main() {
            gl_Position = uProj * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* UI_FRAG = R"(#version 330 core
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
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
    // start with a tiny buffer; we'll resize as needed
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
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

    // Higher-contrast colors
    float base[4];
    if (enabled) {
        if (hover) { base[0]=0.18f; base[1]=0.60f; base[2]=1.00f; base[3]=1.0f; } // vibrant blue
        else       { base[0]=0.09f; base[1]=0.22f; base[2]=0.55f; base[3]=1.0f; } // deep blue
    } else {
        base[0]=0.25f; base[1]=0.25f; base[2]=0.25f; base[3]=1.0f;
    }

    float border[4];
    if (hover) { border[0]=1.0f; border[1]=1.0f; border[2]=1.0f; border[3]=1.0f; }
    else       { border[0]=0.0f; border[1]=0.0f; border[2]=0.0f; border[3]=1.0f; }

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


static void setProjForWindow(GLFWwindow* window, float proj[16]) {
    int fbw=0, fbh=0; glfwGetFramebufferSize(window, &fbw, &fbh);
    makeOrtho(0.0f, (float)fbw, 0.0f, (float)fbh, -1.0f, 1.0f, proj);
}

void Text(GLFWwindow* window, float x, float y, const char* msg,
          float r, float g, float b, float scale)
{
    if (!msg || !*msg) return;

    // Generate quads with stb_easy_font (origin at top-left if we pass our y directly)
    char quadBuf[10000];                     // enough for short labels
    int numQuads = stb_easy_font_print(0.0f, 0.0f, (char*)msg, nullptr, quadBuf, sizeof(quadBuf));
    if (numQuads <= 0) return;

    struct EV { float x, y; unsigned char rgba[4]; };
    EV* qv = (EV*)quadBuf;                   // 4 vertices per quad

    // Convert quads → triangles (two per quad) and apply x/y/scale
    std::vector<float> verts;
    verts.reserve(numQuads * 6 * 2);
    for (int q = 0; q < numQuads; ++q) {
        EV v0 = qv[q*4 + 0];
        EV v1 = qv[q*4 + 1];
        EV v2 = qv[q*4 + 2];
        EV v3 = qv[q*4 + 3];

        float p0x = x + v0.x * scale, p0y = y + v0.y * scale;
        float p1x = x + v1.x * scale, p1y = y + v1.y * scale;
        float p2x = x + v2.x * scale, p2y = y + v2.y * scale;
        float p3x = x + v3.x * scale, p3y = y + v3.y * scale;

        // tri 0: 0-1-2
        verts.push_back(p0x); verts.push_back(p0y);
        verts.push_back(p1x); verts.push_back(p1y);
        verts.push_back(p2x); verts.push_back(p2y);
        // tri 1: 0-2-3
        verts.push_back(p0x); verts.push_back(p0y);
        verts.push_back(p2x); verts.push_back(p2y);
        verts.push_back(p3x); verts.push_back(p3y);
    }

    float proj[16]; setProjForWindow(window, proj);

    glUseProgram(uiProg);
    GLint locP = glGetUniformLocation(uiProg, "uProj");
    GLint locC = glGetUniformLocation(uiProg, "uColor");
    glUniformMatrix4fv(locP, 1, GL_FALSE, proj);
    glUniform4f(locC, r, g, b, 1.0f);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 2));
}

void TextCenteredInRect(GLFWwindow* window, float x, float y, float w, float h,
                        const char* msg, float r, float g, float b, float scale)
{
    if (!msg) return;
    int tw = stb_easy_font_width((char*)msg);
    int th = stb_easy_font_height((char*)msg);
    float fw = tw * scale;
    float fh = th * scale;

    float tx = x + (w - fw) * 0.5f;
    float ty = y + (h - fh) * 0.5f;
    Text(window, tx, ty, msg, r, g, b, scale);
}

}