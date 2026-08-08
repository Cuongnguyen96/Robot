#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow * window);

// Vertex shader
const char* vertexShaderSource = "#version 330 core \n"
    "layout (location = 0) in vec3 aPos; \n"
    "void main() \n"
    "{ \n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); \n"
    "} \0";

// Fragment shader
const char* fragmentShaderSource = "#version 330 core \n"
    "out vec4 FragColor; \n"
    "void main() \n"
    "{ \n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f); \n"
    "} \0";

int main() 
{
    // Initialzie GLFW
    glfwInit();
    // Configure GLFW: OpenGL version 3.3 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // Explicitly use the core-profile. 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    // Create a window object (width, height, name)
    GLFWwindow * window = glfwCreateWindow(800, 600, "OpenGL", NULL, NULL);
    if (window == nullptr) {
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

    // Create Shader
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // Check Shader compiler 
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPIPLATION_FAILED\n" << infoLog << std::endl;
    }

    // Fragment shader
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // Check Shader compiler 
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPIPLATION_FAILED\n" << infoLog << std::endl;
    }

    // Link 
    unsigned int shaderProgram;
    // Returns the ID reference to the newly created program object. 
    shaderProgram = glCreateProgram();
    // Attach the previously compiled shaders to the programe object and then link them
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Check link compiler 
    glGetShaderiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Detele
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    
    // Vertex input
    float vertices[] = {
        // Frist triangle
         0.5f,  0.5f, 0.0f, // Top right
         0.5f, -0.5f, 0.0f, // Bottiom right
        -0.5f, -0.5f, 0.0f, // Bottom left
        -0.5f,  0.5f, 0.0f  // Top left
    };

    // Indices
    unsigned int indices[] {
        0, 1, 3,    // Frist triangle
        1, 2, 3     // Second triangle
    };


    // Unique ID corresponding to that buffer
    unsigned int VBO;
    glGenBuffers(1, &VBO);

    // Vertex Array Object
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);

    // Bind Vertex array object
    glBindVertexArray(VAO);

    // Element Buffer Object (EBO)
    unsigned int EBO;
    glGenBuffers(1, &EBO);

    // Copy vertex data into buffer's memory
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Copy our index array in element buffer 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Set the vertex attributes pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


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

        // Draw
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO); // AUTO BIND EBO
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


        // Used to render to during this render iteration and show it as output to the screen. 
        glfwSwapBuffers(window);

        // Checks if any events are triggered (like keyboard input or mouse movement events), 
        // updates the window state, and calls the corresponding functions
        glfwPollEvents();
    }

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