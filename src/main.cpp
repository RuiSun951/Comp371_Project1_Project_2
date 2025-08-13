#include "../OBJloader.h" // from Tut05
#include <vector>
#include <GL/glew.h>
#include "Vertex.h" // from src/Vertex.h
#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.inl>
#include "camera.h" // from src/camera.h
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "SceneNode.h" // from src/SceneNode.h
#include <cmath>
#include "gameUI.h"
#include <cstdio>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#define STB_EASY_FONT_IMPLEMENTATION
#include "../stb/stb_easy_font.h"


// Global variables
// gameUI variables
enum class gameMode { MENU, VIEW, GAME, GAME_OVER };
gameMode appMode = gameMode::MENU;
bool   lPressedLast = false;
static bool mouseDownLastFrame = false;
static bool gameOverPrinted     = false;

// Load shader source code from a file
std::string loadShaderSource(const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compilation error:\n" << log << std::endl;
    }

    return shader;
}

GLuint createShaderProgram(const char* vertPath, const char* fragPath) {
    std::string vertCode = loadShaderSource(vertPath);
    std::string fragCode = loadShaderSource(fragPath);
    
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());

    GLuint sceneProgram = glCreateProgram();
    glAttachShader(sceneProgram, vertexShader);
    glAttachShader(sceneProgram, fragmentShader);
    glLinkProgram(sceneProgram);

    // Check for linking errors
    GLint success;
    glGetProgramiv(sceneProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(sceneProgram, 512, nullptr, log);
        std::cerr << "Shader program linking error:\n" << log << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return sceneProgram;
}

// Game state
int    shotsLeft   = 3;
int    totalScore  = 0;
bool   firePressedLast = false;

// --- Laser as a screen-space quad ---
GLuint laserVAO = 0, laserVBO = 0;
GLuint laserProg = 0;
const  float LASER_PIXELS   = 6.0f;  // thickness in pixels

static const char* LASER_VERT = R"(#version 330 core
layout (location=0) in vec3 aPosNDC; // already in NDC (-1..1)
void main() { gl_Position = vec4(aPosNDC, 1.0); }
)";

static const char* LASER_FRAG = R"(#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() { FragColor = vec4(uColor, 1.0); }
)";

bool   laserActive  = false;
float  laserTimer   = 0.0f;   // default 0 second to show the laser beam
glm::vec3 laserA(0), laserB(0);
static bool rayHitsSphere(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& center, float radius)
{
    // Ray-sphere: |ro + t rd - c|^2 = r^2, solve for t>=0
    glm::vec3 oc = ro - center;
    float b = glm::dot(oc, rd);
    float c = glm::dot(oc, oc) - radius*radius;
    float disc = b*b - c;
    if (disc < 0.0f) return false;
    float t = -b - sqrtf(disc);
    return t >= 0.0f;
}

GLuint laserProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Laser program link error:\n" << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// laser fine tuning
const float LASER_DURATION = 0.50f;
const glm::vec3 LASER_COLOR(0.10f, 1.00f, 0.25f);   // bright neon green

// Crosshair for shooting
GLuint crossVAO = 0, crossVBO = 0;

// Hit radius
const float R_SUN   = 1.5f;
const float R_EARTH = 1.0f;
const float R_MARS  = 0.4f;
const float R_MOON  = 0.5f;
const float R_STATION = 0.3f;
const float R_SHOOTING_STAR = 0.1f; // shooting star hit radius

// Scores
const int SCORE_SUN   = 3;
const int SCORE_EARTH = 5;
const int SCORE_MARS  = 7;
const int SCORE_MOON  = 6;
const int SCORE_STATION = 8;
const int SCORE_SHOOTING_STAR = 15; 

static inline glm::vec3 extractTranslation(const glm::mat4& M) {
    return glm::vec3(M[3][0], M[3][1], M[3][2]);
}

// time control toggle
bool capsPressedLastFrame = false;
bool timeControlOn       = false;
float timeSpeed          = 1.0f;
const float normalSpeed  = 1.0f;
const float fastSpeed    = 3.0f;

// background galaxy toggle
bool pPressedLastFrame = false;
bool renderGalaxy      = true;

//fine tune the speed of the simulation
const float DAYS_PER_SECOND   = 1.0f;
const float SUN_DAY           = 27.0f;   
const float EARTH_DAY         = 1.0f;   
const float EARTH_YEAR        = 365.0f; 
const float MARS_DAY          = 1.03f;  
const float MARS_YEAR         = 687.0f;  
const float MOON_MONTH        = 27.3f;


// Vertex structure for the sphere model
std::vector<Vertex> sphereVertices;
std::vector<unsigned int> sphereIndices;
GLuint sphereVAO, sphereVBO, sphereEBO;

// Station mesh buffers
std::vector<Vertex> stationVertices;
std::vector<unsigned int> stationIndices;
GLuint stationVAO = 0, stationVBO = 0, stationEBO = 0;


// Orbit line variables declare
// this is generating orbit traces for ez observation in testing
const float PLANET_A_ORBIT_RADIUS = 5.0f;
const float PLANET_B_ORBIT_RADIUS = 3.0f;
const float MOON_ORBIT_RADIUS     = 2.0f;

GLuint planetAOrbitVAO, planetAOrbitVBO;
GLuint planetBOrbitVAO, planetBOrbitVBO;
const int ORBIT_SEGMENTS = 100;
std::vector<glm::vec3> planetAOrbitVertices;
std::vector<glm::vec3> planetBOrbitVertices;

GLuint moonOrbitVAO, moonOrbitVBO;
std::vector<glm::vec3> moonOrbitVertices;

// shooting star as a dynamic lighting source
// for shooting star trail
std::vector<glm::vec3> trailPositions;
const int TRAIL_LENGTH=300;
GLuint trailVAO, trailVBO;

//ground plane VAO,VBO,EBO for vasting shadows on
GLuint groundVAO=0, groundVBO=0, groundEBO=0;

// Load the sphere model from an OBJ file
bool loadSphereModel(const char *path) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> indices;

    if (!loadOBJ(path, positions, normals, uvs, indices)) {
        std::cerr << "Failed to load " << path << std::endl;
        return false;
    }

    struct PackedVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;

        bool operator==(const PackedVertex& other) const {
            return position == other.position && normal == other.normal && texCoord == other.texCoord;
        }
    };

    struct Hasher {
        size_t operator()(const PackedVertex& v) const {
            size_t h1 = std::hash<float>()(v.position.x) ^ std::hash<float>()(v.position.y) ^ std::hash<float>()(v.position.z);
            size_t h2 = std::hash<float>()(v.normal.x) ^ std::hash<float>()(v.normal.y) ^ std::hash<float>()(v.normal.z);
            size_t h3 = std::hash<float>()(v.texCoord.x) ^ std::hash<float>()(v.texCoord.y);
            return h1 ^ h2 ^ h3;
        }
    };

    std::unordered_map<PackedVertex, unsigned int, Hasher> vertexToIndex;
    sphereVertices.clear();
    sphereIndices.clear();

    for (size_t i = 0; i < positions.size(); ++i) {
        PackedVertex packed = {
            positions[i],
            (i < normals.size()) ? normals[i] : glm::vec3(0.0f),
            (i < uvs.size()) ? uvs[i] : glm::vec2(0.0f)
        };

        auto it = vertexToIndex.find(packed);
        if (it != vertexToIndex.end()) {
            sphereIndices.push_back(it->second);
        } else {
            sphereVertices.push_back({ packed.position, packed.texCoord, packed.normal });
            unsigned int newIndex = static_cast<unsigned int>(sphereVertices.size() - 1);
            vertexToIndex[packed] = newIndex;
            sphereIndices.push_back(newIndex);
        }
    }

    // Upload to GPU
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);

    glBindVertexArray(sphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(Vertex), sphereVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned int), sphereIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return true;

}

bool loadModelToBuffers(const char* path,
                        GLuint& VAO, GLuint& VBO, GLuint& EBO,
                        std::vector<Vertex>& vertices,
                        std::vector<unsigned int>& indices) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> objIndices;

    if (!loadOBJ(path, positions, normals, uvs, objIndices)) {
        std::cerr << "Failed to load " << path << std::endl;
        return false;
    }

    struct PackedVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        bool operator==(const PackedVertex& other) const {
            return position == other.position &&
                   normal == other.normal &&
                   texCoord == other.texCoord;
        }
    };

    struct Hasher {
        size_t operator()(const PackedVertex& v) const {
            size_t h1 = std::hash<float>()(v.position.x) ^ std::hash<float>()(v.position.y) ^ std::hash<float>()(v.position.z);
            size_t h2 = std::hash<float>()(v.normal.x) ^ std::hash<float>()(v.normal.y) ^ std::hash<float>()(v.normal.z);
            size_t h3 = std::hash<float>()(v.texCoord.x) ^ std::hash<float>()(v.texCoord.y);
            return h1 ^ h2 ^ h3;
        }
    };

    std::unordered_map<PackedVertex, unsigned int, Hasher> vertexToIndex;
    vertices.clear();
    indices.clear();

    for (size_t i = 0; i < positions.size(); ++i) {
        PackedVertex packed = {
            positions[i],
            (i < normals.size()) ? normals[i] : glm::vec3(0.0f),
            (i < uvs.size()) ? uvs[i] : glm::vec2(0.0f)
        };

        auto it = vertexToIndex.find(packed);
        if (it != vertexToIndex.end()) {
            indices.push_back(it->second);
        } else {
            vertices.push_back({ packed.position, packed.texCoord, packed.normal });
            unsigned int newIndex = static_cast<unsigned int>(vertices.size() - 1);
            vertexToIndex[packed] = newIndex;
            indices.push_back(newIndex);
        }
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return true;
}


//set up camera
bool tabPressedLastFrame = false;
const float cameraSpeed = 2.5f;
const float cameraFastSpeed = 6.0f;

//Yibo Tang
void drawSphere() {
    //glBindVertexArray(sphereVAO);
    //glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
    //glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size());
    //glBindVertexArray(0);//
}

// depth-only draw helper for shadow pass
void drawSphereDepth(GLuint prog, const glm::mat4& M, GLuint modelLocShadow){
    glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(M));
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// Yibo Tang: Insert the texture
GLuint loadTexture(const char* filename) {   
    stbi_set_flip_vertically_on_load(true); // Flip the image vertically
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //Load image using stb_image
    int width, height, nrChannels;
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);
    
    if (data) {
        GLenum format;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;
        else {
            std::cerr << "Unsupported channel count in texture: " << filename << std::endl;
            stbi_image_free(data);
            return 0;
        }
        //std::cout << "Loaded texture " << filename << ": " << width << "x" << height << " Channels: " << nrChannels << std::endl;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

//clean up helper function
 void deleteSceneGraph(SceneNode* node){
    for (SceneNode* child : node->children) {
        deleteSceneGraph(child);
    }
    delete node;
}

//spacestation obj model debug helper fuctions
static bool hasAnyUVs(const std::vector<Vertex>& v){
    for (const auto& x : v) if (x.texCoord.x != 0.0f || x.texCoord.y != 0.0f) return true;
    return false;
}
static bool hasAnyNormals(const std::vector<Vertex>& v){
    for (const auto& x : v) if (glm::dot(x.normal , x.normal) > 1e-10f) return true;
    return false;
}
static void generateSphericalUVs(std::vector<Vertex>& v){
    for (auto& x : v){
        glm::vec3 p = glm::normalize(x.position);
        float u = 0.5f + atan2f(p.z, p.x) / (2.0f * glm::pi<float>());
        float vcoord = 0.5f - asinf(p.y) / glm::pi<float>();
        x.texCoord = glm::vec2(u, vcoord);
    }
}
static void recomputeNormals(std::vector<Vertex>& v, const std::vector<unsigned int>& idx){
    for (auto& x : v) x.normal = glm::vec3(0.0f);
    for (size_t i = 0; i + 2 < idx.size(); i += 3){
        Vertex& a = v[idx[i+0]];
        Vertex& b = v[idx[i+1]];
        Vertex& c = v[idx[i+2]];
        glm::vec3 n = glm::normalize(glm::cross(b.position - a.position, c.position - a.position));
        if (!std::isfinite(n.x)) continue;
        a.normal += n; b.normal += n; c.normal += n;
    }
    for (auto& x : v){
        float L2 = glm::dot(x.normal, x.normal);
        x.normal = (L2 > 1e-12f) ? glm::normalize(x.normal) : glm::vec3(0,1,0);
    }
}

int main() {
    // Initialize OpenGL context, GLEW, etc. here...
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Create a window
    GLFWwindow* window = glfwCreateWindow(1200, 900, "Mini Solar System", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window); // Make this window the OpenGL context

    // Initialize GLEW after the context
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }

    // wrap game UI
    UI::Init();
    laserProg = laserProgram(LASER_VERT, LASER_FRAG);
    glGenVertexArrays(1, &laserVAO);
    glGenBuffers(1, &laserVBO);
    glBindVertexArray(laserVAO);
    glBindBuffer(GL_ARRAY_BUFFER, laserVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 6, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Crosshair VAO/VBO (4 verts = 2 lines)
    glGenVertexArrays(1, &crossVAO);
    glGenBuffers(1, &crossVBO);
    glBindVertexArray(crossVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crossVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Enable back face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); // Counter-clockwise is default winding

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Shadow map1 setup for static up right corner light
    const GLuint SHADOW_W = 2048, SHADOW_H = 2048;
    GLuint depthFBO, depthTex;
    glGenFramebuffers(1, &depthFBO);
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                SHADOW_W, SHADOW_H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderCol[4] = {1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderCol);

    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Shadow map2 setup for dynamic shooting star light
    GLuint depthCubeFBO, depthCubeTex;
    glGenFramebuffers(1, &depthCubeFBO);
    glGenTextures(1, &depthCubeTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeTex);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                    SHADOW_W, SHADOW_H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, depthCubeFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubeTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // Create shadow shader program for light 1
    GLuint shadowProgram = createShaderProgram("shaders/shadow_vertex.glsl", "shaders/shadow_fragment.glsl");
   
    // Create point-light shadow (cubemap) program for light 2
    GLuint pointShadowProgram = createShaderProgram("shaders/pointShadow_vertex.glsl", "shaders/pointShadow_fragment.glsl");

    // Create scene shader program
    GLuint sceneProgram = createShaderProgram("shaders/vertexShader.glsl", "shaders/fragmentShader.glsl");
    glUseProgram(sceneProgram);

    // uniform declareation for sceneProgram
    const GLint uModel        = glGetUniformLocation(sceneProgram, "model");
    const GLint uView         = glGetUniformLocation(sceneProgram, "view");
    const GLint uProj         = glGetUniformLocation(sceneProgram, "projection");
    const GLint uUseLighting  = glGetUniformLocation(sceneProgram, "useLighting");
    const GLint uUseTexture   = glGetUniformLocation(sceneProgram, "useTexture");
    const GLint uViewPos      = glGetUniformLocation(sceneProgram, "viewPos");
    const GLint uObjectColor  = glGetUniformLocation(sceneProgram, "objectColor");

    const GLint uLightPos1    = glGetUniformLocation(sceneProgram, "lightPos1");
    const GLint uLightColor1  = glGetUniformLocation(sceneProgram, "lightColor1");
    const GLint uLightPos2    = glGetUniformLocation(sceneProgram, "lightPos2");
    const GLint uLightColor2  = glGetUniformLocation(sceneProgram, "lightColor2");

    const GLint uLightSpace   = glGetUniformLocation(sceneProgram, "lightSpaceMatrix");
    const GLint uFarPlane2    = glGetUniformLocation(sceneProgram, "farPlane2");

    // Samplers
    const GLint uTex1         = glGetUniformLocation(sceneProgram, "texture1");
    const GLint uShadowMap    = glGetUniformLocation(sceneProgram, "shadowMap");
    const GLint uShadowCube2  = glGetUniformLocation(sceneProgram, "shadowCube2");
    const GLint uReceiveShadows = glGetUniformLocation(sceneProgram, "receiveShadows");

    // Set fixed sampler bindings
    glUniform1i(uTex1,        0); // GL_TEXTURE0
    glUniform1i(uShadowMap,   1); // GL_TEXTURE1
    glUniform1i(uShadowCube2, 2); // GL_TEXTURE2

    // Load the textures
    GLuint sunTexture = loadTexture("texture/sun.jpg");
    GLuint earthTexture = loadTexture("texture/earth.jpg");
    GLuint marsTexture = loadTexture("texture/mars.jpg");
    GLuint moonTexture = loadTexture("texture/moon.jpg");
    GLuint galaxyTexture = loadTexture("texture/galaxy.jpg");

    // Load the sphere model from OBJ file
    if (!loadSphereModel("models/sphere.obj")) {
        return -1; 
    }
    // Loadd the spacestation model from OBJ file
    if (!loadModelToBuffers("models/spacestation.obj", stationVAO, stationVBO, stationEBO, stationVertices, stationIndices)) {
        std::cerr << "Failed to load spacestation.obj\n";
    }
    
    // Fix station data if OBJ lacked normals/UVs or indices didn't align
    bool needReupload = false;
    if (!hasAnyNormals(stationVertices)) {
        recomputeNormals(stationVertices, stationIndices);
        needReupload = true;
    }
    if (!hasAnyUVs(stationVertices)) {
        generateSphericalUVs(stationVertices);
        needReupload = true;
    }
    if (needReupload) {
        glBindBuffer(GL_ARRAY_BUFFER, stationVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            stationVertices.size() * sizeof(Vertex), stationVertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    //set up orbit traces ellipses? circles
    for (int i = 0; i <= ORBIT_SEGMENTS; ++i) {
        float angle = 2.0f * glm::pi<float>() * i / ORBIT_SEGMENTS;
        planetAOrbitVertices.push_back(glm::vec3(PLANET_A_ORBIT_RADIUS * cos(angle), 0.0f, PLANET_A_ORBIT_RADIUS * sin(angle)));
        planetBOrbitVertices.push_back(glm::vec3(PLANET_B_ORBIT_RADIUS * cos(angle), 0.0f, PLANET_B_ORBIT_RADIUS * sin(angle)));
        moonOrbitVertices.push_back(glm::vec3(MOON_ORBIT_RADIUS * cos(angle), 0.0f, MOON_ORBIT_RADIUS * sin(angle)));
    }

    // Upload to GPU
    // planetA orbit
    glGenVertexArrays(1, &planetAOrbitVAO);
    glGenBuffers(1, &planetAOrbitVBO);
    glBindVertexArray(planetAOrbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planetAOrbitVBO);
    glBufferData(GL_ARRAY_BUFFER, planetAOrbitVertices.size() * sizeof(glm::vec3), planetAOrbitVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // planetB orbit
    glGenVertexArrays(1, &planetBOrbitVAO);
    glGenBuffers(1, &planetBOrbitVBO);
    glBindVertexArray(planetBOrbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planetBOrbitVBO);
    glBufferData(GL_ARRAY_BUFFER, planetBOrbitVertices.size() * sizeof(glm::vec3), planetBOrbitVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // moon orbit
    glGenVertexArrays(1, &moonOrbitVAO);
    glGenBuffers(1, &moonOrbitVBO);
    glBindVertexArray(moonOrbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, moonOrbitVBO);
    glBufferData(GL_ARRAY_BUFFER, moonOrbitVertices.size() * sizeof(glm::vec3), moonOrbitVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // shooting star trail
    glGenVertexArrays(1, &trailVAO);
    glGenBuffers(1, &trailVBO);
    glBindVertexArray(trailVAO);
    glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
    glBufferData(GL_ARRAY_BUFFER, TRAIL_LENGTH * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    if (trailPositions.size() > TRAIL_LENGTH)
        trailPositions.erase(trailPositions.begin());   //cap trail size
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    {
        struct GVert { glm::vec3 p; glm::vec2 uv; glm::vec3 n; };
        const float S=100.f, Y=-1.f;
        GVert v[4] = {
            {{-S,Y,-S},{0,0},{0,1,0}},
            {{ S,Y,-S},{1,0},{0,1,0}},
            {{ S,Y, S},{1,1},{0,1,0}},
            {{-S,Y, S},{0,1},{0,1,0}},
        };
        unsigned int idx[6]={0,1,2, 0,2,3};

        glGenVertexArrays(1,&groundVAO);
        glGenBuffers(1,&groundVBO);
        glGenBuffers(1,&groundEBO);

        glBindVertexArray(groundVAO);
        glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GVert),(void*)offsetof(GVert,p));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(GVert),(void*)offsetof(GVert,uv));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(GVert),(void*)offsetof(GVert,n));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    // Set background color
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // pure white background
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    //disable cursor


    // camera setup
    Camera camera(
        glm::vec3(0.0f, 1.0f, 10.0f),  // camera position
        glm::vec3(0.0f, 1.0f, 0.0f),  // camera up
        -90.0f,                       // yaw
        0.0f                          // pitch
    );

    SceneNode* root = new SceneNode();  //identity node
    SceneNode* galaxy = new SceneNode();
    SceneNode* sun = new SceneNode();   //separate sun from identity
    SceneNode* planetB = new SceneNode();
    SceneNode* shootingStar = new SceneNode();
    SceneNode* planetA_orbit = new SceneNode();
    SceneNode* planetA_body = new SceneNode();
    SceneNode* moon = new SceneNode();
    SceneNode* station = new SceneNode();

    // initial size + offset from Earth
    station->localTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));

    root->addChild(sun);
    root->addChild(galaxy);
    root->addChild(planetA_orbit);
    planetA_orbit ->addChild(planetA_body);
    planetA_orbit ->addChild(moon);
    planetA_body ->addChild(station);  // attach to Earth
    root->addChild(planetB);
    root->addChild(shootingStar);


    // Now modelLoc is valid here:
    //root->drawFunc = [&](const glm::mat4& model) {
    //glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    // drawSphere();

    sun->drawFunc = [&](const glm::mat4& model) {
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 0);   // light source, no lighting
        glUniform1i(uUseTexture,  1);
        glUniform1i(uReceiveShadows, 0);    // do not receive shadows
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);      // reset tint
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTexture);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };


    planetA_body->drawFunc = [&](const glm::mat4& model) {
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 1);
        glUniform1i(uUseTexture,  1);
        glUniform1i(uReceiveShadows, 1);
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);      // reset tint
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, earthTexture);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    planetB->drawFunc = [&](const glm::mat4& model) {
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 1);
        glUniform1i(uUseTexture,  1);
        glUniform1i(uReceiveShadows, 1);
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);      // reset tint
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, marsTexture);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    shootingStar->drawFunc = [&](const glm::mat4& model) {
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 0); // light source, no lighting
        glUniform1i(uReceiveShadows, 0); // do not receive shadows
        glUniform1i(uUseTexture,  0);
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);      // reset tint
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    moon->drawFunc = [&](const glm::mat4& model) {
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 1);
        glUniform1i(uUseTexture,  1);
        glUniform1i(uReceiveShadows, 1);
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);      // reset tint
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, moonTexture);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    station->drawFunc = [&](const glm::mat4& model){
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 1);
        glUniform1i(uUseTexture,  0); //no texture
        glUniform1i(uReceiveShadows, 1);
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));

        float t = (float)glfwGetTime();
        glm::vec3 color = 0.5f + 0.5f * glm::vec3(
            sin(t*1.7f), sin(t*2.3f + 1.0f), sin(t*2.9f + 2.0f));
        glUniform3f(uObjectColor, color.x, color.y, color.z);

        // do NOT bind stationTexture anymore
        glBindVertexArray(stationVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)stationIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    // Time control factor
    float timeScale = 0.2f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float simTime =0.0f;

    // main render loop
    while (!glfwWindowShouldClose(window)) {
        int fbw = 0, fbh = 0;   //later use for view/proj

        glfwPollEvents();

        if (renderGalaxy) glClearColor(0.10f, 0.10f, 0.10f, 1.0f);   // dark gray when galaxy on, for immersive background
        else              glClearColor(0.25f, 0.25f, 0.25f, 1.0f);

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // game menu mode control
        if (appMode == gameMode::MENU) {
            // Always show galaxy as background
            renderGalaxy = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            
            // bug fix: set view/proj before drawing the galaxy once
            fbw = 0, fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glm::mat4 viewM = camera.getViewMatrix();
            glm::mat4 projM = glm::perspective(glm::radians(45.0f), (float)fbw / (float)fbh, 0.1f, 200.0f);
            glUseProgram(sceneProgram);
            glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(viewM));
            glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projM));
            glm::vec3 camPosM = camera.getPosition();
            glUniform3f(uViewPos, camPosM.x, camPosM.y, camPosM.z);

            {
                glm::mat4 galaxyTransform =
                    glm::scale(glm::translate(glm::mat4(1.0f), camera.getPosition()), glm::vec3(50.0f));

                glDisable(GL_DEPTH_TEST);

                // If the texture is valid, render inside-out sphere.
                if (galaxyTexture) {
                    glCullFace(GL_FRONT);
                    glUseProgram(sceneProgram);
                    glUniform1i(uUseLighting, 0);
                    glUniform1i(uUseTexture,  1);
                    glUniform1i(uReceiveShadows, 0);
                    glUniform3f(uObjectColor, 1,1,1);
                    glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(galaxyTransform));
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, galaxyTexture);
                    glBindVertexArray(sphereVAO);
                    glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);
                    glCullFace(GL_BACK);
                } else {
                    // Fallback: just clear to a darker color
                    glClearColor(0.03f, 0.03f, 0.05f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                glEnable(GL_DEPTH_TEST);
            }

            // Two centered buttons (top = VIEW MODE, bottom = GAME MODE)
            fbw=0, fbh=0; 
            glfwGetFramebufferSize(window, &fbw, &fbh);
            float bw = 520.0f, bh = 120.0f;
            float cx = fbw * 0.5f - bw * 0.5f;
            float cy = fbh * 0.5f - bh * 0.5f;
            float pad = 20.0f;

            UI::Button(window, cx, cy - (bh + pad), bw, bh, true);
            UI::Button(window, cx, cy + (bh + pad), bw, bh, true);
            UI::Text  (window, cx, cy - (bh + pad), bw, bh, "VIEW MODE");
            UI::Text  (window, cx, cy + (bh + pad), bw, bh, "GAME MODE");

            // DPI-aware hit test: convert window coords -> framebuffer coords
            int winW=0, winH=0; 
            glfwGetWindowSize(window, &winW, &winH);
            int fbw2=0, fbh2=0; 
            glfwGetFramebufferSize(window, &fbw2, &fbh2);
            double scaleX = (winW>0) ? double(fbw2)/double(winW) : 1.0;
            double scaleY = (winH>0) ? double(fbh2)/double(winH) : 1.0;

            double mx=0.0, my=0.0; glfwGetCursorPos(window, &mx, &my);
            double mxFB = mx * scaleX, myFB = my * scaleY;

            auto inside = [](double x,double y,double rx,double ry,double rw,double rh){
                return x>=rx && x<=rx+rw && y>=ry && y<=ry+rh;
            };

            static bool mouseDownLastFrame = false; // local static for edge-trigger
            bool mouseDownNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

            bool viewHover = inside(mxFB, myFB, cx,              cy - (bh + pad), bw, bh);
            bool gameHover = inside(mxFB, myFB, cx,              cy + (bh + pad), bw, bh);

            bool startView = (mouseDownNow && !mouseDownLastFrame && viewHover);
            bool startGame = (mouseDownNow && !mouseDownLastFrame && gameHover);
            mouseDownLastFrame = mouseDownNow;

            if (startView) {
                appMode = gameMode::VIEW;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            if (startGame) {
                appMode = gameMode::GAME;
                shotsLeft  = 3;
                totalScore = 0;
                laserActive = false;
                laserTimer  = 0.0f;
                firePressedLast = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            // Finish this frame early (don’t run normal scene)
            glfwSwapBuffers(window);
            continue;
        }

        // mouse control
        deltaTime = 0.0f;
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // check time speed toggle by CAPS
        bool capsPressedNow = glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS;
        if (capsPressedNow && !capsPressedLastFrame) {
            timeControlOn = !timeControlOn;
            timeSpeed     = timeControlOn ? fastSpeed : normalSpeed;
        }
        capsPressedLastFrame = capsPressedNow;

        if (deltaTime > 0.01f)  // cap movement speed of wasd
            deltaTime = 0.01f;

        camera.update(window, deltaTime);

        // camera view: toggle fpp tpp
        bool tabPressedNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabPressedNow && !tabPressedLastFrame) {
            camera.toggleMode();
        }
        tabPressedLastFrame = tabPressedNow;

        // background galaxy toggle on/off with 'P' — only in VIEW mode
        if (appMode == gameMode::VIEW) {
            bool pPressedNow = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
            if (pPressedNow && !pPressedLastFrame) {
                renderGalaxy = !renderGalaxy;
            }
            pPressedLastFrame = pPressedNow;
        } else {
            // consume current key state so returning to VIEW won't immediately toggle
            pPressedLastFrame = (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS);
        }


        bool lNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
        if (lNow && !lPressedLast) {
            if (appMode == gameMode::VIEW || appMode == gameMode::GAME || appMode == gameMode::GAME_OVER) {
                appMode = gameMode::MENU;
                // show cursor again in menu
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                // reset session variables for a fresh start
                shotsLeft   = 3;
                totalScore  = 0;
                laserActive = false;
                laserTimer  = 0.0f;
                // allow GAME OVER print again
                // (the printed static in step 6 will re-arm on next run)
            }
        }
        lPressedLast = lNow;


        // Create transformation matrices
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view  = camera.getViewMatrix();
        // Use actual framebuffer size for aspect ratio
        fbw = 1, fbh = 1;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        float aspect = (fbh > 0) ? (float)fbw / (float)fbh  : 1.0f;
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            aspect,
            0.1f,
            100.0f
        );
        glUseProgram(sceneProgram);

        // Update camera/view uniforms
        glm::vec3 camPos = camera.getPosition();
        glUniform3f(uViewPos, camPos.x, camPos.y, camPos.z);

        // update galaxy transform every frame so it follows camera
        galaxy->localTransform = glm::scale(glm::translate(glm::mat4(1.0f), camera.getPosition()), glm::vec3(50.0f));

        // Light 1: static top-right corner -ish
        glm::vec3 lightPos1 = glm::vec3(10.0f, 10.0f, 10.0f);
        glm::vec3 lightColor1 = glm::vec3(1.0f);

        // Light 2: dynamic shooting star across the sky, single direction shooting, loop periodically
        float angle = simTime * 0.5f;
        glm::vec3 lightPos2 = glm::vec3(0.0f , 8.0f * cos(angle), 8.0f * sin(angle));
        glm::vec3 lightColor2 = glm::vec3(1.0f, 1.0f, 1.0f);  // shooting star bright full white for better seeing in testing

        // build light-space matrix for light 1 shadow (from lightPos1 toward origin)
        glm::mat4 lightProj = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 0.1f, 60.0f);
        glm::mat4 lightView = glm::lookAt(lightPos1, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4 lightSpaceMatrix = lightProj * lightView;

        // light 2 shadow setup 
        float nearPL = 0.1f, farPL = 100.0f; 
        glm::mat4 shadowProj2 = glm::perspective(glm::radians(90.0f), 1.0f, nearPL, farPL);
        glm::vec3 lp = lightPos2;
        glm::mat4 views2[6] = {
            glm::lookAt(lp, lp + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)), // +X
            glm::lookAt(lp, lp + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)), // -X
            glm::lookAt(lp, lp + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)), // +Y
            glm::lookAt(lp, lp + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)), // -Y
            glm::lookAt(lp, lp + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)), // +Z
            glm::lookAt(lp, lp + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0))  // -Z
        };

        //Animate Orbit Around Earth
        float r = 2.0f, w = glm::radians(10.0f), t = glfwGetTime();
        station->localTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(cos(t*w)*r, 0.0f, sin(t*w)*r)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.025f));

        // Update shooting star's transform
        glm::mat4 starTransform = glm::translate(glm::mat4(1.0f), lightPos2);
        starTransform = glm::scale(starTransform, glm::vec3(0.1f));
        shootingStar->localTransform = starTransform;
        
        // Update trail positions
        trailPositions.push_back(lightPos2);
        if (trailPositions.size() > TRAIL_LENGTH)
            trailPositions.erase(trailPositions.begin());

        glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, trailPositions.size() * sizeof(glm::vec3), trailPositions.data());

        // upload 2 lights to shader
        glUniform3fv(uLightPos1,   1, glm::value_ptr(lightPos1));
        glUniform3fv(uLightColor1, 1, glm::value_ptr(lightColor1));
        glUniform3fv(uLightPos2,   1, glm::value_ptr(lightPos2));
        glUniform3fv(uLightColor2, 1, glm::value_ptr(lightColor2));

        // Update transformation matrices
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));

        // Draw the sphere (Yibo Tang removed these to avoid overlap of planet diplay)
        //glBindVertexArray(sphereVAO);
        //glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        //glBindVertexArray(0);

        // Yibo Tang: Update transforms (hierarchical scene graph)
        simTime       += deltaTime * timeSpeed;
        float simDays = simTime * DAYS_PER_SECOND;

        // Sun self-rotation (25 days per rotation)
        float sunAngle = simDays / SUN_DAY * glm::two_pi<float>();
        sun->localTransform =
            glm::rotate(glm::mat4(1.0f), sunAngle, glm::vec3(0,1,0))
        * glm::scale(glm::mat4(1.0f), glm::vec3(1.5f));

        // Earth orbit + spin
        float earthOrbitAngle = simDays / EARTH_YEAR * glm::two_pi<float>();
        planetA_orbit->localTransform =
            glm::rotate(glm::mat4(1.0f), earthOrbitAngle, glm::vec3(0,1,0))
        * glm::translate(glm::mat4(1.0f), glm::vec3(PLANET_A_ORBIT_RADIUS,0,0));

        float earthSpinAngle = simDays / EARTH_DAY * glm::two_pi<float>();

        planetA_body->localTransform =
            glm::rotate(glm::mat4(1.0f), earthSpinAngle, glm::vec3(0,1,0));

        // Mars orbit + spin
        float marsOrbitAngle = simDays / MARS_YEAR * glm::two_pi<float>();
        float marsSpinAngle  = simDays / MARS_DAY  * glm::two_pi<float>();   

        planetB->localTransform =
            glm::rotate(glm::mat4(1.0f), marsOrbitAngle, glm::vec3(0,1,0))
            * glm::translate(glm::mat4(1.0f), glm::vec3(PLANET_B_ORBIT_RADIUS,0,0))
            * glm::rotate(glm::mat4(1.0f), marsSpinAngle,  glm::vec3(0,1,0))
            * glm::scale(glm::mat4(1.0f), glm::vec3(0.4f));

        // Moon orbit around Earth
        float moonOrbitAngle = simDays / MOON_MONTH * glm::two_pi<float>();
        moon->localTransform =
            glm::rotate(glm::mat4(1.0f), moonOrbitAngle, glm::vec3(0, 1, 0)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(MOON_ORBIT_RADIUS, 0, 0)) *
            glm::rotate(glm::mat4(1.0f), moonOrbitAngle, glm::vec3(0, 1, 0)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));

        // Compute globals once per frame (after local transforms are updated)
        glm::mat4 earthGlobal = planetA_orbit->getGlobalTransform() * planetA_body->localTransform;
        glm::mat4 moonGlobal  = moon->getGlobalTransform(planetA_orbit->getGlobalTransform());

        // SHADOW DEPTH PASS: LIGHT 1
        glViewport(0, 0, SHADOW_W, SHADOW_H);
        glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(shadowProgram);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 3.0f);

        glUniformMatrix4fv(glGetUniformLocation(shadowProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        GLuint modelLocShadow = glGetUniformLocation(shadowProgram, "model");

        glCullFace(GL_FRONT); // reduce acne

        // Sun
        glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(sun->getGlobalTransform()));
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Earth
        glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(earthGlobal));
        glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Mars
        glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(planetB->getGlobalTransform()));
        glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Moon
        glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(moonGlobal));
        glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Space station shadow
        glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(station->getGlobalTransform()));
        glBindVertexArray(stationVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)stationIndices.size(), GL_UNSIGNED_INT, 0);
        
        //ground
        if (!renderGalaxy) {
            glm::mat4 M = glm::mat4(1.0f);
            glUniformMatrix4fv(modelLocShadow, 1, GL_FALSE, glm::value_ptr(M));
            glBindVertexArray(groundVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); 

        // SHADOW DEPTH PASS: LIGHT 2 (shooting star)
        glViewport(0, 0, SHADOW_W, SHADOW_H);
        glBindFramebuffer(GL_FRAMEBUFFER, depthCubeFBO);
        glUseProgram(pointShadowProgram);

        GLint uVP_PL       = glGetUniformLocation(pointShadowProgram, "vp");
        GLint uModel_PL    = glGetUniformLocation(pointShadowProgram, "model");
        GLint uLightPos_PL = glGetUniformLocation(pointShadowProgram, "lightPos");
        GLint uFarPlane_PL = glGetUniformLocation(pointShadowProgram, "farPlane");

        glUniform3fv(uLightPos_PL, 1, glm::value_ptr(lp));
        glUniform1f(uFarPlane_PL, farPL);

        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        for (int face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depthCubeTex, 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            glm::mat4 vp = shadowProj2 * views2[face];
            glUniformMatrix4fv(uVP_PL, 1, GL_FALSE, glm::value_ptr(vp));

            // Sun
            glUniformMatrix4fv(uModel_PL, 1, GL_FALSE, glm::value_ptr(sun->getGlobalTransform()));
            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Earth
            glUniformMatrix4fv(uModel_PL, 1, GL_FALSE, glm::value_ptr(earthGlobal));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Mars
            glUniformMatrix4fv(uModel_PL, 1, GL_FALSE, glm::value_ptr(planetB->getGlobalTransform()));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Moon
            glUniformMatrix4fv(uModel_PL, 1, GL_FALSE, glm::value_ptr(moonGlobal));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Space station
            glUniformMatrix4fv(uModel_PL, 1, GL_FALSE, glm::value_ptr(station->getGlobalTransform()));
            glBindVertexArray(stationVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)stationIndices.size(), GL_UNSIGNED_INT, 0);

            if (!renderGalaxy) {
                glm::mat4 M = glm::mat4(1.0f);
                glUniformMatrix4fv(uModel_PL,1,GL_FALSE,glm::value_ptr(M));
                glBindVertexArray(groundVAO);
                glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
                glBindVertexArray(0);
            }
        }

        glBindVertexArray(0);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_CULL_FACE); //restore culling

        // Reset viewport to window size
        int fbW, fbH; 
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glUseProgram(sceneProgram);

        // light 1 (directional/spot) shadow matrix
        glUniformMatrix4fv(uLightSpace, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        // shadow textures (sampler units already fixed once)
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTex);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeTex);

        // far plane
        glUniform1f(uFarPlane2, farPL);

        // Draw the trail of the shooting star
        glUniform1i(uUseLighting, 0);
        glUniform1i(uUseTexture,  0);
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(trailVAO);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)trailPositions.size());

        // Draw galaxy background (inside-out sphere, front face culling)
        if (renderGalaxy) {
            int fbw = 0, fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);

            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 proj = glm::perspective(glm::radians(45.0f),  (float)fbw / (float)fbh, 0.1f, 200.0f);

            glUseProgram(sceneProgram);
            glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(proj));

            glm::vec3 camPos = camera.getPosition();
            glUniform3f(uViewPos, camPos.x, camPos.y, camPos.z);

            glm::mat4 galaxyTransform = glm::scale(glm::translate(glm::mat4(1.0f), camPos), glm::vec3(50.0f));

            glDisable(GL_DEPTH_TEST);
            glCullFace(GL_FRONT);

            glUniform1i(uUseLighting, 0);
            glUniform1i(uUseTexture,  1);
            glUniform1i(uReceiveShadows, 0);
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(galaxyTransform));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, galaxyTexture);

            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glCullFace(GL_BACK);
        }
        
        // Draw orbit lines
        glUseProgram(sceneProgram);
        glUniform1i(uUseLighting, 0);
        glUniform1i(uUseTexture,  0);
        glDisable(GL_CULL_FACE);
        glLineWidth(2.0f); 

        // Planet A orbit (blue)
        glm::mat4 planetAOrbitModel = glm::rotate(glm::mat4(1.0f),earthOrbitAngle,glm::vec3(0,1,0));
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(planetAOrbitModel));
        glUniform3f(uObjectColor, 0.0f, 0.0f, 1.0f);
        glBindVertexArray(planetAOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, planetAOrbitVertices.size());

        // Planet B orbit (red)
        glm::mat4 planetBOrbitModel = glm::rotate(glm::mat4(1.0f), marsOrbitAngle, glm::vec3(0,1,0));        
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(planetBOrbitModel));
        glUniform3f(uObjectColor, 1.0f, 0.0f, 0.0f);
        glBindVertexArray(planetBOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, planetBOrbitVertices.size());

        // Moon orbit (orange)
        glm::mat4 moonOrbitLineM = planetA_orbit->localTransform;
        glBindVertexArray(moonOrbitVAO);
        glUniform3f(uObjectColor, 1.0f, 0.5f, 0.0f);
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(moonOrbitLineM));
        glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(moonOrbitVertices.size()));

        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);  // Re-enable culling
        glUniform1i(uUseLighting, 1);  //turn lighting back on
        glUniform1i(uUseTexture,  1);   //turn texture back on
      
        //reset object color to white for next draw calls
        glUniform3f(uObjectColor, 1.0f, 1.0f, 1.0f);

        // Draw the scene recursively, instead of draw sphere one by one, use root
        root->draw(glm::mat4(1.0f));

        if (appMode == gameMode::GAME) {
            // Draw crosshair: small red cross at window center (screen-space)
            glm::mat4 view3D = view;
            glm::mat4 proj3D = projection;

            glfwGetFramebufferSize(window, &fbW, &fbH);

            const float s = 10.0f;
            const float cx = 0.5f * fbW;
            const float cy = 0.5f * fbH;

            glm::vec3 ch[4] = {
                {cx - s, cy, 0.0f}, {cx + s, cy, 0.0f},
                {cx, cy - s, 0.0f}, {cx, cy + s, 0.0f}
            };
            glBindBuffer(GL_ARRAY_BUFFER, crossVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ch), ch);

            glm::mat4 viewHUD = glm::mat4(1.0f);
            glm::mat4 projHUD = glm::ortho(0.0f, (float)fbW, (float)fbH, 0.0f, -1.0f, 1.0f);

            glUseProgram(sceneProgram);
            glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(viewHUD));
            glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projHUD));
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
            glUniform1i(uUseLighting, 0);
            glUniform1i(uUseTexture,  0);
            glUniform1i(uReceiveShadows, 0);
            glUniform3f(uObjectColor, 1.0f, 0.0f, 0.0f);

            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(crossVAO);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, 4);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);

            // restore 3D matrices
            glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view3D));
            glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(proj3D));

            UI::Button(window, 20, 20, 220, 40, false);
            UI::Button(window, 20, 70, 220, 40, false);
            std::string s1 = "Shots Left: " + std::to_string(shotsLeft);
            std::string s2 = "Score: "      + std::to_string(totalScore);
            UI::Text(window, 20, 20, 220, 40, s1.c_str());
            UI::Text(window, 20, 70, 220, 40, s2.c_str());
        }


        // Edge-trigger fire on LMB (GAME only)
        if (appMode == gameMode::GAME) {
            bool fireNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (fireNow && !firePressedLast && shotsLeft > 0) {
                glm::vec3 ro = camera.getPosition();
                glm::vec3 rd = glm::normalize(camera.getFront());

                glm::vec3 cSun     = extractTranslation(sun->getGlobalTransform());
                glm::vec3 cEarth   = extractTranslation(planetA_orbit->getGlobalTransform() * planetA_body->localTransform);
                glm::vec3 cMars    = extractTranslation(planetB->getGlobalTransform());
                glm::vec3 cMoon    = extractTranslation(moon->getGlobalTransform(planetA_orbit->getGlobalTransform()));
                glm::vec3 cStation = extractTranslation(station->getGlobalTransform());

                int gained = 0;
                if (rayHitsSphere(ro, rd, cSun, R_SUN))         gained += SCORE_SUN;
                if (rayHitsSphere(ro, rd, cEarth, R_EARTH))     gained += SCORE_EARTH;
                if (rayHitsSphere(ro, rd, cMars, R_MARS))       gained += SCORE_MARS;
                if (rayHitsSphere(ro, rd, cMoon, R_MOON))       gained += SCORE_MOON;
                if (rayHitsSphere(ro, rd, cStation, R_STATION)) gained += SCORE_STATION;

                totalScore += gained;
                shotsLeft  -= 1;

                const float EPS = 0.20f;                 // small offset to avoid near-plane clipping
                glm::vec3 start = ro + rd * EPS;         // start just in front of the camera
                glm::vec3 end   = ro + rd * 100.0f;      // keep the far end
                
                laserA = start;
                laserB = end;
                laserActive = true;
                laserTimer  = LASER_DURATION;
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                
                if (shotsLeft == 0) {
                    appMode = gameMode::GAME_OVER;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    firePressedLast = false;
                }
            }
            firePressedLast = fireNow;
        } else {
            // ensure no latent edge-trigger carries into other modes
            firePressedLast = false;
        }


        // Draw laser while active (GAME only) — screen-space quad, constant pixel thickness
        if (appMode == gameMode::GAME && laserActive) {
            laserTimer -= deltaTime;
            if (laserTimer <= 0.0f) laserActive = false;

            // Project endpoints to NDC
            glm::vec4 aClip = projection * view * glm::vec4(laserA, 1.0f);
            glm::vec4 bClip = projection * view * glm::vec4(laserB, 1.0f);
            if (aClip.w == 0.0f || bClip.w == 0.0f) {
                // avoid div by zero; skip this frame
            } else {
                glm::vec3 aNDC = glm::vec3(aClip) / aClip.w; // [-1..1]
                glm::vec3 bNDC = glm::vec3(bClip) / bClip.w;

                // Convert to screen pixels
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(window, &fbW, &fbH);
                auto ndcToScreen = [fbW, fbH](const glm::vec3& p)->glm::vec3{
                    float sx = (p.x * 0.5f + 0.5f) * fbW;
                    float sy = (1.0f - (p.y * 0.5f + 0.5f)) * fbH; // invert Y
                    return glm::vec3(sx, sy, p.z);
                };
                auto aS = ndcToScreen(aNDC);
                auto bS = ndcToScreen(bNDC);

                // 2D perpendicular of the segment
                glm::vec2 v = glm::vec2(bS - aS);
                float L = glm::length(v);
                if (L > 1e-3f) {
                    glm::vec2 dir = v / L;
                    glm::vec2 perp(-dir.y, dir.x);
                    glm::vec2 off = perp * (LASER_PIXELS * 0.5f);

                    // 4 corners in screen space (z from the nearer endpoint)
                    float z = std::min(aS.z, bS.z);
                    glm::vec3 P0 = glm::vec3(aS.x + off.x, aS.y + off.y, z);
                    glm::vec3 P1 = glm::vec3(bS.x + off.x, bS.y + off.y, z);
                    glm::vec3 P2 = glm::vec3(bS.x - off.x, bS.y - off.y, z);
                    glm::vec3 P3 = glm::vec3(aS.x - off.x, aS.y - off.y, z);

                    // Back to NDC for the laser shader
                    auto screenToNDC = [fbW, fbH](const glm::vec3& s)->glm::vec3{
                        float x = (s.x / (float)fbW) * 2.0f - 1.0f;
                        float y = 1.0f - (s.y / (float)fbH) * 2.0f;
                        return glm::vec3(x, y, s.z);
                    };
                    glm::vec3 v0 = screenToNDC(P0);
                    glm::vec3 v1 = screenToNDC(P1);
                    glm::vec3 v2 = screenToNDC(P2);
                    glm::vec3 v3 = screenToNDC(P3);

                    glm::vec3 tris[6] = { v0, v1, v2,  v0, v2, v3 };

                    glBindBuffer(GL_ARRAY_BUFFER, laserVBO);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tris), tris);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);

                    // Draw in screen space with additive blending
                    glUseProgram(laserProg);
                    GLint uColorLoc = glGetUniformLocation(laserProg, "uColor");
                    glUniform3f(uColorLoc, LASER_COLOR.x, LASER_COLOR.y, LASER_COLOR.z);

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);

                    glBindVertexArray(laserVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);

                    glDisable(GL_BLEND);
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_CULL_FACE);
                }
            }
        }


        // game over screen display
        if (appMode == gameMode::GAME_OVER) {
            // ensure cursor is visible on this screen
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            // Background galaxy
            glm::mat4 galaxyTransform =
                glm::scale(glm::translate(glm::mat4(1.0f), camera.getPosition()), glm::vec3(50.0f));

            glDisable(GL_DEPTH_TEST);
            glCullFace(GL_FRONT);

            glUseProgram(sceneProgram);
            glUniform1i(uUseLighting, 0);
            glUniform1i(uUseTexture,  1);
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(galaxyTransform));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, galaxyTexture);

            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glCullFace(GL_BACK);

            // Centered UI
            int fbw=0, fbh=0; glfwGetFramebufferSize(window, &fbw, &fbh);
            float boxW = 520.0f, boxH = 80.0f;
            float cx   = fbw * 0.5f;
            float cy   = fbh * 0.5f;
            float padY = 20.0f;

            // Title + score
            UI::Button(window, cx - 260, cy - 180, 520, 80, false);
            UI::Button(window, cx - 260, cy -  90, 520, 60, false);
            UI::Text  (window, cx - 260, cy - 180, 520, 80, "GAME OVER");

            {
                std::string sc = "SCORE: " + std::to_string(totalScore);
                UI::Text(window, cx - 260, cy - 90, 520, 60, sc.c_str());
            }

            // Two buttons: Try Again / Main Menu
            float tryX = cx - boxW - 20.0f;
            float tryY = cy + 20.0f;
            float menX = cx + 20.0f;
            float menY = cy + 20.0f;

            UI::Button(window, tryX, tryY, boxW, boxH, true);
            UI::Button(window, menX, menY, boxW, boxH, true);
            UI::Text  (window, tryX, tryY, boxW, boxH, "TRY AGAIN");
            UI::Text  (window, menX, menY, boxW, boxH, "MAIN MENU");

            // DPI-aware click hit-test (same idea as in MENU)
            int winW=0, winH=0; glfwGetWindowSize(window, &winW, &winH);
            int fbw2=0, fbh2=0; glfwGetFramebufferSize(window, &fbw2, &fbh2);
            double scaleX = (winW>0) ? double(fbw2)/double(winW) : 1.0;
            double scaleY = (winH>0) ? double(fbh2)/double(winH) : 1.0;

            double mx=0.0, my=0.0; glfwGetCursorPos(window, &mx, &my);
            double mxFB = mx * scaleX, myFB = my * scaleY;

            auto inside = [](double x,double y,double rx,double ry,double rw,double rh){
                return x>=rx && x<=rx+rw && y>=ry && y<=ry+rh;
            };

            static bool mouseDownLastGO = false;
            bool mouseDownNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

            bool overTry = inside(mxFB, myFB, tryX, tryY, boxW, boxH);
            bool overMen = inside(mxFB, myFB, menX, menY, boxW, boxH);

            bool clickedTry = (mouseDownNow && !mouseDownLastGO && overTry);
            bool clickedMen = (mouseDownNow && !mouseDownLastGO && overMen);
            mouseDownLastGO = mouseDownNow;

            if (clickedTry) {
                // reset game state and jump back to GAME
                shotsLeft    = 3;
                totalScore   = 0;
                laserActive  = false;
                laserTimer   = 0.0f;
                firePressedLast = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
                appMode = gameMode::GAME;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // lock cursor again
            }
            if (clickedMen) {
                // back to main menu
                shotsLeft    = 3;
                laserActive  = false;
                laserTimer   = 0.0f;
                firePressedLast = false;
                appMode = gameMode::MENU;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        // esc to exit the program
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, true);
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
    }

    // Clean-up
    deleteSceneGraph(root);

    if (laserVBO) glDeleteBuffers(1, &laserVBO);
    if (laserVAO) glDeleteVertexArrays(1, &laserVAO);
    if (crossVBO) glDeleteBuffers(1, &crossVBO);
    if (crossVAO) glDeleteVertexArrays(1, &crossVAO);
    UI::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}