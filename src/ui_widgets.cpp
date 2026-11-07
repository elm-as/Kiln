#include "ui.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern Font g_ui_font;
extern const Input* g_ui_in;
extern uint32_t g_ui_hot, g_ui_active, g_ui_focus;
extern vec2 g_ui_drag_start;
extern float g_ui_drag_value;
extern bool g_ui_dragged;
extern std::string g_ui_edit_buf;

uint32_t ui_make_id(const char* label);
std::string ui_visible(const char* label);
bool ui_mouse_in(Rect r);
Rect ui_layout(float w);
float ui_full_width();
bool ui_visible_in_panel(Rect r);
bool ui_interact(uint32_t id, Rect r, bool* held = 0);

static const Color C_WIDGET = { 42, 45, 58, 255 };
static const Color C_HOT = { 58, 63, 82, 255 };
static const Color C_ACTIVE = { 78, 96, 150, 255 };
static const Color C_SEL = { 64, 84, 140, 255 };
static const Color C_TEXT = { 222, 224, 232, 255 };
static const Color C_DIM = { 140, 145, 165, 255 };

// Shared by the number and text fields. When focused, the field shows
// edit_buf and eats typing; returns true on commit.
static bool text_field(uint32_t id, Rect r, std::string& shown, bool* committed)
{
    *committed = false;
    bool held;
    ui_interact(id, r, &held);
    bool editing = g_ui_focus == id;

    if (editing) {
        for (int i = 0; i < g_ui_in->text_len; i++)
            g_ui_edit_buf += g_ui_in->text[i];
        // a whole character, not the last byte of an accent
        if (g_ui_in->pressed[KEY_BACKSPACE] && !g_ui_edit_buf.empty()) {
            while (!g_ui_edit_buf.empty() && ((unsigned char)g_ui_edit_buf.back() & 0xc0) == 0x80)
                g_ui_edit_buf.pop_back();
            if (!g_ui_edit_buf.empty())
                g_ui_edit_buf.pop_back();
        }
        if (g_ui_in->pressed[KEY_ENTER] || g_ui_in->pressed[KEY_TAB]) {
            *committed = true;
            g_ui_focus = 0;
        } else if (g_ui_in->pressed[KEY_ESCAPE]) {
            g_ui_focus = 0;
        } else if (g_ui_in->mouse_pressed[MOUSE_LEFT] && !ui_mouse_in(r)) {
            *committed = true;
            g_ui_focus = 0;
        }
        shown = g_ui_edit_buf;
    }

    if (ui_visible_in_panel(r)) {
        draw_rect(r, editing ? C_SEL : g_ui_hot == id ? C_HOT : C_WIDGET);
        draw_text(g_ui_font, shown.c_str(), r.x + 4, r.y + 2, C_TEXT);
        if (editing && fmod(platform_time(), 1.0) < 0.6) {
            float cx = r.x + 4 + text_width(g_ui_font, shown.c_str());
            draw_rect({ cx, r.y + 3, 1, r.h - 6 }, C_TEXT);
        }
    }
    return editing;
}

static bool number_field(const char* label, float* v, float speed, bool integer, float width)
{
    uint32_t id = ui_make_id(label);
    std::string text = ui_visible(label);
    float label_w = text.empty() ? 0 : text_width(g_ui_font, text.c_str()) + 6;
    Rect whole = ui_layout(width > 0 ? width : ui_full_width());
    Rect r = { whole.x + label_w, whole.y, whole.w - label_w, whole.h };
    if (ui_visible_in_panel(whole) && label_w > 0)
        draw_text(g_ui_font, text.c_str(), whole.x, whole.y + 2, C_DIM);

    bool changed = false;
    char buf[64];
    if (integer)
        snprintf(buf, sizeof(buf), "%d", (int)*v);
    else
        snprintf(buf, sizeof(buf), "%.2f", *v);
    std::string shown = buf;

    if (g_ui_focus != id) {
        bool held;
        ui_interact(id, r, &held);
        if (held) {
            float dx = g_ui_in->mouse_px - g_ui_drag_start.x;
            if (!g_ui_dragged && fabsf(dx) > 3) {
                g_ui_dragged = true;
                g_ui_drag_value = *v;
            }
            if (g_ui_dragged) {
                float nv = g_ui_drag_value + dx * speed;
                if (integer)
                    nv = floorf(nv + 0.5f);
                if (nv != *v) {
                    *v = nv;
                    changed = true;
                }
            }
        }
        // a click without a drag types the number in instead
        if (g_ui_active == id && g_ui_in->mouse_released[MOUSE_LEFT] && !g_ui_dragged) {
            g_ui_focus = id;
            g_ui_edit_buf = shown;
            g_ui_active = 0;
        }
        if (ui_visible_in_panel(r)) {
            draw_rect(r, held ? C_ACTIVE : g_ui_hot == id ? C_HOT : C_WIDGET);
            draw_text(g_ui_font, shown.c_str(), r.x + 4, r.y + 2, C_TEXT);
        }
        return changed;
    }

    bool committed;
    text_field(id, r, shown, &committed);
    if (committed) {
        char* end;
        double d = strtod(g_ui_edit_buf.c_str(), &end);
        if (end != g_ui_edit_buf.c_str()) {
            *v = integer ? floorf((float)d + 0.5f) : (float)d;
            changed = true;
        }
    }
    return changed;
}

bool ui_drag_float(const char* label, float* v, float speed)
{
    return number_field(label, v, speed, false, 0);
}

bool ui_drag_int(const char* label, int* v, float speed)
{
    float f = (float)*v;
    bool c = number_field(label, &f, speed, true, 0);
    *v = (int)f;
    return c;
}

bool ui_drag_vec2(const char* label, vec2* v, float speed)
{
    std::string text = ui_visible(label);
    ui_push_id(label);
    float total = ui_full_width();
    float label_w = text_width(g_ui_font, text.c_str()) + 6;
    Rect lr = ui_layout(label_w);
    if (ui_visible_in_panel(lr))
        draw_text(g_ui_font, text.c_str(), lr.x, lr.y + 2, C_DIM);
    float each = (total - label_w - 6) / 2;
    ui_same_line();
    bool a = number_field("##x", &v->x, speed, false, each);
    ui_same_line();
    bool b = number_field("##y", &v->y, speed, false, each);
    ui_pop_id();
    return a || b;
}

bool ui_input_text(const char* label, std::string* s)
{
    uint32_t id = ui_make_id(label);
    std::string text = ui_visible(label);
    float label_w = text.empty() ? 0 : text_width(g_ui_font, text.c_str()) + 6;
    Rect whole = ui_layout(ui_full_width());
    Rect r = { whole.x + label_w, whole.y, whole.w - label_w, whole.h };
    if (ui_visible_in_panel(whole) && label_w > 0)
        draw_text(g_ui_font, text.c_str(), whole.x, whole.y + 2, C_DIM);

    std::string shown = *s;
    if (g_ui_focus != id) {
        if (ui_interact(id, r)) {
            g_ui_focus = id;
            g_ui_edit_buf = *s;
        }
        if (ui_visible_in_panel(r)) {
            draw_rect(r, g_ui_hot == id ? C_HOT : C_WIDGET);
            draw_text(g_ui_font, shown.c_str(), r.x + 4, r.y + 2, C_TEXT);
        }
        return false;
    }
    bool committed;
    text_field(id, r, shown, &committed);
    if (committed && g_ui_edit_buf != *s) {
        *s = g_ui_edit_buf;
        return true;
    }
    return false;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ui_color(const char* label, Color* c)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", c->r, c->g, c->b, c->a);
    std::string s = buf;
    ui_push_id(label);
    bool changed = false;
    if (ui_input_text(label, &s)) {
        const char* p = s.c_str();
        if (*p == '#')
            p++;
        size_t n = strlen(p);
        if (n == 6 || n == 8) {
            uint8_t v[4] = { 0, 0, 0, 255 };
            bool ok = true;
            for (size_t i = 0; i < n / 2; i++) {
                int hi = hexval(p[i * 2]), lo = hexval(p[i * 2 + 1]);
                if (hi < 0 || lo < 0)
                    ok = false;
                else
                    v[i] = uint8_t(hi * 16 + lo);
            }
            if (ok) {
                *c = { v[0], v[1], v[2], v[3] };
                changed = true;
            }
        }
    }
    ui_pop_id();
    return changed;
}

void ui_text_at(float x, float y, const char* s, Color c)
{
    float w = text_width(g_ui_font, s);
    draw_rect({ x - 3, y - 1, w + 6, (float)g_ui_font.height + 2 }, { 0, 0, 0, 160 });
    draw_text(g_ui_font, s, x, y, c);
}

