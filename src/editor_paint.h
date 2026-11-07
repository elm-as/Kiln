#pragma once
#include "tilemap.h"

// Fills rectangular region between (x0, y0) and (x1, y1) inclusive with char c.
void tile_box_fill(Tilemap& map, int x0, int y0, int x1, int y1, char c);

// Flood fills connected region of identical tiles starting at (start_x, start_y) with fill_c.
// Returns count of modified tiles.
int tile_flood_fill(Tilemap& map, int start_x, int start_y, char fill_c);
