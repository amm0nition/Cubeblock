#include <fstream>
#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp> 
#include "block_manager.hpp"
#include "stb_image.h"

using json = nlohmann::json;

void BlockManager::loadBlocks(const char* configPath) {
    std::ifstream f(configPath);
    if (!f.is_open()) {
        std::cerr << "Failed to open block config: " << configPath << std::endl;
        return;
    }

    json data = json::parse(f);
    std::vector<std::string> texturePaths;
    std::map<std::string, int> pathToIndex;

    // Process JSON
    for (auto& block : data) {
        int id = block["id"];
        BlockFaceTextures faces;

        // Lambda to avoid duplicates
        auto registerTex = [&](std::string path) -> int {
            if (pathToIndex.find(path) == pathToIndex.end()) {
                pathToIndex[path] = texturePaths.size();
                texturePaths.push_back(path);
            }
            return pathToIndex[path];
            };

        if (block.contains("texture") && block["texture"].is_string()) {
            int idx = registerTex(block["texture"]);
            faces = { idx, idx, idx };
        }
        else if (block.contains("textures")) {
            faces.topLayer = registerTex(block["textures"]["top"]);
            faces.bottomLayer = registerTex(block["textures"]["bottom"]);
            faces.sideLayer = registerTex(block["textures"]["side"]);
        }
        blockData[id] = faces;
    }

    // Generate OpenGL Texture Array
    if (texturePaths.empty()) return;

    glGenTextures(1, &textureArrayID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArrayID);

    // Load the first image to determine width/height for the whole array
    int width, height, nrChannels;
    std::string firstPath = "textures/" + texturePaths[0];
    unsigned char* temp = stbi_load(firstPath.c_str(), &width, &height, &nrChannels, 0);

    if (!temp) {
        std::cerr << "Failed to load base texture: " << firstPath << std::endl;
        return;
    }
    stbi_image_free(temp);

    // Allocate the storage on GPU (width x height x NumberOfImages)
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, width, height, texturePaths.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Upload images to layers
    for (int i = 0; i < texturePaths.size(); i++) {
        std::string fullPath = "textures/" + texturePaths[i];
        unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cerr << "Failed to load texture layer: " << fullPath << std::endl;
        }
    }

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    // Texture Parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (glfwExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        float aniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &aniso);
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY, aniso);
    }
}
