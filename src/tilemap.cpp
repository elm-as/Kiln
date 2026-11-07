#include "tilemap.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "assets.h"

bool Tileset::load(const Json& j)
{
    size = j["size"].i(16);
    for (TileDef& d : defs)
        d = TileDef();

    for (const auto& kv : j["tiles"].obj) {
        if (kv.first.size() != 1) {
            fprintf(stderr, "tileset: key '%s' should be one character\n", kv.first.c_str());
            continue;
        }
        unsigned char c = kv.first[0];
        if (c >= 128)
            continue;
        const Json& t = kv.second;
        TileDef& d = defs[c];
        d.used = true;
        d.name = t["name"].str(kv.first.c_str());
        d.sprite = sprite_id(t["sprite"].str());
        const SpriteSheet* s = sprite_get(d.sprite);
        auto frame_list = [&](const Json& list, std::vector<int>& out) {
            if (list.is_arr()) {
                for (const Json& f : list.arr)
                    out.push_back(f.is_str() && s ? s->frame(f.str()) : f.i());
            } else if (list.is_str() && s) {
                out.push_back(s->frame(list.str()));
            } else if (list.is_num()) {
                out.push_back(list.i());
            }
        };
        frame_list(t["frames"], d.frames);
        if (d.frames.empty())
            d.frames.push_back(0);
        frame_list(t["top"], d.top);
        d.fps = t["fps"].f(0);
        if (t["solid"].boolean_or(false)) d.flags |= TILE_SOLID;
        if (t["oneway"].boolean_or(false)) d.flags |= TILE_ONEWAY;
        if (t["hazard"].boolean_or(false)) d.flags |= TILE_HAZARD;
        if (t["lava"].boolean_or(false)) d.flags |= TILE_LAVA;
        if (t["water"].boolean_or(false)) d.flags |= TILE_WATER;
        if (t["ice"].boolean_or(false)) d.flags |= TILE_ICE;
        d.light = t["light"].f(0);
        if (t["light_color"].is_str()) {
            unsigned long v = strtoul(t["light_color"].str() + (t["light_color"].s[0] == '#'), 0, 16);
            d.light_color = rgb((uint32_t)v);
        }
        d.box = { 0, 0, (float)size, (float)size };
        if (t["box"].size() == 4)
            d.box = { t["box"][0].f(), t["box"][1].f(), t["box"][2].f(), t["box"][3].f() };
        d.edges = t["edges"].boolean_or(false);
    }
    return true;
}

const TileDef* Tileset::get(char c) const
{
    unsigned char u = (unsigned char)c;
    if (u >= 128 || !defs[u].used)
        return 0;
    return &defs[u];
}



void Tilemap::resize(int nw, int nh)
{
    std::vector<char> next(size_t(nw) * nh, '.');
    for (int y = 0; y < nh && y < h; y++)
        for (int x = 0; x < nw && x < w; x++)
            next[size_t(y) * nw + x] = cells[size_t(y) * w + x];
    w = nw;
    h = nh;
    cells.swap(next);
}

bool Tilemap::load(const Json& rows)
{
    w = h = 0;
    cells.clear();
    if (!rows.is_arr())
        return false;
    h = (int)rows.size();
    for (const Json& r : rows.arr)
        if ((int)r.s.size() > w)
            w = (int)r.s.size();
    cells.assign(size_t(w) * h, '.');
    for (int y = 0; y < h; y++) {
        const std::string& r = rows.arr[y].s;
        for (int x = 0; x < (int)r.size(); x++)
            cells[size_t(y) * w + x] = r[x] == ' ' ? '.' : r[x];
    }
    return true;
}

Json Tilemap::save() const
{
    Json rows = Json::array();
    for (int y = 0; y < h; y++)
        rows.push(Json::string(std::string(&cells[size_t(y) * w], w)));
    return rows;
}

uint8_t tile_flags(const Tilemap& m, const Tileset& ts, int x, int y)
{
    const TileDef* d = ts.get(m.get(x, y));
    return d ? d->flags : 0;
}

static uint32_t hash2(int x, int y)
{
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void tilemap_draw(const Tilemap& m, const Tileset& ts, Rect view, Color tint, bool background, double time)
{
    int s = ts.size;
    int x0 = (int)floorf(view.x / s), y0 = (int)floorf(view.y / s);
    int x1 = (int)floorf((view.x + view.w) / s), y1 = (int)floorf((view.y + view.h) / s);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= m.w) x1 = m.w - 1;
    if (y1 >= m.h) y1 = m.h - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            char c = m.get(x, y);
            const TileDef* d = ts.get(c);
            if (!d || d->frames.empty())
                continue;
            bool on_top = !background && !d->top.empty() && m.get(x, y - 1) != c && y > 0;
            const std::vector<int>& list = on_top ? d->top : d->frames;
            int frame;
            if (d->fps > 0) {
                // offset by column so a pool ripples along instead of blinking
                long step = (long)(time * d->fps) + x;
                frame = list[(size_t)(step % (long)list.size())];
            } else {
                frame = list[hash2(x, y) % list.size()];
            }
            vec2 p = { float(x * s), float(y * s) };
            SpriteOpts o;
            o.tint = tint;
            draw_spr_ex(d->sprite, frame, p, o);

            if (d->edges && !background) {
                bool solid_l = (tile_flags(m, ts, x - 1, y) & TILE_SOLID) || x == 0;
                bool solid_r = (tile_flags(m, ts, x + 1, y) & TILE_SOLID) || x == m.w - 1;
                bool solid_d = (tile_flags(m, ts, x, y + 1) & TILE_SOLID) || y == m.h - 1;
                if (!solid_l) draw_rect({ p.x, p.y, 1, (float)s }, d->edge);
                if (!solid_r) draw_rect({ p.x + s - 1, p.y, 1, (float)s }, d->edge);
                if (!solid_d) draw_rect({ p.x, p.y + s - 1, (float)s, 1 }, d->edge);
            }
        }
    }
}
