#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <math.h>
#include "shader/shader.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow * window);

int main() 
{
    // Initialzie GLFW
    // ------------------------------------
    glfwInit();
    // Configure GLFW: OpenGL version 3.3 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // Explicitly use the core-profile. 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    // Create a window object (width, height, name)
    // ------------------------------------
    GLFWwindow * window = glfwCreateWindow(800, 600, "OpenGL", NULL, NULL);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Window resize by registering
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // GLAD manages function pointers for OpenGL 
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Build and compile our shader program
    // ------------------------------------
    Shader ourShader("03_Shader/3.3.shader.vs", "03_Shader/3.3.shader.fs");

    
    float vertices[] = {
         // positions         // colors
         0.5f , -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // Bottom right
        -0.5f , -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // Bottom left
         0.0f ,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f    // Top
    };

    // Unique ID corresponding to that buffer
    unsigned int VBO;
    glGenBuffers(1, &VBO);

    // Vertex Array Object
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);

    // Bind Vertex array object
    glBindVertexArray(VAO);

    // Copy vertex data into buffer's memory
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);


    // vertex position attributes 
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // vertex color attributes 
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    // The application to keep drawing images and handling user input until 
    // the program has been explicitly told to stop
    // Checks at the start of each loop iteration if GLFW has been instructed to close. 
    // If so, the function returns true and the render loop stops running, after which we can close the application.
    while (!glfwWindowShouldClose(window)) {

        // Input
        processInput(window);

        // Render
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);


        // Update the uniform color
        // float timeValue = glfwGetTime();
        // float greenValue = sin(timeValue) / 2.0f + 0.5f;
        // int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
        // // R=0, G=greenValue, B=0, A=1.0
        // glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f);

        // AUTO BIND EBO
        ourShader.use();
        glBindVertexArray(VAO); 
        glDrawArrays(GL_TRIANGLES, 0, 3);


        // Used to render to during this render iteration and show it as output to the screen. 
        glfwSwapBuffers(window);

        // Checks if any events are triggered (like keyboard input or mouse movement events), 
        // updates the window state, and calls the corresponding functions
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glfwTerminate();

    return 0;
}

// Process all input
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// glfw: whenever the window size changed (by OS or user resize)
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Make sure the viewport matches the new window dimensions
    // Viewport: The size of the rendering window so OpenGL knows 
    // OpenGL uses the data specified via glViewport to transform the 2D coordinates 
    // it processed to coordinates on your screen.
    glViewport(0 , 0, width, height);
}