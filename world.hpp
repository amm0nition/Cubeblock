#pragma once

#include <map>
#include <string>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <glm/glm.hpp> 

#include "chunk.hpp"

namespace fs = std::filesystem;

class World {
public:
    // === Settings ===
    bool isInfinite = true;
    int renderDistance = 16;

    // Boundaries (Used if isInfinite is false)
    const int WORLD_MIN_X = -4;
    const int WORLD_MAX_X = 4;
    const int WORLD_MIN_Z = -4;
    const int WORLD_MAX_Z = 4;

    // === Storage ===
    std::map<std::pair<int, int>, Chunk*> activeChunks;
    std::string saveFolder = "saves/world1/";

    // Get a block ID at global world coordinates
    BlockID getBlock(int x, int y, int z) {
        // 1. Calculate Chunk Coordinate
        int cx = static_cast<int>(floor(x / (float)CHUNK_SIZE));
        int cz = static_cast<int>(floor(z / (float)CHUNK_SIZE));

        // 2. Calculate Local Block Coordinate
        int lx = x - (cx * CHUNK_SIZE);
        int lz = z - (cz * CHUNK_SIZE);

        // 3. Find Chunk
        if (activeChunks.find({ cx, cz }) != activeChunks.end()) {
            Chunk* c = activeChunks[{cx, cz}];
            // 4. Check Bounds (Y is local to chunk height)
            if (y >= 0 && y < CHUNK_SIZE && lx >= 0 && lz >= 0) {
                return c->blocks[y][lx][lz];
            }
        }
        return BLOCK_AIR;
    }

    World() {
        // Ensure save folder exists
        if (!fs::exists(saveFolder)) {
            fs::create_directories(saveFolder);
        }
    }

    void setBlock(int x, int y, int z, BlockID type) {
        int cx = static_cast<int>(floor(x / (float)CHUNK_SIZE));
        int cz = static_cast<int>(floor(z / (float)CHUNK_SIZE));
        int lx = x - (cx * CHUNK_SIZE);
        int lz = z - (cz * CHUNK_SIZE);

        if (activeChunks.find({ cx, cz }) != activeChunks.end()) {
            activeChunks[{cx, cz}]->setBlock(lx, y, lz, type);
        }
    }

    void saveAllChunks() {
        //std::cout << "Saving " << activeChunks.size() << " chunks..." << std::endl;
        for (auto& pair : activeChunks) {
            saveChunk(pair.second);
        }
        //std::cout << "World saved." << std::endl;
    }

    void update(glm::vec3 playerPos) {
        int px = static_cast<int>(floor(playerPos.x / CHUNK_SIZE));
        int pz = static_cast<int>(floor(playerPos.z / CHUNK_SIZE));

        // 1. Load/Generate new chunks
        for (int x = px - renderDistance; x <= px + renderDistance; x++) {
            for (int z = pz - renderDistance; z <= pz + renderDistance; z++) {

                // Infinite check
                if (!isInfinite) {
                    if (x < WORLD_MIN_X || x >= WORLD_MAX_X || z < WORLD_MIN_Z || z >= WORLD_MAX_Z) continue;
                }

                // If chunk doesn't exist in memory, load or create it
                if (activeChunks.find({ x, z }) == activeChunks.end()) {
                    Chunk* newChunk = new Chunk(x, z);

                    // TRY LOADING FROM FILE
                    if (loadChunk(newChunk)) {
                        newChunk->generateMesh(); // Mesh only after loading data
                    }
                    else {
                        // File didn't exist, so generate fresh terrain
                        newChunk->generateBlocks();
                        newChunk->generateMesh();
                        // Optional: Save immediately so the file exists next time
                        saveChunk(newChunk);
                    }

                    activeChunks[{x, z}] = newChunk;
                }
            }
        }

        // 2. Unload far chunks
        auto it = activeChunks.begin();
        while (it != activeChunks.end()) {
            int cx = it->second->x;
            int cz = it->second->z;

            if (abs(cx - px) > renderDistance + 1 || abs(cz - pz) > renderDistance + 1) {
                // SAVE BEFORE DELETING
                saveChunk(it->second);

                it->second->del();
                delete it->second;
                it = activeChunks.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void render(Shader& shader) {
        for (auto& pair : activeChunks) {
            pair.second->draw(shader);
        }
    }

private:
    // Save chunk blocks to binary file
    void saveChunk(Chunk* c) {
        if (!c->isModified) return;

        std::string filename = saveFolder + "chunk_" + std::to_string(c->x) + "_" + std::to_string(c->z) + ".bin";
        std::ofstream out(filename, std::ios::binary);
        if (out.is_open()) {
            out.write((char*)&c->blocks, sizeof(c->blocks));
            out.close();

            // Reset flag after successful save
            c->isModified = false;
        }
    }

    // Load chunk blocks from binary file
    bool loadChunk(Chunk* c) {
        std::string filename = saveFolder + "chunk_" + std::to_string(c->x) + "_" + std::to_string(c->z) + ".bin";
        std::ifstream in(filename, std::ios::binary | std::ios::ate); // Open at the end to check size

        if (in.is_open()) {
            // Check file size
            std::streamsize fileSize = in.tellg();
            std::streamsize expectedSize = sizeof(c->blocks);

            if (fileSize != expectedSize) {
                // The file is the wrong size (old version?), so we reject it
                in.close();
                return false;
            }

            // Go back to start and read
            in.seekg(0, std::ios::beg);
            in.read((char*)&c->blocks, expectedSize);
            in.close();
            return true;
        }
        return false;
    }
};
