#ifndef _H_TILED_CANVAS_
#define _H_TILED_CANVAS_

#include "types.h"
#include "config.h"
#include "tile.h"
#include "blend.h"
#include "primitives.h"
#include <unordered_map>
#include <memory>
#include <functional>

// Floor division that works correctly for negative numbers
inline i32 floorDiv(i32 a, i32 b) {
    return (a >= 0) ? (a / b) : ((a - b + 1) / b);
}

// Positive modulo that works correctly for negative numbers
inline u32 floorMod(i32 a, i32 b) {
    i32 m = a % b;
    return static_cast<u32>(m < 0 ? m + b : m);
}

class TiledCanvas {
public:
    std::unordered_map<u64, std::unique_ptr<Tile>> tiles;
    u32 width = 0;
    u32 height = 0;

    TiledCanvas() = default;
    TiledCanvas(u32 w, u32 h) : width(w), height(h) {}

    // Non-copyable, but movable
    TiledCanvas(const TiledCanvas&) = delete;
    TiledCanvas& operator=(const TiledCanvas&) = delete;
    TiledCanvas(TiledCanvas&&) = default;
    TiledCanvas& operator=(TiledCanvas&&) = default;

    // Create deep copy
    std::unique_ptr<TiledCanvas> clone() const;

    void resize(u32 newWidth, u32 newHeight);

    // Pixel access - inline for hot path
    u32 getPixel(i32 x, i32 y) const {
        i32 tileX = floorDiv(x, static_cast<i32>(Config::TILE_SIZE));
        i32 tileY = floorDiv(y, static_cast<i32>(Config::TILE_SIZE));
        u64 key = makeTileKey(tileX, tileY);

        auto it = tiles.find(key);
        if (it == tiles.end()) return 0;

        u32 localX = floorMod(x, static_cast<i32>(Config::TILE_SIZE));
        u32 localY = floorMod(y, static_cast<i32>(Config::TILE_SIZE));
        return it->second->getPixel(localX, localY);
    }

    void setPixel(i32 x, i32 y, u32 color) {
        i32 tileX = floorDiv(x, static_cast<i32>(Config::TILE_SIZE));
        i32 tileY = floorDiv(y, static_cast<i32>(Config::TILE_SIZE));
        u64 key = makeTileKey(tileX, tileY);

        u32 localX = floorMod(x, static_cast<i32>(Config::TILE_SIZE));
        u32 localY = floorMod(y, static_cast<i32>(Config::TILE_SIZE));

        u8 alpha = color & 0xFF;

        auto it = tiles.find(key);
        if (it == tiles.end()) {
            if (alpha == 0) return;
            auto tile = std::make_unique<Tile>();
            tile->setPixel(localX, localY, color);
            tiles[key] = std::move(tile);
        } else {
            it->second->setPixel(localX, localY, color);
        }
    }

    void blendPixel(i32 x, i32 y, u32 color, BlendMode mode = BlendMode::Normal, f32 opacity = 1.0f) {
        if ((color & 0xFF) == 0) return;
        u32 dst = getPixel(x, y);
        u32 result = Blend::blend(dst, color, mode, opacity);
        setPixel(x, y, result);
    }

    void alphaBlendPixel(i32 x, i32 y, u32 color) {
        if ((color & 0xFF) == 0) return;
        u32 dst = getPixel(x, y);
        u32 result = Blend::alphaBlend(dst, color);
        setPixel(x, y, result);
    }

    void clear() { tiles.clear(); }
    void clearRect(const Recti& rect);
    void fill(u32 color);

    // Template functions - must stay in header
    template<typename Func>
    void forEachTile(Func&& callback) const {
        for (const auto& [key, tile] : tiles) {
            i32 tileX, tileY;
            extractTileCoords(key, tileX, tileY);
            callback(tileX, tileY, *tile);
        }
    }

    template<typename Func>
    void forEachPixel(Func&& callback) const {
        for (const auto& [key, tile] : tiles) {
            i32 tileX, tileY;
            extractTileCoords(key, tileX, tileY);
            i32 baseX = tileX * static_cast<i32>(Config::TILE_SIZE);
            i32 baseY = tileY * static_cast<i32>(Config::TILE_SIZE);

            for (u32 ly = 0; ly < Config::TILE_SIZE; ++ly) {
                i32 y = baseY + static_cast<i32>(ly);
                for (u32 lx = 0; lx < Config::TILE_SIZE; ++lx) {
                    i32 x = baseX + static_cast<i32>(lx);
                    u32 pixel = tile->getPixel(lx, ly);
                    callback(x, y, pixel);
                }
            }
        }
    }

    void pruneEmptyTiles();
    void pruneOutOfBounds();

    Recti getBounds() const;
    Recti getContentBounds() const;

    size_t getTileCount() const { return tiles.size(); }
    size_t getMemoryUsage() const { return tiles.size() * sizeof(Tile); }

    // Tile access
    Tile* getOrCreateTile(i32 tileX, i32 tileY);
    const Tile* getTile(i32 tileX, i32 tileY) const;
    Tile* getTile(i32 tileX, i32 tileY);

    // Undo support
    // Clone only tiles that intersect the given rect (in pixel coords)
    std::unordered_map<u64, std::unique_ptr<Tile>> cloneTilesInRect(const Recti& bounds) const;

    // Clone a single tile by key (returns nullptr if tile doesn't exist)
    std::unique_ptr<Tile> cloneTileByKey(u64 key) const;

    // Restore tiles from a map, swapping with current tiles
    // Returns the tiles that were replaced (for redo)
    std::unordered_map<u64, std::unique_ptr<Tile>> swapTiles(
        std::unordered_map<u64, std::unique_ptr<Tile>>& newTiles);

    // Get tile keys that overlap a rect (in pixel coords)
    std::vector<u64> getTileKeysInRect(const Recti& bounds) const;
};

#endif
