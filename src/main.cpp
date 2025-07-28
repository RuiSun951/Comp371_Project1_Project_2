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

//shooting star as a dynamic lighting source
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
    glBufferData(GL_ARRAY_BUFFER, planetAOrbitVertices.size() * sizeof(glm::vec3), planetBOrbitVertices.data(), GL_STATIC_DRAW);
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
        glm::vec3(0.0f, 1.0f, 5.0f),  // camera position
        glm::vec3(0.0f, 1.0f, 0.0f),  // camera up
        -90.0f,                       // yaw
        0.0f                          // pitch
    );
    static float lastFrame = 0.0f;

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");

    SceneNode* root = new SceneNode();  //identity node
    SceneNode* sun = new SceneNode();   //separate sun from identity
    SceneNode* planetB = new SceneNode();
    SceneNode* shootingStar = new SceneNode();
    SceneNode* planetA_orbit = new SceneNode();
    SceneNode* planetA_body = new SceneNode();
    SceneNode* moon = new SceneNode();

    root->addChild(sun);
    root->addChild(planetA_orbit);
    planetA_orbit ->addChild(planetA_body);
    planetA_orbit ->addChild(moon);
    root->addChild(planetB);
    root->addChild(shootingStar);


    // Now modelLoc is valid here:
    auto drawFunc = [&](const glm::mat4& model) {
        glUseProgram(shaderProgram);    // optional, but ensure shader is active
        glUniformMatrix4fv(modelLoc,1,GL_FALSE,glm::value_ptr(model));
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
    };

    sun->drawFunc = drawFunc;
    planetA_body->drawFunc = drawFunc;
    planetB->drawFunc = drawFunc;
    moon->drawFunc = drawFunc;
    shootingStar->drawFunc = drawFunc;

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
        float scaledTime = glfwGetTime() * timeScale;

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

        // Update camera/view uniforms
        glm::vec3 camPos = camera.getPosition();
        glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), camPos.x, camPos.y, camPos.z);

        // Light 1: static top-right corner -ish
        glm::vec3 lightPos1 = glm::vec3(10.0f, 10.0f, 10.0f);
        glm::vec3 lightColor1 = glm::vec3(1.0f);

        // Light 2: dynamic shooting star across the sky, single direction shooting, loop periodically
        float angle = currentFrame * 0.5f; 
        glm::vec3 lightPos2 = glm::vec3(0.0f , 8.0f * cos(angle), 8.0f * sin(angle));
        glm::vec3 lightColor2 = glm::vec3(1.0f, 1.0f, 1.0f);  // shooting star bright full white for better seeing in testing

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
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos1"), 1, glm::value_ptr(lightPos1));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor1"), 1, glm::value_ptr(lightColor1));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos2"), 1, glm::value_ptr(lightPos2));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor2"), 1, glm::value_ptr(lightColor2));

        // Update transformation matrices
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Yibo Tang: Update transforms (hierarchical scene graph)
        // Sun self-rotation, with biggest scale
        glm::mat4 sunM = glm::rotate(glm::mat4(1.0f), scaledTime, glm::vec3(0,1,0))
                    * glm::scale (glm::mat4(1.0f), glm::vec3(1.5f));
        sun->localTransform = sunM;

        // planetA orbiting the sun + self-rotation
        float orbitA_speed = scaledTime * 0.1f;
        glm::mat4 orbitA = glm::rotate(glm::mat4(1.0f), orbitA_speed, glm::vec3(0,1,0));
        planetA_orbit->localTransform = orbitA * glm::translate(glm::mat4(1.0f), glm::vec3(PLANET_A_ORBIT_RADIUS,0,0));    

        // planetB inner orbit and rotation
        float orbitB_speed = scaledTime * 0.3f;
        glm::mat4 orbitB = glm::rotate(glm::mat4(1.0f), orbitB_speed, glm::vec3(0,1,0));
        planetB->localTransform =
            orbitB
            * glm::translate(glm::mat4(1.0f), glm::vec3(PLANET_B_ORBIT_RADIUS,0,0))
            * glm::scale(glm::mat4(1.0f), glm::vec3(0.4f));

        //Moon orbiting the planetA
        float orbitM_speed = scaledTime * 0.4f;
        glm::mat4 orbitM = glm::rotate(glm::mat4(1.0f), orbitM_speed, glm::vec3(0,1,0))
                        * glm::translate(glm::mat4(1.0f), glm::vec3(MOON_ORBIT_RADIUS,0,0));
        moon->localTransform = orbitM * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));

        // Draw planetA orbit (around origin)
        glUniformMatrix4fv(modelLoc,1,GL_FALSE,glm::value_ptr(orbitA));
        glUniform3f(glGetUniformLocation(shaderProgram,"objectColor"), 0.0f,0.4f,1.0f);
        glBindVertexArray(planetAOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGMENTS+1);

        // Draw planetB orbit
        glUniformMatrix4fv(modelLoc,1,GL_FALSE,glm::value_ptr(orbitB));
        glUniform3f(glGetUniformLocation(shaderProgram,"objectColor"), 1.0f,0.2f,0.2f);
        glBindVertexArray(planetBOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGMENTS+1);

        // compute moon ring transformation
        glm::mat4 planetATrans = orbitA * glm::translate(glm::mat4(1.0f),glm::vec3(PLANET_A_ORBIT_RADIUS,0,0));
        glm::mat4 moonRingM = planetATrans;

        // Draw the orange moon orbit circle:
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(moonRingM));
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 1.0f, 0.5f, 0.0f);
        glBindVertexArray(moonOrbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGMENTS+1);

        // Draw the trail of the shooting star
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 1.0f, 1.0f, 1.0f); // blue trail
        glLineWidth(2.0f);  // trail thicker
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(trailVAO);
        glDrawArrays(GL_LINE_STRIP, 0, trailPositions.size());

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

    // Clean
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

