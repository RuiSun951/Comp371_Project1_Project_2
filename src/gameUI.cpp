#define GLEW_STATIC 1
#include <GL/glew.h>
#include "gameUI.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include "../stb/stb_easy_font.h"


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
    void buildTextTriangles(const char* text, float x, float y, std::vector<float>& outVerts, float& outW, float& outH){
        outVerts.clear();
        static unsigned char buf[200000];
        int quads = stb_easy_font_print(0.0f, 0.0f, (char*)text, nullptr, buf, sizeof(buf));

        float minx =  1e9f, miny =  1e9f;
        float maxx = -1e9f, maxy = -1e9f;

        // each quad has 4 verts; each vert is 16 bytes: float x,y then 8 bytes we ignore
        for (int i = 0; i < quads; ++i) {
            const unsigned char* q = buf + i * 4 * 16;

            auto v2 = [&](int idx)->std::pair<float,float> {
                const float* p = (const float*)(q + idx * 16);
                return { p[0], p[1] };
            };

            auto [x0,y0] = v2(0);
            auto [x1,y1] = v2(1);
            auto [x2,y2] = v2(2);
            auto [x3,y3] = v2(3);

            // track bounds
            minx = std::min(minx, std::min(std::min(x0,x1), std::min(x2,x3)));
            maxx = std::max(maxx, std::max(std::max(x0,x1), std::max(x2,x3)));
            miny = std::min(miny, std::min(std::min(y0,y1), std::min(y2,y3)));
            maxy = std::max(maxy, std::max(std::max(y0,y1), std::max(y2,y3)));

            // two triangles: (0,1,2) (0,2,3)
            float tri[12] = { x0,y0, x1,y1, x2,y2,  x0,y0, x2,y2, x3,y3 };
            // offset by desired x,y
            for (int k = 0; k < 12; k += 2) {
                outVerts.push_back(tri[k]   + x);
                outVerts.push_back(tri[k+1] + y);
            }
        }

        outW = (quads > 0) ? (maxx - minx) : 0.0f;
        outH = (quads > 0) ? (maxy - miny) : 0.0f;
    }
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
    int fbw = 0, fbh = 0; glfwGetFramebufferSize(window, &fbw, &fbh);

    double mx, my; glfwGetCursorPos(window, &mx, &my);
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    float sx = ww ? (float)fbw / (float)ww : 1.0f;
    float sy = wh ? (float)fbh / (float)wh : 1.0f;
    mx *= sx; my *= sy;

    bool hover = (mx >= x && mx <= x + w && my >= y && my <= y + h);

    float proj[16];
    makeOrtho(0.0f, (float)fbw, 0.0f, (float)fbh, -1.0f, 1.0f, proj);

    float verts[8] = { x, y,  x + w, y,  x + w, y + h,  x, y + h };

    // LIGHT GRAY fills for contrast
    float base[4];
    if (enabled) {
        if (hover) { base[0]=0.93f; base[1]=0.93f; base[2]=0.93f; base[3]=1.0f; } // hover
        else       { base[0]=0.85f; base[1]=0.85f; base[2]=0.85f; base[3]=1.0f; } // idle
    } else {
        base[0]=0.70f; base[1]=0.70f; base[2]=0.70f; base[3]=1.0f;               // disabled
    }
    float border[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMaskWas; glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWas);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(uiProg);
    GLint locP = glGetUniformLocation(uiProg, "uProj");
    GLint locC = glGetUniformLocation(uiProg, "uColor");
    glUniformMatrix4fv(locP, 1, GL_FALSE, proj);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    // fill
    glUniform4fv(locC, 1, base);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    // border
    glUniform4fv(locC, 1, border);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool clicked = (enabled && hover && down && !prevMouseDown);
    prevMouseDown = down;

    // restore state
    if (depthMaskWas) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
    if (cullWas)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);

    return clicked;
}


void Text(GLFWwindow* window, float x, float y, float w, float h, const char* text) {
    int fbw = 0, fbh = 0; 
    glfwGetFramebufferSize(window, &fbw, &fbh);

    // Build triangles at origin to measure unscaled size
    std::vector<float> tri;
    float tw = 0.0f, th = 0.0f;
    buildTextTriangles(text, 0.0f, 0.0f, tri, tw, th);

    // Guard against empty strings
    if (tw <= 0.0f || th <= 0.0f || tri.empty()) return;

    // Scale to fit inside the rect with 10% padding
    const float margin = 0.90f; // 90% of rect
    float s = margin * std::min(w / tw, h / th);

    // Offset so scaled text is centered in the rect
    float ox = x + (w - tw * s) * 0.5f;
    float oy = y + (h - th * s) * 0.5f;

    // Apply scale + offset
    std::vector<float> tri2(tri.size());
    for (size_t i = 0; i < tri.size(); i += 2) {
        tri2[i + 0] = tri[i + 0] * s + ox;
        tri2[i + 1] = tri[i + 1] * s + oy;
    }

    float proj[16];
    makeOrtho(0.0f, (float)fbw, 0.0f, (float)fbh, -1.0f, 1.0f, proj);

    // Draw on top: no depth/cull, don't write depth
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMaskWas; glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWas);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(uiProg);
    GLint locP = glGetUniformLocation(uiProg, "uProj");
    GLint locC = glGetUniformLocation(uiProg, "uColor");
    glUniformMatrix4fv(locP, 1, GL_FALSE, proj);

    // Opaque black text
    float col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glUniform4fv(locC, 1, col);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, tri2.size() * sizeof(float), tri2.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(tri2.size() / 2));

    // Restore state
    if (depthMaskWas) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
    if (cullWas)  glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

}