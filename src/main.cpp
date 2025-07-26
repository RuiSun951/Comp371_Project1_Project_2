#include "../OBJloader.h" // from Tut05
#include <vector>
#include <GL/glew.h>
#include "Vertex.h" // from src/Vertex.h
#include <iostream>
#include <GLFW/glfw3.h>
#include "camera.h" // from src/camera.h


std::vector<Vertex> sphereVertices;
std::vector<unsigned int> sphereIndices;
GLuint sphereVAO, sphereVBO, sphereEBO;

// Load the sphere model from an OBJ file
bool loadSphereModel(const char *path) {
    std::vector <glm::vec3> positions;
    std::vector <glm::vec3> normals;
    std::vector <glm::vec2> uvs;

    if (!loadOBJ(path, positions, normals, uvs)) {
        std::cerr << "Failed to load " << path << std::endl;
        return false;
    }

    // Convert to Vertex struct
    for (size_t i = 0; i < positions.size(); ++i) {
        Vertex v;
        v.position = positions[i];
        v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0.0f);
        v.texCoord = (i < uvs.size()) ? uvs[i] : glm::vec2(0.0f);
        sphereVertices.push_back(v);
        sphereIndices.push_back(i); 
    }

    // Upload to GPU
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(Vertex), sphereVertices.data(), GL_STATIC_DRAW);

    // Layout
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
Camera camera(glm::vec3(0.0f, 1.0f, 5.0f), glm::vec3(0, 1, 0), -90.0f, 0.0f);
bool tabPressedLastFrame = false;

float lastX = 400, lastY = 300;
bool firstMouse = true;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed

    lastX = xpos;
    lastY = ypos;

    camera.processMouseMovement(xoffset, yoffset);
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
    glFrontFace(GL_CCW); // Counter-clockwise is default winding (most OBJ files use this)

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Load the sphere model from OBJ file
    if (!loadSphereModel("models/sphere.obj")) {
        return -1; 
    }

    // Set background color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray

    // mouse control
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    //disable cursor



    // main render loop...
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // camera view control
        bool tabPressedNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;

        if (tabPressedNow && !tabPressedLastFrame) {
            camera.toggleMode();
        }
        tabPressedLastFrame = tabPressedNow;

        camera.processKeyboard(window, deltaTime);




        // Draw the sphere
        glBindVertexArray(sphereVAO);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size());
        glBindVertexArray(0);

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

