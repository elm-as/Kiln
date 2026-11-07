#include "editor_paint.h"
#include <algorithm>
#include <queue>
#include <utility>

void tile_box_fill(Tilemap& map, int x0, int y0, int x1, int y1, char c)
{
    if (map.w <= 0 || map.h <= 0)
        return;

    int min_x = std::max(0, std::min(x0, x1));
    int max_x = std::min(map.w - 1, std::max(x0, x1));
    int min_y = std::max(0, std::min(y0, y1));
    int max_y = std::min(map.h - 1, std::max(y0, y1));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            map.set(x, y, c);
        }
    }
}

int tile_flood_fill(Tilemap& map, int start_x, int start_y, char fill_c)
{
    if (start_x < 0 || start_y < 0 || start_x >= map.w || start_y >= map.h)
        return 0;

    char target_c = map.get(start_x, start_y);
    if (target_c == fill_c)
        return 0;

    int count = 0;
    std::queue<std::pair<int, int>> q;
    q.push({ start_x, start_y });
    map.set(start_x, start_y, fill_c);
    count++;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx < 0 || ny < 0 || nx >= map.w || ny >= map.h)
                continue;
            if (map.get(nx, ny) == target_c) {
                map.set(nx, ny, fill_c);
                count++;
                q.push({ nx, ny });
            }
        }
    }

    return count;
}
