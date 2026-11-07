#include "editor.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "assets.h"
#include "editor_paint.h"
#include "fs.h"
#include "physics.h"
#include "platform.h"
#include "ui.h"

EditorHooks g_ed_hooks;
Scene* g_ed_scene;
std::string g_ed_path;
bool g_ed_open;
Input g_ed_in;

vec2 g_ed_cam;
float g_ed_zoom = 3;
int g_ed_tool = 0; // 0: TILES, 1: BACK, 2: ENTITIES
char g_ed_brush = '#';
std::string g_ed_prefab;
Entity g_ed_selected;

static bool painting;
static char paint_char;
static int last_tx, last_ty;
bool g_ed_dragging;
static vec2 drag_offset;
static bool panning;
static vec2 pan_mouse, pan_cam;

std::vector<std::string> g_ed_undo_stack, g_ed_redo_stack;
std::string g_ed_pending;
bool g_ed_gesture;

std::string g_ed_toast;
double g_ed_toast_until;
bool g_ed_show_colliders = true;
bool g_ed_show_grid = true;
bool g_ed_show_links = true;
bool g_ed_show_lighting = false;
bool g_ed_bucket_mode = false;
bool g_ed_box_active = false;
int g_ed_box_x0 = 0, g_ed_box_y0 = 0, g_ed_box_x1 = 0, g_ed_box_y1 = 0;
static char box_char = '#';
std::string g_ed_prefab_filter;

void editor_say(const char* fmt, const char* arg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, arg);
    g_ed_toast = buf;
    g_ed_toast_until = platform_time() + 2.5;
}

std::string editor_snapshot()
{
    return json_write(scene_save(*g_ed_scene), false);
}

static void restore(const std::string& snap)
{
    Json j;
    std::string err;
    if (json_parse(snap, &j, &err))
        scene_load(*g_ed_scene, j);
    g_ed_selected = NO_ENTITY;
    g_ed_dragging = false;
}

void editor_init(const EditorHooks& h) { g_ed_hooks = h; }

void editor_open(Scene* s, const std::string& save_path)
{
    g_ed_scene = s; g_ed_path = save_path; g_ed_open = true;
    g_ed_selected = NO_ENTITY;
    g_ed_undo_stack.clear(); g_ed_redo_stack.clear();
    g_ed_gesture = false;
    if (g_ed_prefab.empty()) {
        std::vector<std::string> names = prefab_names();
        if (!names.empty()) g_ed_prefab = names[0];
    }
}

void editor_close() { g_ed_open = false; painting = g_ed_dragging = panning = false; }
bool editor_is_open() { return g_ed_open; }
vec2 editor_screen_to_world(float x, float y) { return { g_ed_cam.x + x / g_ed_zoom, g_ed_cam.y + y / g_ed_zoom }; }

vec2 editor_camera_center()
{
    int w, h;
    platform_window_size(&w, &h);
    return editor_screen_to_world(w * 0.5f, h * 0.5f);
}

// zoom out (or in) until the whole map fits the window
void editor_fit()
{
    int w, h;
    platform_window_size(&w, &h);
    int ts = g_ed_hooks.tileset ? g_ed_hooks.tileset->size : 16;
    float mw = (float)g_ed_scene->map.w * ts, mh = (float)g_ed_scene->map.h * ts;
    if (mw <= 0 || mh <= 0) return;
    g_ed_zoom = clampf(fminf(w / mw, h / mh) * 0.95f, 0.5f, 8);
    g_ed_cam = { mw / 2 - w * 0.5f / g_ed_zoom, mh / 2 - h * 0.5f / g_ed_zoom };
}

void editor_focus(vec2 p)
{
    int w, h;
    platform_window_size(&w, &h);
    g_ed_cam = { p.x - w * 0.5f / g_ed_zoom, p.y - h * 0.5f / g_ed_zoom };
}

Rect editor_entity_bounds(Entity e)
{
    World& w = g_ed_scene->world;
    Transform* t = w.get<Transform>(e);
    if (!t) return { 0, 0, 0, 0 };
    if (Sprite* s = w.get<Sprite>(e)) {
        if (const SpriteSheet* sh = sprite_get(s->sprite)) {
            vec2 p = t->pos + s->offset - sh->origin;
            return { p.x, p.y, (float)sh->w, (float)sh->h };
        }
    }
    if (Collider* c = w.get<Collider>(e)) return collider_rect(*t, *c);
    return { t->pos.x - 4, t->pos.y - 4, 8, 8 };
}

Entity editor_pick(vec2 p)
{
    // topmost first: highest layer, then newest
    Entity best = NO_ENTITY;
    int best_layer = -(1 << 30);
    g_ed_scene->world.each<Info>([&](Entity e, Info&) {
        if (!contains(editor_entity_bounds(e), p)) return;
        Sprite* s = g_ed_scene->world.get<Sprite>(e);
        int layer = s ? s->layer : 0;
        if (layer >= best_layer) { best_layer = layer; best = e; }
    });
    return best;
}

static Tilemap& layer_map() { return g_ed_tool == 1 ? g_ed_scene->back : g_ed_scene->map; }

static void paint_line(int x0, int y0, int x1, int y1, char c)
{
    // bresenham, so a quick stroke doesn't leave gaps
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    Tilemap& m = layer_map();
    for (;;) {
        m.set(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void editor_save()
{
    std::string text = json_write(scene_save(*g_ed_scene));
    if (write_file(g_ed_path.c_str(), text.data(), text.size()))
        editor_say("saved %s", g_ed_path.c_str());
    else
        editor_say("could not write %s", g_ed_path.c_str());
}

void editor_undo()
{
    if (g_ed_undo_stack.empty())
        return;
    g_ed_redo_stack.push_back(editor_snapshot());
    restore(g_ed_undo_stack.back());
    g_ed_undo_stack.pop_back();
}

void editor_redo()
{
    if (g_ed_redo_stack.empty())
        return;
    g_ed_undo_stack.push_back(editor_snapshot());
    restore(g_ed_redo_stack.back());
    g_ed_redo_stack.pop_back();
}

void editor_duplicate()
{
    if (!g_ed_selected || !g_ed_scene->world.alive(g_ed_selected))
        return;
    World& w = g_ed_scene->world;
    Info* info = w.get<Info>(g_ed_selected);
    Json over = entity_overrides(w, g_ed_selected);
    vec2 p = w.get<Transform>(g_ed_selected)->pos + vec2{ 8, 0 };
    Entity e = prefab_spawn(w, info->prefab.c_str(), p, &over);
    g_ed_selected = e;
}

void editor_frame(const Input& input, float dt)
{
    g_ed_in = input;
    if (!g_ed_open)
        return;

    bool ctrl = g_ed_in.down[KEY_CTRL];
    bool typing = ui_want_keyboard();
    bool ui_mouse = ui_want_mouse();

    int any_press = g_ed_in.mouse_pressed[0] || g_ed_in.mouse_pressed[1] || g_ed_in.mouse_pressed[2] ||
                    (!typing && (g_ed_in.pressed[KEY_DELETE] || (ctrl && g_ed_in.pressed[KEY_D]))) ||
                    g_ed_in.pressed[KEY_ENTER];
    if (any_press && !g_ed_gesture) {
        g_ed_pending = editor_snapshot();
        g_ed_gesture = true;
    }

    if (!typing) {
        if (ctrl && g_ed_in.pressed[KEY_S]) editor_save();
        if (ctrl && g_ed_in.pressed[KEY_Z]) editor_undo();
        if (ctrl && g_ed_in.pressed[KEY_Y]) editor_redo();
        if (ctrl && g_ed_in.pressed[KEY_D]) editor_duplicate();
        if (!ctrl && g_ed_in.pressed[KEY_1]) g_ed_tool = 0;
        if (!ctrl && g_ed_in.pressed[KEY_2]) g_ed_tool = 1;
        if (!ctrl && g_ed_in.pressed[KEY_3]) g_ed_tool = 2;
        if (!ctrl && g_ed_in.pressed[KEY_G]) g_ed_show_grid = !g_ed_show_grid;
        if (!ctrl && g_ed_in.pressed[KEY_C]) g_ed_show_colliders = !g_ed_show_colliders;
        if (!ctrl && g_ed_in.pressed[KEY_L]) g_ed_show_links = !g_ed_show_links;
        if (!ctrl && g_ed_in.pressed[KEY_O]) {
            g_ed_show_lighting = !g_ed_show_lighting;
            editor_say(g_ed_show_lighting ? "lighting preview ON" : "lighting preview OFF", "");
        }
        if (!ctrl && g_ed_in.pressed[KEY_B]) {
            g_ed_bucket_mode = !g_ed_bucket_mode;
            editor_say(g_ed_bucket_mode ? "bucket fill ON" : "bucket fill OFF", "");
        }
        if (!ctrl && g_ed_in.pressed[KEY_F]) editor_fit();
        if (g_ed_in.pressed[KEY_DELETE] && g_ed_selected && g_ed_scene->world.alive(g_ed_selected)) {
            g_ed_scene->world.destroy(g_ed_selected);
            g_ed_scene->world.flush();
            g_ed_selected = NO_ENTITY;
        }

        float pan = 400 / g_ed_zoom * dt;
        if (!ctrl) {
            if (g_ed_in.down[KEY_A] || g_ed_in.down[KEY_LEFT]) g_ed_cam.x -= pan;
            if (g_ed_in.down[KEY_D] || g_ed_in.down[KEY_RIGHT]) g_ed_cam.x += pan;
            if (g_ed_in.down[KEY_W] || g_ed_in.down[KEY_UP]) g_ed_cam.y -= pan;
            if (g_ed_in.down[KEY_S] || g_ed_in.down[KEY_DOWN]) g_ed_cam.y += pan;
        }
    }

    vec2 mouse = editor_screen_to_world(g_ed_in.mouse_px, g_ed_in.mouse_py);
    if (!ui_mouse && g_ed_in.wheel != 0) {
        float nz = clampf(g_ed_zoom * powf(1.25f, g_ed_in.wheel), 0.5f, 8);
        g_ed_cam = mouse - vec2{ g_ed_in.mouse_px, g_ed_in.mouse_py } * (1 / nz);
        g_ed_zoom = nz;
    }

    // middle drag, or space + left drag, pans
    bool pan_btn = g_ed_in.mouse_pressed[MOUSE_MIDDLE] || (g_ed_in.down[KEY_SPACE] && g_ed_in.mouse_pressed[MOUSE_LEFT]);
    if (pan_btn && !ui_mouse) {
        panning = true;
        pan_mouse = { g_ed_in.mouse_px, g_ed_in.mouse_py };
        pan_cam = g_ed_cam;
    }
    if (panning) {
        g_ed_cam = pan_cam - (vec2{ g_ed_in.mouse_px, g_ed_in.mouse_py } - pan_mouse) * (1 / g_ed_zoom);
        if (!g_ed_in.mouse_down[MOUSE_MIDDLE] && !g_ed_in.mouse_down[MOUSE_LEFT])
            panning = false;
        return;
    }

    int ts = g_ed_hooks.tileset ? g_ed_hooks.tileset->size : 16;
    int tx = (int)floorf(mouse.x / ts), ty = (int)floorf(mouse.y / ts);

    if (g_ed_tool == 0 || g_ed_tool == 1) {
        if (!ui_mouse && g_ed_in.down[KEY_SHIFT]) {
            if (g_ed_in.mouse_pressed[MOUSE_LEFT] || g_ed_in.mouse_pressed[MOUSE_RIGHT]) {
                g_ed_box_active = true;
                g_ed_box_x0 = g_ed_box_x1 = tx;
                g_ed_box_y0 = g_ed_box_y1 = ty;
                box_char = g_ed_in.mouse_pressed[MOUSE_LEFT] ? g_ed_brush : '.';
            }
            if (g_ed_box_active) {
                g_ed_box_x1 = tx;
                g_ed_box_y1 = ty;
                if (!g_ed_in.mouse_down[MOUSE_LEFT] && !g_ed_in.mouse_down[MOUSE_RIGHT]) {
                    tile_box_fill(layer_map(), g_ed_box_x0, g_ed_box_y0, g_ed_box_x1, g_ed_box_y1, box_char);
                    g_ed_box_active = false;
                }
            }
        } else if (g_ed_box_active) {
            tile_box_fill(layer_map(), g_ed_box_x0, g_ed_box_y0, g_ed_box_x1, g_ed_box_y1, box_char);
            g_ed_box_active = false;
        } else if (g_ed_bucket_mode) {
            if (!ui_mouse && g_ed_in.mouse_pressed[MOUSE_LEFT]) {
                int count = tile_flood_fill(layer_map(), tx, ty, g_ed_brush);
                if (count > 0) editor_say("filled %s tiles", std::to_string(count).c_str());
            } else if (!ui_mouse && g_ed_in.mouse_pressed[MOUSE_RIGHT]) {
                int count = tile_flood_fill(layer_map(), tx, ty, '.');
                if (count > 0) editor_say("erased %s tiles", std::to_string(count).c_str());
            }
        } else {
            if (!ui_mouse && (g_ed_in.mouse_pressed[MOUSE_LEFT] || g_ed_in.mouse_pressed[MOUSE_RIGHT])) {
                painting = true;
                paint_char = g_ed_in.mouse_pressed[MOUSE_LEFT] ? g_ed_brush : '.';
                last_tx = tx;
                last_ty = ty;
            }
            if (painting) {
                paint_line(last_tx, last_ty, tx, ty, paint_char);
                last_tx = tx;
                last_ty = ty;
                if (!g_ed_in.mouse_down[MOUSE_LEFT] && !g_ed_in.mouse_down[MOUSE_RIGHT])
                    painting = false;
            }
        }
        // q picks up the tile under the cursor as the brush
        if (!ui_mouse && g_ed_in.pressed[KEY_Q]) {
            char c = layer_map().get(tx, ty);
            if (c != '.') g_ed_brush = c;
        }
    } else {
        // alt places and drags freely, otherwise snap to half tiles
        float snap = g_ed_in.down[KEY_ALT] ? 1.f : ts / 2.f;
        if (!ui_mouse && g_ed_in.mouse_pressed[MOUSE_LEFT]) {
            Entity hit = editor_pick(mouse);
            if (hit) {
                g_ed_selected = hit;
                g_ed_dragging = true;
                drag_offset = g_ed_scene->world.get<Transform>(hit)->pos - mouse;
            } else if (!g_ed_prefab.empty()) {
                vec2 p = { floorf(mouse.x / snap + 0.5f) * snap, floorf(mouse.y / snap + 0.5f) * snap };
                g_ed_selected = prefab_spawn(g_ed_scene->world, g_ed_prefab.c_str(), p);
            }
        }
        if (!ui_mouse && g_ed_in.mouse_pressed[MOUSE_RIGHT])
            g_ed_selected = editor_pick(mouse);
        if (g_ed_dragging) {
            if (Transform* t = g_ed_scene->world.get<Transform>(g_ed_selected)) {
                vec2 p = mouse + drag_offset;
                t->pos = { floorf(p.x / snap + 0.5f) * snap, floorf(p.y / snap + 0.5f) * snap };
                t->prev = t->pos;
            }
            if (!g_ed_in.mouse_down[MOUSE_LEFT])
                g_ed_dragging = false;
        }
    }
}
