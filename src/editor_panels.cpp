#include "editor.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "assets.h"
#include "platform.h"
#include "ui.h"

extern EditorHooks g_ed_hooks;
extern Scene* g_ed_scene;
extern std::string g_ed_path;
extern bool g_ed_open;
extern Input g_ed_in;
extern vec2 g_ed_cam;
extern float g_ed_zoom;
extern int g_ed_tool;
extern char g_ed_brush;
extern std::string g_ed_prefab;
extern Entity g_ed_selected;
extern std::vector<std::string> g_ed_undo_stack, g_ed_redo_stack;
extern std::string g_ed_pending;
extern bool g_ed_gesture;
extern std::string g_ed_toast;
extern double g_ed_toast_until;
extern bool g_ed_show_colliders, g_ed_show_grid, g_ed_show_links, g_ed_show_lighting, g_ed_bucket_mode;
extern std::string g_ed_prefab_filter;

std::string editor_snapshot();
vec2 editor_screen_to_world(float x, float y);
void editor_save();
void editor_undo();
void editor_redo();
void editor_duplicate();
void editor_draw_overlays(Rect view);
void editor_draw_labels();

static const Color SELECT = { 255, 210, 80, 255 };

static void tile_icon(Rect r, char c)
{
    const TileDef* d = g_ed_hooks.tileset ? g_ed_hooks.tileset->get(c) : 0;
    if (!d || d->frames.empty())
        return;
    SpriteOpts o;
    float s = (r.h - 2) / g_ed_hooks.tileset->size;
    o.scale = { s, s };
    draw_spr_ex(d->sprite, !d->top.empty() ? d->top[0] : d->frames[0], { r.x + r.w - r.h, r.y + 1 }, o);
}

static void left_panel(float h)
{
    ui_panel_begin("Editor", { 8, 8, 230, h - 48 });

    const char* names[] = { "1 Tiles", "2 Back", "3 Entities" };
    for (int i = 0; i < 3; i++) {
        if (i) ui_same_line();
        std::string label = g_ed_tool == i ? std::string("[") + names[i] + "]" : std::string(names[i]);
        if (ui_button((label + "##tool" + std::to_string(i)).c_str()))
            g_ed_tool = i;
    }
    if (ui_button("Save  ctrl+s"))
        editor_save();
    ui_same_line();
    if (ui_button("Undo"))
        editor_undo();
    ui_same_line();
    if (ui_button("Redo"))
        editor_redo();
    ui_checkbox("grid (g)", &g_ed_show_grid);
    ui_same_line();
    ui_checkbox("colliders (c)", &g_ed_show_colliders);
    ui_checkbox("links (l)", &g_ed_show_links);
    ui_same_line();
    ui_checkbox("lighting (o)", &g_ed_show_lighting);
    ui_checkbox("bucket (b)", &g_ed_bucket_mode);
    ui_separator();

    if (g_ed_tool != 2 && g_ed_hooks.tileset) {
        ui_label("brush (q picks)");
        for (int c = 33; c < 127; c++) {
            const TileDef* d = g_ed_hooks.tileset->get((char)c);
            if (!d)
                continue;
            char label[64];
            snprintf(label, sizeof(label), "%c  %s##tile%d", c, d->name.c_str(), c);
            if (ui_selectable(label, g_ed_brush == c))
                g_ed_brush = (char)c;
            tile_icon(ui_last_rect(), (char)c);
        }
        ui_label_colored({ 140, 145, 165, 255 }, "left paint, right erase");
        ui_label_colored({ 140, 145, 165, 255 }, "shift+drag: box fill");
    } else {
        ui_label("prefabs");
        ui_input_text("filter", &g_ed_prefab_filter);
        for (const std::string& p : prefab_names()) {
            if (!g_ed_prefab_filter.empty() && p.find(g_ed_prefab_filter) == std::string::npos)
                continue;
            if (ui_selectable(p.c_str(), p == g_ed_prefab))
                g_ed_prefab = p;
        }
        ui_label_colored({ 140, 145, 165, 255 }, "click places, drag moves");
        ui_label_colored({ 140, 145, 165, 255 }, "alt: no snap  del  ctrl+d");
    }
    ui_panel_end();
}

static void scene_panel(float x)
{
    ui_panel_begin("Scene", { x, 8, 280, 230 });
    ui_label("%s", g_ed_path.c_str());
    int w = g_ed_scene->map.w, h = g_ed_scene->map.h;
    ui_drag_int("width", &w, 0.1f);
    ui_drag_int("height", &h, 0.1f);
    if (w != g_ed_scene->map.w || h != g_ed_scene->map.h) {
        w = std::max(4, std::min(w, 1000));
        h = std::max(4, std::min(h, 1000));
        g_ed_scene->map.resize(w, h);
        g_ed_scene->back.resize(w, h);
    }
    for (auto& kv : g_ed_scene->props.obj) {
        Json& v = kv.second;
        const char* k = kv.first.c_str();
        if (v.is_str()) {
            ui_input_text(k, &v.s);
        } else if (v.is_num()) {
            float f = (float)v.n;
            if (ui_drag_float(k, &f, 0.1f))
                v.n = f;
        } else if (v.type == Json::BOOL) {
            ui_checkbox(k, &v.b);
        }
    }
    ui_panel_end();
}

static void inspector(float x, float h)
{
    ui_panel_begin("Inspector", { x, 246, 280, h - 286 });
    World& w = g_ed_scene->world;
    if (!g_ed_selected || !w.alive(g_ed_selected)) {
        ui_label_colored({ 140, 145, 165, 255 }, "nothing selected");
        ui_panel_end();
        return;
    }
    Info* info = w.get<Info>(g_ed_selected);
    ui_label("%s  #%u", info ? info->prefab.c_str() : "?", entity_index(g_ed_selected));
    if (info)
        ui_input_text("name", &info->name);
    if (Transform* t = w.get<Transform>(g_ed_selected)) {
        ui_drag_float("x", &t->pos.x, 0.5f);
        ui_same_line();
        ui_drag_float("y", &t->pos.y, 0.5f);
        t->prev = t->pos;
    }
    if (ui_button("Duplicate")) editor_duplicate();
    ui_same_line();
    if (ui_button("Delete")) {
        w.destroy(g_ed_selected);
        w.flush();
        g_ed_selected = NO_ENTITY;
        ui_panel_end();
        return;
    }

    const ComponentType* remove = 0;
    for (const ComponentType& t : component_types()) {
        if (!t.has(w, g_ed_selected))
            continue;
        ui_push_id(t.name);
        if (ui_header(t.name)) {
            if (t.inspect)
                t.inspect(w, g_ed_selected);
            if (strcmp(t.name, "Transform") && ui_button("remove"))
                remove = &t;
        }
        ui_pop_id();
    }
    if (remove)
        remove->remove(w, g_ed_selected);

    if (ui_header("add component", false)) {
        for (const ComponentType& t : component_types()) {
            if (t.has(w, g_ed_selected))
                continue;
            if (ui_selectable(t.name, false))
                t.read(w, g_ed_selected, Json::object());
        }
    }
    ui_panel_end();
}

void editor_draw()
{
    if (!g_ed_open)
        return;
    int ww, wh;
    platform_window_size(&ww, &wh);

    Rect view = { g_ed_cam.x, g_ed_cam.y, ww / g_ed_zoom, wh / g_ed_zoom };
    render_set_view(g_ed_cam, g_ed_zoom);
    if (g_ed_hooks.draw_world)
        g_ed_hooks.draw_world(*g_ed_scene, view);
    editor_draw_overlays(view);
    render_set_view({ 0, 0 }, 1);

    ui_begin(g_ed_in);
    editor_draw_labels();
    left_panel((float)wh);
    scene_panel(ww - 288.f);
    inspector(ww - 288.f, (float)wh);

    vec2 m = editor_screen_to_world(g_ed_in.mouse_px, g_ed_in.mouse_py);
    int ts = g_ed_hooks.tileset ? g_ed_hooks.tileset->size : 16;
    char status[256];
    const char* tool_name = g_ed_tool == 0 ? "tiles" : g_ed_tool == 1 ? "back tiles" : "entities";
    snprintf(status, sizeof(status),
             "%s   tile %d,%d   px %.0f,%.0f   zoom %.2g   entities %u   undo %d   |   F5 play   F1 close   mid-drag/space pan   wheel zoom   f fit",
             tool_name, (int)floorf(m.x / ts), (int)floorf(m.y / ts), m.x, m.y, g_ed_zoom,
             g_ed_scene->world.live, (int)g_ed_undo_stack.size());
    ui_text_at(12, wh - 28.f, status, { 200, 205, 220, 255 });
    if (platform_time() < g_ed_toast_until)
        ui_text_at(250, 14, g_ed_toast.c_str(), SELECT);
    if (!assets_log().empty())
        ui_text_at(250, 40, assets_log().c_str(), { 255, 120, 110, 255 });
    ui_end();

    // close the undo step once the gesture is over and something actually changed
    if (g_ed_gesture && !g_ed_in.mouse_down[0] && !g_ed_in.mouse_down[1] && !g_ed_in.mouse_down[2]) {
        g_ed_gesture = false;
        std::string now = editor_snapshot();
        if (now != g_ed_pending) {
            g_ed_undo_stack.push_back(g_ed_pending);
            if (g_ed_undo_stack.size() > 100)
                g_ed_undo_stack.erase(g_ed_undo_stack.begin());
            g_ed_redo_stack.clear();
        }
    }
}

