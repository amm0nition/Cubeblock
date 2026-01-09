#define STB_IMAGE_IMPLEMENTATION
#define STB_PERLIN_IMPLEMENTATION

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "stb_image.h"
#include "stb_perlin.h"
#include "shader_s.hpp"
#include "block_manager.hpp"
#include "world.hpp"

using json = nlohmann::json;

// ========================
// === Global Variables ===
// ========================

unsigned int windowWidth = 1600;
unsigned int windowHeight = 900;
const char* windowTitle = "Cubeblock";

// Camera Settings
float fov = 75.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

const float cameraSpeed = 0.05f;
glm::vec3 cameraPos = glm::vec3(0.0f, 10.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = windowWidth / 2.0;
float lastY = windowHeight / 2.0;

float lastModifyTime = 0.0f;
float lastAutoSaveTime = 0.0f;

unsigned int highlightVAO = 0, highlightVBO = 0;
unsigned int chVAO, chVBO;

// Global Managers
BlockManager globalBlockManager;
World world;

// =======================
// === Shader Sources ===
// =======================

// Simple Shader for target block highlight
const char* highlightVert = R"(
    #version 460 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

const char* highlightFrag = R"(
    #version 460 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 1.0, 1.0, 0.4); // White with 40% opacity
    }
)";

// UI Vertex Shader
const char* crosshairVert = R"(
    #version 460 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;
    uniform float scale;
    uniform float aspectRatio;

    void main() {
        // Position: x scaled by aspect ratio to maintain square shape
        gl_Position = vec4(aPos.x * scale / aspectRatio, aPos.y * scale, 0.0, 1.0);
        TexCoord = aTexCoord;
    }
)";

// UI Fragment Shader
const char* crosshairFrag = R"(
    #version 460 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D uiTexture;

    void main() {
        FragColor = texture(uiTexture, TexCoord);
    }
)";

// ========================
// === Helper Functions ===
// ========================

// Load crosshair texture
unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = (nrComponents == 4) ? GL_RGBA : GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

struct RaycastResult {
    bool hit;
    glm::ivec3 blockPos;
    glm::vec3 worldPos;
    glm::ivec3 normal; // Stores which face we hit
};

RaycastResult raycast(glm::vec3 start, glm::vec3 direction, float range) {
    glm::vec3 dir = glm::normalize(direction);
    glm::ivec3 mapPos = glm::floor(start);

    glm::vec3 deltaDist = glm::abs(1.0f / dir);
    glm::ivec3 step;
    glm::vec3 sideDist;

    // Track the last axis we moved on to calculate normal
    int lastAxis = -1; // 0=x, 1=y, 2=z

    if (dir.x < 0) { step.x = -1; sideDist.x = (start.x - mapPos.x) * deltaDist.x; }
    else { step.x = 1;  sideDist.x = (mapPos.x + 1.0f - start.x) * deltaDist.x; }

    if (dir.y < 0) { step.y = -1; sideDist.y = (start.y - mapPos.y) * deltaDist.y; }
    else { step.y = 1;  sideDist.y = (mapPos.y + 1.0f - start.y) * deltaDist.y; }

    if (dir.z < 0) { step.z = -1; sideDist.z = (start.z - mapPos.z) * deltaDist.z; }
    else { step.z = 1;  sideDist.z = (mapPos.z + 1.0f - start.z) * deltaDist.z; }

    float dist = 0.0f;
    while (dist < range) {
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) {
                mapPos.x += step.x;
                dist = sideDist.x;
                sideDist.x += deltaDist.x;
                lastAxis = 0;
            }
            else {
                mapPos.z += step.z;
                dist = sideDist.z;
                sideDist.z += deltaDist.z;
                lastAxis = 2;
            }
        }
        else {
            if (sideDist.y < sideDist.z) {
                mapPos.y += step.y;
                dist = sideDist.y;
                sideDist.y += deltaDist.y;
                lastAxis = 1;
            }
            else {
                mapPos.z += step.z;
                dist = sideDist.z;
                sideDist.z += deltaDist.z;
                lastAxis = 2;
            }
        }

        BlockID b = world.getBlock(mapPos.x, mapPos.y, mapPos.z);
        if (b != BLOCK_AIR) {
            // Calculate Normal based on the last step taken
            glm::ivec3 normal(0);
            if (lastAxis == 0) normal.x = -step.x;
            if (lastAxis == 1) normal.y = -step.y;
            if (lastAxis == 2) normal.z = -step.z;

            return { true, mapPos, start + dir * dist, normal };
        }
    }
    return { false, glm::ivec3(0), glm::vec3(0), glm::ivec3(0) };
}

// ========================
// === Input Callbacks ===
// ========================

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    windowWidth = width;
    windowHeight = height;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 90.0f) fov = 90.0f;
}

void processInput(GLFWwindow* window) {
    float currentSpeed = 5.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) currentSpeed *= 3.0f;

    // Camera Controls
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += currentSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= currentSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += cameraUp * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) cameraPos -= cameraUp * currentSpeed;

    // Toggle Infinite Mode (Press 'I')
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) world.isInfinite = !world.isInfinite;

    // === MOUSE INTERACTION (With Delay) ===
    float currentTime = (float)glfwGetTime();

    // Check if 0.2 seconds have passed since last action
    if (currentTime - lastModifyTime > 0.2f) {

        bool leftClick = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightClick = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        if (leftClick || rightClick) {
            RaycastResult ray = raycast(cameraPos, cameraFront, 8.0f);

            if (ray.hit) {
                if (rightClick) {
                    // Place Block
                    glm::ivec3 p = ray.blockPos + ray.normal;
                    world.setBlock(p.x, p.y, p.z, BLOCK_STONE);
                }
                else if (leftClick) {
                    // Remove Block
                    world.setBlock(ray.blockPos.x, ray.blockPos.y, ray.blockPos.z, BLOCK_AIR);
                }
                // Reset the timer
                lastModifyTime = currentTime;
            }
        }
    }
}

// =====================
// === Main Function ===
// =====================
int main()
{
    // Initialise GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // ============================
    // === Shader Compilation ===
    // ============================

    // Build Highlight Shader
    unsigned int hVert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(hVert, 1, &highlightVert, NULL);
    glCompileShader(hVert);
    unsigned int hFrag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(hFrag, 1, &highlightFrag, NULL);
    glCompileShader(hFrag);
    unsigned int highlightProg = glCreateProgram();
    glAttachShader(highlightProg, hVert);
    glAttachShader(highlightProg, hFrag);
    glLinkProgram(highlightProg);

    // Build crosshair shader
    unsigned int chVert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(chVert, 1, &crosshairVert, NULL);
    glCompileShader(chVert);
    unsigned int chFrag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(chFrag, 1, &crosshairFrag, NULL);
    glCompileShader(chFrag);
    unsigned int crosshairProg = glCreateProgram();
    glAttachShader(crosshairProg, chVert);
    glAttachShader(crosshairProg, chFrag);
    glLinkProgram(crosshairProg);

    Shader ourShader("shaders/vertex.glsl", "shaders/fragment.glsl");

    // ============================
    // === Mesh Generation      ===
    // ============================

    // Build Highlight Cube Mesh
    float cubeVertices[] = {
        // Back face
        -0.5f, -0.5f, -0.5f,  0.5f, 0.5f, -0.5f,  0.5f, -0.5f, -0.5f,         0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
        // Front face
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, 0.5f,  0.5f,         0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
        // Left face
        -0.5f,  0.5f,  0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        // Right face
        0.5f,  0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, 0.5f, -0.5f,         0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,        0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
        // Top face
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,        0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f
    };
    glGenVertexArrays(1, &highlightVAO);
    glGenBuffers(1, &highlightVBO);
    glBindVertexArray(highlightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, highlightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Create Quad Mesh (Centered at 0,0)
    float chVertices[] = {
        // Pos        // Tex
        -0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,

        -0.5f,  0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &chVAO);
    glGenBuffers(1, &chVBO);
    glBindVertexArray(chVAO);
    glBindBuffer(GL_ARRAY_BUFFER, chVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(chVertices), chVertices, GL_STATIC_DRAW);

    // Attrib 0: Position (2 floats)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Attrib 1: TexCoord (2 floats)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ============================
    // === Initialization       ===
    // ============================

    // Load textures from json file
    globalBlockManager.loadBlocks("blocks.json");

    // Load Texture
    unsigned int crosshairTexture = loadTexture("textures/crosshair.png"); // Make sure this file exists!

    // Configure Shader
    ourShader.use();
    ourShader.setInt("textureArray", 0);
    ourShader.setVec3("light.direction", -0.2f, -1.0f, -0.3f);
    ourShader.setVec3("light.ambient", 0.2f, 0.2f, 0.2f);
    ourShader.setVec3("light.diffuse", 0.8f, 0.8f, 0.8f);

    // World Settings
    world.isInfinite = true;

    // ============================
    // === Render Loop          ===
    // ============================
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.5f, 0.81f, 0.92f, 1.0f); // Sky Blue Color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)windowWidth / (float)windowHeight, 0.1f, 100.0f);

        // --- UPDATE WORLD ---
        world.update(cameraPos);

        // --- RENDER WORLD ---
        ourShader.use();
        ourShader.setMat4("view", view);
        ourShader.setMat4("projection", projection);

        ourShader.setVec3("viewPos", cameraPos);

        // === FOG SETTINGS ===
        ourShader.setVec3("fogColor", 0.53f, 0.81f, 0.92f);

        // Calculate dynamic density based on Render Distance
        // Formula: density = 2.0 / MaxDistance. 
        // This ensures visibility drops to ~1.8% at the edge of the world.
        float maxDist = (float)(world.renderDistance * CHUNK_SIZE);
        float density = 2.4f / maxDist; // 2.4 makes it slightly thicker to hide corners

        ourShader.setFloat("fogDensity", density);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, globalBlockManager.textureArrayID);

        world.render(ourShader);

        // Auto-save
        if (currentFrame - lastAutoSaveTime > 60.0f) {
            std::cout << "Auto-saving..." << std::endl;
            world.saveAllChunks();
            lastAutoSaveTime = currentFrame;
        }

        // Raycast
        RaycastResult ray = raycast(cameraPos, cameraFront, 8.0f);

        if (ray.hit) {
            // Enable Blending for transparency
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(highlightProg);

            // Set View/Proj
            glUniformMatrix4fv(glGetUniformLocation(highlightProg, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(highlightProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

            // Position the cube at the hit block
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(
                ray.blockPos.x + 0.5f,
                ray.blockPos.y + 0.5f,
                ray.blockPos.z + 0.5f
            ));
            model = glm::scale(model, glm::vec3(1.01f));

            glUniformMatrix4fv(glGetUniformLocation(highlightProg, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glBindVertexArray(highlightVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            glDisable(GL_BLEND);
        }
        // Draw crosshair
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(crosshairProg);
        glUniform1f(glGetUniformLocation(crosshairProg, "scale"), 0.05f);
        glUniform1f(glGetUniformLocation(crosshairProg, "aspectRatio"), (float)windowWidth / (float)windowHeight);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, crosshairTexture);
        glBindVertexArray(chVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        world.saveAllChunks();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
