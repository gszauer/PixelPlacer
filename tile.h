#ifndef _H_TILE_
#define _H_TILE_

#include "types.h"
#include "config.h"
#include <cstring>
#include <memory>

struct Tile {
    u32 pixels[Config::TILE_SIZE * Config::TILE_SIZE];

    Tile() {
        clear();
    }

    void clear() {
        std::memset(pixels, 0, sizeof(pixels));
    }

    u32 getPixel(u32 localX, u32 localY) const {
        return pixels[localY * Config::TILE_SIZE + localX];
    }

    void setPixel(u32 localX, u32 localY, u32 color) {
        pixels[localY * Config::TILE_SIZE + localX] = color;
    }

    bool isEmpty() const {
        for (u32 i = 0; i < Config::TILE_SIZE * Config::TILE_SIZE; ++i) {
            if ((pixels[i] & 0xFF) != 0) { // Check alpha channel
                return false;
            }
        }
        return true;
    }

    // Create a deep copy
    std::unique_ptr<Tile> clone() const {
        auto copy = std::make_unique<Tile>();
        std::memcpy(copy->pixels, pixels, sizeof(pixels));
        return copy;
    }
};

// Generate tile key from signed tile coordinates
// Uses offset encoding to map signed range to unsigned for hashing
inline u64 makeTileKey(i32 tileX, i32 tileY) {
    u32 ux = static_cast<u32>(tileX) + 0x80000000u;
    u32 uy = static_cast<u32>(tileY) + 0x80000000u;
    return (static_cast<u64>(ux) << 32) | static_cast<u64>(uy);
}

inline void extractTileCoords(u64 key, i32& tileX, i32& tileY) {
    u32 ux = static_cast<u32>(key >> 32);
    u32 uy = static_cast<u32>(key & 0xFFFFFFFF);
    tileX = static_cast<i32>(ux - 0x80000000u);
    tileY = static_cast<i32>(uy - 0x80000000u);
}

#endif
