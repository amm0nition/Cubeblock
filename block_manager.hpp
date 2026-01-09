#pragma once

#include <map>
#include <string>
#include <vector>

// Stores which "layer" of the Texture Array corresponds to which face
struct BlockFaceTextures {
    int topLayer;
    int bottomLayer;
    int sideLayer;
};

class BlockManager {
public:
    // Holds the data loaded from JSON: Block ID -> Texture Info
    std::map<int, BlockFaceTextures> blockData;

    // The OpenGL ID for the GL_TEXTURE_2D_ARRAY
    unsigned int textureArrayID;

    // Call this once at startup
    void loadBlocks(const char* configPath);
};
