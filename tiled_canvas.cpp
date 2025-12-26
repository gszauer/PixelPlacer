#include "tiled_canvas.h"
#include <limits>
#include <algorithm>

std::unique_ptr<TiledCanvas> TiledCanvas::clone() const {
    auto copy = std::make_unique<TiledCanvas>(width, height);
    for (const auto& [key, tile] : tiles) {
        copy->tiles[key] = tile->clone();
    }
    return copy;
}

void TiledCanvas::resize(u32 newWidth, u32 newHeight) {
    width = newWidth;
    height = newHeight;
    pruneOutOfBounds();
}

void TiledCanvas::clearRect(const Recti& rect) {
    i32 startX = std::max(0, rect.x);
    i32 startY = std::max(0, rect.y);
    i32 endX = std::min(static_cast<i32>(width), rect.x + rect.w);
    i32 endY = std::min(static_cast<i32>(height), rect.y + rect.h);

    for (i32 y = startY; y < endY; ++y) {
        for (i32 x = startX; x < endX; ++x) {
            setPixel(x, y, 0);
        }
    }
}

void TiledCanvas::fill(u32 color) {
    if ((color & 0xFF) == 0) {
        clear();
        return;
    }

    u32 tilesX = (width + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
    u32 tilesY = (height + Config::TILE_SIZE - 1) / Config::TILE_SIZE;

    for (u32 ty = 0; ty < tilesY; ++ty) {
        for (u32 tx = 0; tx < tilesX; ++tx) {
            u64 key = makeTileKey(tx, ty);
            auto tile = std::make_unique<Tile>();

            u32 startX = tx * Config::TILE_SIZE;
            u32 startY = ty * Config::TILE_SIZE;
            u32 endX = std::min(startX + Config::TILE_SIZE, width);
            u32 endY = std::min(startY + Config::TILE_SIZE, height);

            for (u32 y = startY; y < endY; ++y) {
                for (u32 x = startX; x < endX; ++x) {
                    tile->setPixel(x - startX, y - startY, color);
                }
            }
            tiles[key] = std::move(tile);
        }
    }
}

void TiledCanvas::pruneEmptyTiles() {
    for (auto it = tiles.begin(); it != tiles.end(); ) {
        if (it->second->isEmpty()) {
            it = tiles.erase(it);
        } else {
            ++it;
        }
    }
}

void TiledCanvas::pruneOutOfBounds() {
    i32 maxTileX = static_cast<i32>((width + Config::TILE_SIZE - 1) / Config::TILE_SIZE);
    i32 maxTileY = static_cast<i32>((height + Config::TILE_SIZE - 1) / Config::TILE_SIZE);

    for (auto it = tiles.begin(); it != tiles.end(); ) {
        i32 tileX, tileY;
        extractTileCoords(it->first, tileX, tileY);
        if (tileX < 0 || tileY < 0 || tileX >= maxTileX || tileY >= maxTileY) {
            it = tiles.erase(it);
        } else {
            ++it;
        }
    }
}

Recti TiledCanvas::getBounds() const {
    if (tiles.empty()) return Recti(0, 0, 0, 0);

    i32 minX = std::numeric_limits<i32>::max();
    i32 minY = std::numeric_limits<i32>::max();
    i32 maxX = std::numeric_limits<i32>::min();
    i32 maxY = std::numeric_limits<i32>::min();

    for (const auto& [key, tile] : tiles) {
        i32 tileX, tileY;
        extractTileCoords(key, tileX, tileY);
        i32 x = tileX * static_cast<i32>(Config::TILE_SIZE);
        i32 y = tileY * static_cast<i32>(Config::TILE_SIZE);
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x + static_cast<i32>(Config::TILE_SIZE));
        maxY = std::max(maxY, y + static_cast<i32>(Config::TILE_SIZE));
    }

    return Recti(minX, minY, maxX - minX, maxY - minY);
}

Recti TiledCanvas::getContentBounds() const {
    if (tiles.empty()) return Recti(0, 0, 0, 0);

    i32 minX = std::numeric_limits<i32>::max();
    i32 minY = std::numeric_limits<i32>::max();
    i32 maxX = std::numeric_limits<i32>::min();
    i32 maxY = std::numeric_limits<i32>::min();
    bool foundContent = false;

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
                if ((pixel & 0xFF) > 0) {
                    foundContent = true;
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    maxX = std::max(maxX, x + 1);
                    maxY = std::max(maxY, y + 1);
                }
            }
        }
    }

    if (!foundContent) return Recti(0, 0, 0, 0);
    return Recti(minX, minY, maxX - minX, maxY - minY);
}

Tile* TiledCanvas::getOrCreateTile(i32 tileX, i32 tileY) {
    u64 key = makeTileKey(tileX, tileY);
    auto it = tiles.find(key);
    if (it == tiles.end()) {
        auto tile = std::make_unique<Tile>();
        Tile* ptr = tile.get();
        tiles[key] = std::move(tile);
        return ptr;
    }
    return it->second.get();
}

const Tile* TiledCanvas::getTile(i32 tileX, i32 tileY) const {
    u64 key = makeTileKey(tileX, tileY);
    auto it = tiles.find(key);
    return it != tiles.end() ? it->second.get() : nullptr;
}

Tile* TiledCanvas::getTile(i32 tileX, i32 tileY) {
    u64 key = makeTileKey(tileX, tileY);
    auto it = tiles.find(key);
    return it != tiles.end() ? it->second.get() : nullptr;
}
