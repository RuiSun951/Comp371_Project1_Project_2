#include "../OBJloader.h" // from Tut05
#include <vector>
#include <GL/glew.h>
#include "Vertex.h" // from src/Vertex.h
#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "camera.h" // from src/camera.h
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "SceneNode.h" // from src/SceneNode.h



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

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, log);
        std::cerr << "Shader program linking error:\n" << log << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Vertex structure for the sphere model
std::vector<Vertex> sphereVertices;
std::vector<unsigned int> sphereIndices;
GLuint sphereVAO, sphereVBO, sphereEBO;

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

//set up camera
bool tabPressedLastFrame = false;
const float cameraSpeed = 2.5f;
const float cameraFastSpeed = 6.0f;

//Yibo Tang
void drawSphere() {
    glBindVertexArray(sphereVAO);
    glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size());
    glBindVertexArray(0);
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

    // Create shader program
    GLuint shaderProgram = createShaderProgram("shaders/vertexShader.glsl", "shaders/fragmentShader.glsl");
    glUseProgram(shaderProgram);

    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 5.0f, 5.0f, 5.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.4f, 0.6f, 1.0f);



    // Load the sphere model from OBJ file
    if (!loadSphereModel("models/sphere.obj")) {
        return -1; 
    }

    // Set background color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    //disable cursor


    // camera setup
    Camera camera(
        glm::vec3(0.0f, 1.0f, 5.0f),  // camera position
        glm::vec3(0.0f, 1.0f, 0.0f),  // camera up
        -90.0f,                       // yaw
        0.0f                          // pitch
    );
    static float lastFrame = 0.0f;

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");

  SceneNode* root = new SceneNode();
  SceneNode* planet = new SceneNode();
  SceneNode* moon = new SceneNode();

  root->addChild(planet);
  planet->addChild(moon);

  // Now modelLoc is valid here:
  root->drawFunc = [&](const glm::mat4& model) {
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    drawSphere();
};
  planet->drawFunc = root->drawFunc;
  moon->drawFunc = root->drawFunc;

  // Time control factor
  float timeScale = 0.2f;

  float deltaTime = 0.0f;
  //float lastFrame = 0.0f;

    // main render loop
    while (!glfwWindowShouldClose(window)) {

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // mouse control
        float deltaTime = 0.0f;
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // Yibo Tang
        float scaledTime = currentFrame * timeScale;

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
        GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
        GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");

        //Yibo Tang
       // glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        //glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Update camera/view uniforms
        glm::vec3 camPos = camera.getPosition();
        glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), camPos.x, camPos.y, camPos.z);

        // Update transformation matrices
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Draw the sphere
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Yibo Tang: Update transforms (hierarchical scene graph)
        // Sun self-rotation
        root->localTransform = glm::rotate(glm::mat4(1.0f), scaledTime, glm::vec3(0, 1, 0));

        // planet orbiting the sun + self-rotation
        glm::mat4 planetOrbit = glm::rotate(glm::mat4(1.0f), scaledTime *0.1f, glm::vec3(0, 1, 0));
        planetOrbit = glm::translate(planetOrbit, glm::vec3(5.0f, 0.0f, 0.0f));
        planet->localTransform = glm::rotate(planetOrbit, scaledTime * 0.8f, glm::vec3(0, 1, 0));

        //Moon orbiting the planet
        glm::mat4 moonOrbit = glm::rotate(glm::mat4(1.0f), scaledTime * 0.4f, glm::vec3(0, 1, 0));
        moonOrbit = glm::translate(moonOrbit, glm::vec3(2.0f, 0.0f, 0.0f));
        moon->localTransform = glm::scale(moonOrbit, glm::vec3(0.5f));

        // Draw the whole scene recursively
        root->draw(glm::mat4(1.0f)); // Identity matrix as root


        // esc to exit the program
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, true);
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

