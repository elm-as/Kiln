#include "editor.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "assets.h"
#include "physics.h"
#include "ui.h"

extern EditorHooks g_ed_hooks;
extern Scene* g_ed_scene;
extern Input g_ed_in;
extern vec2 g_ed_cam;
extern float g_ed_zoom;
extern int g_ed_tool;
extern std::string g_ed_prefab;
extern Entity g_ed_selected;
extern bool g_ed_dragging;
extern bool g_ed_show_colliders, g_ed_show_grid, g_ed_show_links, g_ed_show_lighting;
extern bool g_ed_box_active;
extern int g_ed_box_x0, g_ed_box_y0, g_ed_box_x1, g_ed_box_y1;

vec2 editor_screen_to_world(float x, float y);
Rect editor_entity_bounds(Entity e);
Entity editor_pick(vec2 p);

static const Color GRID = { 255, 255, 255, 22 };
static const Color SELECT = { 255, 210, 80, 255 };

static vec2 world_to_screen(vec2 p)
{
    return (p - g_ed_cam) * g_ed_zoom;
}

static const Json* script_props(Entity e)
{
    Script* s = g_ed_scene->world.get<Script>(e);
    return s ? &s->props : 0;
}

// same link name, same color
static Color link_color(const std::string& name)
{
    unsigned h = 2166136261u;
    for (char ch : name)
        h = (h ^ (unsigned char)ch) * 16777619u;
    return { (uint8_t)(120 + h % 136), (uint8_t)(120 + (h >> 8) % 136), (uint8_t)(120 + (h >> 16) % 136), 230 };
}

static void draw_script_hints(float px)
{
    World& w = g_ed_scene->world;
    std::map<std::string, std::vector<vec2>> links;
    w.each<Script, Transform>([&](Entity e, Script& s, Transform& t) {
        const Json& p = s.props;
        if (p["w"].is_num() && p["h"].is_num()) {
            float zw = (float)p["w"].n, zh = (float)p["h"].n;
            Rect zone = { t.pos.x - zw / 2, t.pos.y - zh, zw, zh };
            draw_rect(zone, { 110, 200, 255, 28 });
            draw_rect_lines(zone, px, { 110, 200, 255, 140 });
        }
        float dx = p["dx"].is_num() ? (float)p["dx"].n : 0;
        float dy = p["dy"].is_num() ? (float)p["dy"].n : 0;
        if (dx != 0 || dy != 0) {
            Rect r = editor_entity_bounds(e);
            r.x += dx;
            r.y += dy;
            draw_line(t.pos, t.pos + vec2{ dx, dy }, px * 2, { 255, 170, 80, 200 });
            draw_rect_lines(r, px, { 255, 170, 80, 150 });
        }
        if (p["link"].is_str() && !p["link"].s.empty())
            links[p["link"].s].push_back(t.pos);
    });
    if (!g_ed_show_links)
        return;
    for (auto& kv : links) {
        Color c = link_color(kv.first);
        for (size_t i = 1; i < kv.second.size(); i++)
            draw_line(kv.second[0], kv.second[i], px * 2, c);
        for (vec2 p : kv.second)
            draw_rect({ p.x - 2 * px, p.y - 2 * px, 4 * px, 4 * px }, c);
    }
}

// what a click would place, faded, where it would go
static void draw_ghost()
{
    if (g_ed_tool != 2 || g_ed_prefab.empty() || ui_want_mouse() || g_ed_dragging)
        return;
    vec2 m = editor_screen_to_world(g_ed_in.mouse_px, g_ed_in.mouse_py);
    if (editor_pick(m))
        return;
    int ts = g_ed_hooks.tileset ? g_ed_hooks.tileset->size : 16;
    float snap = g_ed_in.down[KEY_ALT] ? 1.f : ts / 2.f;
    vec2 p = { floorf(m.x / snap + 0.5f) * snap, floorf(m.y / snap + 0.5f) * snap };
    const Json* pre = prefab_get(g_ed_prefab.c_str());
    int id = pre ? sprite_id((*pre)["Sprite"]["sprite"].str("")) : -1;
    const SpriteSheet* sh = sprite_get(id);
    if (!sh) {
        draw_rect_lines({ p.x - 4, p.y - 4, 8, 8 }, 1 / g_ed_zoom, { 110, 200, 255, 200 });
        return;
    }
    int frame = 0;
    if ((*pre)["Sprite"]["frame"].is_str()) {
        for (size_t i = 0; i < sh->frame_names.size(); i++)
            if (sh->frame_names[i] == (*pre)["Sprite"]["frame"].s)
                frame = (int)i;
    }
    SpriteOpts o;
    o.origin = sh->origin;
    o.tint = { 255, 255, 255, 120 };
    draw_spr_ex(id, frame, p, o);
}

void editor_draw_overlays(Rect view)
{
    World& w = g_ed_scene->world;
    int ts = g_ed_hooks.tileset ? g_ed_hooks.tileset->size : 16;
    float px = 1 / g_ed_zoom;    // one screen pixel in world units

    if (g_ed_show_grid) {
        int x0 = (int)floorf(view.x / ts), x1 = (int)ceilf((view.x + view.w) / ts);
        int y0 = (int)floorf(view.y / ts), y1 = (int)ceilf((view.y + view.h) / ts);
        for (int x = x0; x <= x1; x++)
            draw_rect({ x * (float)ts, view.y, px, view.h }, GRID);
        for (int y = y0; y <= y1; y++)
            draw_rect({ view.x, y * (float)ts, view.w, px }, GRID);
    }

    // map bounds
    Rect bounds = { 0, 0, (float)g_ed_scene->map.w * ts, (float)g_ed_scene->map.h * ts };
    draw_rect_lines(bounds, 2 * px, { 120, 180, 255, 160 });

    if (g_ed_show_colliders) {
        w.each<Collider, Transform>([&](Entity, Collider& c, Transform& t) {
            Color col = c.trigger ? Color{ 80, 255, 160, 180 } : Color{ 255, 90, 90, 180 };
            if (c.shape == SHAPE_CIRCLE)
                draw_circle_lines(t.pos + c.offset, c.radius, px, col);
            else
                draw_rect_lines(collider_rect(t, c), px, col);
        });
    }

    draw_script_hints(px);
    // things with nothing to draw still need to be seen and clicked
    w.each<Info, Transform>([&](Entity e, Info&, Transform& t) {
        if (!w.has<Sprite>(e))
            draw_rect_lines({ t.pos.x - 4, t.pos.y - 4, 8, 8 }, px * 2, { 110, 200, 255, 220 });
    });
    draw_ghost();

    if (g_ed_selected && w.alive(g_ed_selected)) {
        Rect r = editor_entity_bounds(g_ed_selected);
        draw_rect_lines({ r.x - px, r.y - px, r.w + 2 * px, r.h + 2 * px }, px * 2, SELECT);
    }

    if (g_ed_box_active) {
        int bx0 = std::min(g_ed_box_x0, g_ed_box_x1), bx1 = std::max(g_ed_box_x0, g_ed_box_x1);
        int by0 = std::min(g_ed_box_y0, g_ed_box_y1), by1 = std::max(g_ed_box_y0, g_ed_box_y1);
        Rect box_r = { bx0 * (float)ts, by0 * (float)ts, (bx1 - bx0 + 1) * (float)ts, (by1 - by0 + 1) * (float)ts };
        draw_rect(box_r, { 255, 210, 80, 50 });
        draw_rect_lines(box_r, 2 * px, SELECT);
    }

    if (g_ed_show_lighting) {
        draw_rect(view, { 10, 12, 22, 180 });
        if (g_ed_hooks.tileset) {
            int x0 = (int)floorf(view.x / ts), x1 = (int)ceilf((view.x + view.w) / ts);
            int y0 = (int)floorf(view.y / ts), y1 = (int)ceilf((view.y + view.h) / ts);
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    char c = g_ed_scene->map.get(x, y);
                    const TileDef* d = g_ed_hooks.tileset->get(c);
                    if (d && d->light > 0)
                        draw_light({ x * (float)ts + ts * 0.5f, y * (float)ts + ts * 0.5f }, d->light, d->light_color);
                }
            }
        }
        w.each<Light, Transform>([&](Entity, Light& l, Transform& t) {
            draw_light(t.pos + l.offset, l.radius, l.color);
            draw_circle_lines(t.pos + l.offset, l.radius, px, { l.color.r, l.color.g, l.color.b, 90 });
        });
    }

    // brush preview
    if (g_ed_tool != 2 && !ui_want_mouse()) {
        vec2 m = editor_screen_to_world(g_ed_in.mouse_px, g_ed_in.mouse_py);
        float x = floorf(m.x / ts) * ts, y = floorf(m.y / ts) * ts;
        draw_rect_lines({ x, y, (float)ts, (float)ts }, px * 2, WHITE);
    }
}

// names for what has nothing to draw, and for links, in screen space
void editor_draw_labels()
{
    World& w = g_ed_scene->world;
    std::map<std::string, vec2> link_at;
    w.each<Info, Transform>([&](Entity e, Info& info, Transform& t) {
        if (!w.has<Sprite>(e)) {
            vec2 s = world_to_screen(t.pos);
            ui_text_at(s.x + 6, s.y - 8, info.prefab.c_str(), { 110, 200, 255, 255 });
        }
        const Json* p = script_props(e);
        if (g_ed_show_links && p && (*p)["link"].is_str() && !link_at.count((*p)["link"].s))
            link_at[(*p)["link"].s] = t.pos;
    });
    for (auto& kv : link_at) {
        vec2 s = world_to_screen(kv.second);
        ui_text_at(s.x - 10, s.y + 4, ("~" + kv.first).c_str(), link_color(kv.first));
    }
}

