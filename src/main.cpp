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
#include "camera.h" // from src/camera.h
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "SceneNode.h" // from src/SceneNode.h
#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#include <cmath>


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

// time control toggle
bool capsPressedLastFrame = false;
bool timeControlOn       = false;
float timeSpeed          = 1.0f;
const float normalSpeed  = 1.0f;
const float fastSpeed    = 3.0f;

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

int main() {
    // Initialize OpenGL context, GLEW, etc. here...
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Create a window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Mini Solar System", nullptr, nullptr);
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
    
    //Yibo Tang Set texture sampler uniform to texture unit 0
    glUniform1i(glGetUniformLocation(sceneProgram, "texture1"), 0);

    glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 0.4f, 0.6f, 1.0f);

    // Load the Sun texture
    GLuint sunTexture = loadTexture("texture/sun.jpg");
    GLuint earthTexture = loadTexture("texture/earth.jpg");
    GLuint marsTexture = loadTexture("texture/mars.jpg");
    GLuint moonTexture = loadTexture("texture/moon.jpg");
    GLuint galaxyTexture = loadTexture("texture/galaxy.jpg");
    GLuint stationTexture = loadTexture("texture/Metal034.png"); 



    // Load the sphere model from OBJ file
    if (!loadSphereModel("models/sphere.obj")) {
        return -1; 
    }
    // Loadd the spacestation model from OBJ file
    if (!loadModelToBuffers("models/spacestation.obj",
                        stationVAO, stationVBO, stationEBO,
                        stationVertices, stationIndices)) {
    std::cerr << "Failed to load spacestation.obj\n";
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

    // Set background color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    //disable cursor


    // camera setup
    Camera camera(
        glm::vec3(0.0f, 1.0f, 10.0f),  // camera position
        glm::vec3(0.0f, 1.0f, 0.0f),  // camera up
        -90.0f,                       // yaw
        0.0f                          // pitch
    );
   // static float lastFrame = 0.0f;

   GLuint modelLoc = glGetUniformLocation(sceneProgram, "model");

    SceneNode* root = new SceneNode();  //identity node
    SceneNode* galaxy = new SceneNode();
    SceneNode* sun = new SceneNode();   //separate sun from identity
    SceneNode* planetB = new SceneNode();
    SceneNode* shootingStar = new SceneNode();
    SceneNode* planetA_orbit = new SceneNode();
    SceneNode* planetA_body = new SceneNode();
    SceneNode* moon = new SceneNode();

    SceneNode* station = new SceneNode();
    planetA_body->addChild(station);  // attach to Earth

 // initial size + offset from Earth
 station->localTransform =
    glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f)) *
    glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));

 // draw
//  station->drawFunc = [&](const glm::mat4& model){
//     glUniform1i(glGetUniformLocation(shadowProgram, "useLighting"), true);
//     glUniform1i(glGetUniformLocation(shadowProgram, "useTexture"), false);
//     glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
//     glBindVertexArray(stationVAO);
//     glDrawElements(GL_TRIANGLES, (GLsizei)stationIndices.size(), GL_UNSIGNED_INT, 0);
//     glBindVertexArray(0);
// };

station->drawFunc = [&](const glm::mat4& model){
    glUniform1i(glGetUniformLocation(shadowProgram, "useLighting"), true);
    glUniform1i(glGetUniformLocation(shadowProgram, "useTexture"),  true); // enable texturing
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glActiveTexture(GL_TEXTURE0); // sampler 'texture1' is already set to 0 earlier
    glBindTexture(GL_TEXTURE_2D, stationTexture);

    glBindVertexArray(stationVAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)stationIndices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
};



    root->addChild(sun);
    root->addChild(galaxy);
    root->addChild(planetA_orbit);
    planetA_orbit ->addChild(planetA_body);
    planetA_orbit ->addChild(moon);
    root->addChild(planetB);
    root->addChild(shootingStar);


    // Now modelLoc is valid here:
    //root->drawFunc = [&](const glm::mat4& model) {
    //glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    // drawSphere();

    sun->drawFunc = [&](const glm::mat4& model) {
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), true);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), true);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTexture);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    planetA_body->drawFunc = [&](const glm::mat4& model) {
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), true);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), true);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, earthTexture); // Earth texture here

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    planetB->drawFunc = [&](const glm::mat4& model) {
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), true);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), true);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, marsTexture); 

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    shootingStar->drawFunc = [&](const glm::mat4& model) {
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), true);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), false);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0); // No texture for shooting star
        glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 1.0f, 1.0f, 1.0f);  // Set pure white color in shader

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };

    moon->drawFunc = [&](const glm::mat4& model) {
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), true);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), true);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, moonTexture); // Moon texture here

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    };


    // Time control factor
    float timeScale = 0.2f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float simTime =0.0f;

    // main render loop
    while (!glfwWindowShouldClose(window)) {

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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


        // Create transformation matrices
        glm::mat4 model = glm::mat4(1.0f); // identity
        glm::mat4 view = camera.getViewMatrix();  // use your camera's method
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            800.0f / 600.0f,   // aspect ratio
            0.1f,
            100.0f
        );

        // Send to shader
        GLuint viewLoc = glGetUniformLocation(sceneProgram, "view");
        GLuint projLoc = glGetUniformLocation(sceneProgram, "projection");

        // Update camera/view uniforms
        glm::vec3 camPos = camera.getPosition();
        glUniform3f(glGetUniformLocation(sceneProgram, "viewPos"), camPos.x, camPos.y, camPos.z);

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
        glm::mat4 lightProj = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 60.0f);
        glm::mat4 lightView = glm::lookAt(lightPos1, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4 lightSpaceMatrix = lightProj * lightView;

        // light 2 shadow setup 
        float nearPL = 0.1f, farPL = 100.0f; // tweak if needed
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
        glUniform3fv(glGetUniformLocation(sceneProgram, "lightPos1"), 1, glm::value_ptr(lightPos1));
        glUniform3fv(glGetUniformLocation(sceneProgram, "lightColor1"), 1, glm::value_ptr(lightColor1));
        glUniform3fv(glGetUniformLocation(sceneProgram, "lightPos2"), 1, glm::value_ptr(lightPos2));
        glUniform3fv(glGetUniformLocation(sceneProgram, "lightColor2"), 1, glm::value_ptr(lightColor2));

        // Update transformation matrices
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

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

        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // SHADOW DEPTH PASS: LIGHT 2 (shooting star)
        glViewport(0, 0, SHADOW_W, SHADOW_H);
        glBindFramebuffer(GL_FRAMEBUFFER, depthCubeFBO);
        glUseProgram(pointShadowProgram);

        GLint uVP       = glGetUniformLocation(pointShadowProgram, "vp");
        GLint uModel    = glGetUniformLocation(pointShadowProgram, "model");
        GLint uLightPos = glGetUniformLocation(pointShadowProgram, "lightPos");
        GLint uFarPlane = glGetUniformLocation(pointShadowProgram, "farPlane");

        glUniform3fv(uLightPos, 1, glm::value_ptr(lp));
        glUniform1f(uFarPlane, farPL);
        glCullFace(GL_FRONT); // reduce acne

        for (int face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depthCubeTex, 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            glm::mat4 vp = shadowProj2 * views2[face];
            glUniformMatrix4fv(uVP, 1, GL_FALSE, glm::value_ptr(vp));

            // Sun
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(sun->getGlobalTransform()));
            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Earth
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(earthGlobal));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Mars
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(planetB->getGlobalTransform()));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

            // Moon
            glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(moonGlobal));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);
        }

        glBindVertexArray(0);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Reset viewport to window size
        int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glUseProgram(sceneProgram);

        // light 1 (directional/spot) shadow matrix
        glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        // bind shadow map for light 1
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glUniform1i(glGetUniformLocation(sceneProgram, "shadowMap"), 1);

        // bind cube shadow map for light 2
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeTex);
        glUniform1i(glGetUniformLocation(sceneProgram, "shadowCube2"), 2);

        // pass far plane for light 2 sampling
        glUniform1f(glGetUniformLocation(sceneProgram, "farPlane2"), farPL);

        // Draw the trail of the shooting star
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), false);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"),  false);
        glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(trailVAO);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)trailPositions.size());

        // Draw galaxy background
        glm::mat4 galaxyTransform = glm::scale(glm::translate(glm::mat4(1.0f), camera.getPosition()), glm::vec3(50.0f));
        glDisable(GL_DEPTH_TEST);
        glCullFace(GL_FRONT);

        glUseProgram(sceneProgram);
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), false);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), true);
        glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "model"), 1, GL_FALSE, glm::value_ptr(galaxyTransform));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, galaxyTexture);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Restore state
        glEnable(GL_DEPTH_TEST);
        glCullFace(GL_BACK);
        
        // Draw orbit lines
        glUseProgram(sceneProgram);
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), false);
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), false);
        glDisable(GL_CULL_FACE);
        glLineWidth(2.0f); 

        // Planet A orbit (blue)
        glm::mat4 planetAOrbitModel = glm::rotate(glm::mat4(1.0f),earthOrbitAngle,glm::vec3(0,1,0));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planetAOrbitModel));
        glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 0.0f, 0.0f, 1.0f);
        glBindVertexArray(planetAOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, planetAOrbitVertices.size());

        // Planet B orbit (red)
        glm::mat4 planetBOrbitModel = glm::rotate(glm::mat4(1.0f), marsOrbitAngle, glm::vec3(0,1,0));        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planetBOrbitModel));
        glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 1.0f, 0.0f, 0.0f);
        glBindVertexArray(planetBOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, planetBOrbitVertices.size());

        // Moon orbit (orange)
        glm::mat4 moonOrbitLineM = planetA_orbit->localTransform;
        glBindVertexArray(moonOrbitVAO);
        glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 1.0f, 0.5f, 0.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(moonOrbitLineM));
        glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(moonOrbitVertices.size()));

        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);  // Re-enable culling
        glUniform1i(glGetUniformLocation(sceneProgram, "useLighting"), true);  //turn lighting back on
        glUniform1i(glGetUniformLocation(sceneProgram, "useTexture"), true);   //turn texture back on
      
        //reset object color to white for next draw calls
        glUniform3f(glGetUniformLocation(sceneProgram, "objectColor"), 1.0f, 1.0f, 1.0f);

        // Draw the scene recursively, instead of draw sphere one by one, use root
        root->draw(glm::mat4(1.0f));

        // esc to exit the program
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, true);
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean-up
    deleteSceneGraph(root);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}