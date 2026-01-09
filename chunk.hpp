#pragma once

#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "block_manager.hpp"
#include "shader_s.hpp"

// Access the global manager defined in main.cpp
extern BlockManager globalBlockManager;

// Size of one chunk in blocks
const int CHUNK_SIZE = 16;

enum BlockID : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_DIRT = 1,
    BLOCK_STONE = 2,
    BLOCK_GRASS = 3
};

struct Chunk {
    int x, z; // Chunk coordinates
    unsigned int VAO, VBO = 0;
    int vertexCount = 0;

    bool isModified = false;

    // Block data: Y, X, Z
    BlockID blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

    // Constructor: Just sets coordinates. Does NOT generate yet.
    Chunk(int chunkX, int chunkZ) : x(chunkX), z(chunkZ) {
    }

    void draw(Shader& shader) {
        shader.setMat4("model", glm::mat4(1.0f));
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }

    void del() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void setBlock(int x, int y, int z, BlockID type) {
        // Bounds check
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
            blocks[y][x][z] = type;
            isModified = true;
            generateMesh(); // Rebuild the visuals
        }
    }

    bool isSolid(int x, int y, int z) {
        // If outside the chunk bounds, treat it as AIR (so we draw the edge faces)
        if (x < 0 || x >= CHUNK_SIZE ||
            y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
        {
            return false;
        }
        return blocks[y][x][z] != BLOCK_AIR;
    }

    void addVertex(std::vector<float>& v,
        float x, float y, float z,
        float nx, float ny, float nz,
        float r, float g, float b,
        float u, float v_tex, float layer) {

        // We now push 12 floats per vertex
        v.insert(v.end(), { x,y,z, nx,ny,nz, r,g,b, u,v_tex, layer });
    }

    void generateBlocks() {
        // TERRAIN SETTINGS
        float seed = 1234.0f;
        int baseHeight = 3;

        // --- Octave 1: The Mountain Layer (Big, wide shapes) ---
        float scale1 = 0.02f;
        float amp1 = 6.0f;

        // --- Octave 2: The Bumps Layer (Medium detail) ---
        float scale2 = 0.05f;
        float amp2 = 3.0f;

        // --- Octave 3: The Roughness Layer (Tiny detail) ---
        float scale3 = 0.1f;
        float amp3 = 1.0f;

        for (int x_local = 0; x_local < CHUNK_SIZE; x_local++) {
            for (int z_local = 0; z_local < CHUNK_SIZE; z_local++) {

                // Global Coordinates
                float worldX = (float)(x * CHUNK_SIZE + x_local);
                float worldZ = (float)(z * CHUNK_SIZE + z_local);

                // Calculate Octaves
                float noise1 = stb_perlin_noise3((worldX + seed) * scale1, (worldZ + seed) * scale1, 0, 0, 0, 0);
                float noise2 = stb_perlin_noise3((worldX + seed) * scale2, (worldZ + seed) * scale2, 0, 0, 0, 0);
                float noise3 = stb_perlin_noise3((worldX + seed) * scale3, (worldZ + seed) * scale3, 0, 0, 0, 0);

                // Combine them (Fractal Noise)
                float combinedHeight = (noise1 * amp1) + (noise2 * amp2) + (noise3 * amp3);

                // Convert to Integer Height
                int height = baseHeight + (int)combinedHeight;

                // Safety Clamp (Don't go outside chunk memory!)
                if (height < 1) height = 1;
                if (height >= CHUNK_SIZE) height = CHUNK_SIZE - 1;

                // Fill Blocks
                for (int y = 0; y < CHUNK_SIZE; y++) {
                    if (y < height) {
                        blocks[y][x_local][z_local] = BLOCK_STONE;
                    }
                    else if (y == height) {
                        blocks[y][x_local][z_local] = BLOCK_GRASS;
                    }
                    else {
                        blocks[y][x_local][z_local] = BLOCK_AIR;
                    }
                }
            }
        }
    }

    void generateMesh() {
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
        }

        std::vector<float> vertices;

        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int i = 0; i < CHUNK_SIZE; i++) {
                for (int j = 0; j < CHUNK_SIZE; j++) {

                    BlockID block = blocks[y][i][j];
                    if (block == BLOCK_AIR) continue;

                    float wx = x * CHUNK_SIZE + i;
                    float wy = (float)y;
                    float wz = z * CHUNK_SIZE + j;

                    BlockFaceTextures tex = globalBlockManager.blockData[block];

                    // === TOP FACE (+Y) ===
                    if (!isSolid(i, y + 1, j)) {
                        addVertex(vertices, wx, wy + 1, wz, 0, 1, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.topLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz, 0, 1, 0, 1, 1, 1, 1.0f, 0.0f, (float)tex.topLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz + 1, 0, 1, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.topLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz + 1, 0, 1, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.topLayer);
                        addVertex(vertices, wx, wy + 1, wz + 1, 0, 1, 0, 1, 1, 1, 0.0f, 1.0f, (float)tex.topLayer);
                        addVertex(vertices, wx, wy + 1, wz, 0, 1, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.topLayer);
                    }

                    // === BOTTOM FACE (-Y) ===
                    if (!isSolid(i, y - 1, j)) {
                        addVertex(vertices, wx, wy, wz, 0, -1, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.bottomLayer);
                        addVertex(vertices, wx + 1, wy, wz + 1, 0, -1, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.bottomLayer);
                        addVertex(vertices, wx + 1, wy, wz, 0, -1, 0, 1, 1, 1, 1.0f, 0.0f, (float)tex.bottomLayer);
                        addVertex(vertices, wx + 1, wy, wz + 1, 0, -1, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.bottomLayer);
                        addVertex(vertices, wx, wy, wz, 0, -1, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.bottomLayer);
                        addVertex(vertices, wx, wy, wz + 1, 0, -1, 0, 1, 1, 1, 0.0f, 1.0f, (float)tex.bottomLayer);
                    }

                    // === FRONT FACE (+Z) ===
                    if (!isSolid(i, y, j + 1)) {
                        addVertex(vertices, wx, wy, wz + 1, 0, 0, 1, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy, wz + 1, 0, 0, 1, 1, 1, 1, 1.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz + 1, 0, 0, 1, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz + 1, 0, 0, 1, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy + 1, wz + 1, 0, 0, 1, 1, 1, 1, 0.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy, wz + 1, 0, 0, 1, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                    }

                    // === BACK FACE (-Z) ===
                    if (!isSolid(i, y, j - 1)) {
                        addVertex(vertices, wx, wy, wz, 0, 0, -1, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy + 1, wz, 0, 0, -1, 1, 1, 1, 0.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz, 0, 0, -1, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz, 0, 0, -1, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy, wz, 0, 0, -1, 1, 1, 1, 1.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy, wz, 0, 0, -1, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                    }

                    // === LEFT FACE (-X) ===
                    if (!isSolid(i - 1, y, j)) {
                        addVertex(vertices, wx, wy + 1, wz, -1, 0, 0, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy, wz, -1, 0, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy, wz + 1, -1, 0, 0, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy, wz + 1, -1, 0, 0, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy + 1, wz + 1, -1, 0, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx, wy + 1, wz, -1, 0, 0, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                    }

                    // === RIGHT FACE (+X) ===
                    if (!isSolid(i + 1, y, j)) {
                        addVertex(vertices, wx + 1, wy + 1, wz, 1, 0, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz + 1, 1, 0, 0, 1, 1, 1, 1.0f, 0.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy, wz + 1, 1, 0, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy, wz + 1, 1, 0, 0, 1, 1, 1, 1.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy, wz, 1, 0, 0, 1, 1, 1, 0.0f, 1.0f, (float)tex.sideLayer);
                        addVertex(vertices, wx + 1, wy + 1, wz, 1, 0, 0, 1, 1, 1, 0.0f, 0.0f, (float)tex.sideLayer);
                    }
                }
            }
        }

        vertexCount = vertices.size() / 12;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        // Pos (3)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal (3)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // Color (3)
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        // TexCoords + Layer (3)
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(9 * sizeof(float)));
        glEnableVertexAttribArray(3);
    }
};
