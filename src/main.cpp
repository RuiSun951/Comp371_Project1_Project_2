#include "../OBJloader.h" // from Tut05
#include <vector>
#include <GL/glew.h>
#include "Vertex.h" // from src/Vertex.h
#include <iostream>
#include <GLFW/glfw3.h>


std::vector<Vertex> sphereVertices;
std::vector<unsigned int> sphereIndices;

GLuint sphereVAO, sphereVBO, sphereEBO;

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
        sphereIndices.push_back(i); // 0,1,2,3...
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

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Load the sphere model from OBJ file
    if (!loadSphereModel("models/sphere.obj")) {
        return -1; 
    }

    // Set background color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray

    // main render loop...
    while (!glfwWindowShouldClose(window)) {
        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //set up camera



        // Draw the sphere
        glBindVertexArray(sphereVAO);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size());
        glBindVertexArray(0);

         // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

