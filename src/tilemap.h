#pragma once
#include <string>
#include <vector>
#include "json.h"
#include "render.h"

enum { TILE_SOLID = 1, TILE_ONEWAY = 2, TILE_HAZARD = 4, TILE_LAVA = 8, TILE_WATER = 16, TILE_ICE = 32 };

struct TileDef {
    bool used = false;
    std::string name;
    int sprite = -1;
    std::vector<int> frames;    // picked by position, or played in order when fps is set
    std::vector<int> top;       // used when nothing of the same kind is above
    float fps = 0;              // water and lava ripple, the rest stays put
    uint8_t flags = 0;
    Rect box = { 0, 0, 16, 16 };    // hitbox inside the tile for hazard, lava and water
    bool edges = false;         // draw a dark outline where it meets empty space
    Color edge = { 0, 0, 0, 120 };
    float light = 0;            // glow radius, for lava
    Color light_color = WHITE;
};

// tiles are keyed by the character used in the scene files
struct Tileset {
    int size = 16;
    TileDef defs[128];

    bool load(const Json& j);   // data/tileset.json
    const TileDef* get(char c) const;
};

struct Tilemap {
    int w = 0, h = 0;
    std::vector<char> cells;

    char get(int x, int y) const { return x < 0 || y < 0 || x >= w || y >= h ? '.' : cells[size_t(y) * w + x]; }
    void set(int x, int y, char c) { if (x >= 0 && y >= 0 && x < w && y < h) cells[size_t(y) * w + x] = c; }
    void resize(int nw, int nh);        // keeps what fits
    bool load(const Json& rows);        // array of strings, one per row
    Json save() const;
};

uint8_t tile_flags(const Tilemap& m, const Tileset& ts, int x, int y);
// background layers skip the grassy top frames and the outlines, otherwise
// a wall behind reads exactly like a floor you could stand on
void tilemap_draw(const Tilemap& m, const Tileset& ts, Rect view, Color tint = WHITE, bool background = false, double time = 0);
