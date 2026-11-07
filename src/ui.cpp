#include "ui.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

Font g_ui_font;
const Input* g_ui_in;

static const Color C_PANEL = { 24, 26, 34, 235 };
static const Color C_TITLE = { 44, 48, 64, 255 };
static const Color C_WIDGET = { 42, 45, 58, 255 };
static const Color C_HOT = { 58, 63, 82, 255 };
static const Color C_ACTIVE = { 78, 96, 150, 255 };
static const Color C_SEL = { 64, 84, 140, 255 };
static const Color C_TEXT = { 222, 224, 232, 255 };

struct PanelState {
    float scroll = 0;
    float content_h = 0;
};

static std::map<uint32_t, PanelState> panels;
static std::map<uint32_t, bool> headers;

uint32_t g_ui_hot, g_ui_active, g_ui_focus;
static uint32_t hot_next;
static bool mouse_over_panel, mouse_over_next;
vec2 g_ui_drag_start;
float g_ui_drag_value;
bool g_ui_dragged;
std::string g_ui_edit_buf;

static std::vector<uint32_t> id_stack;

// current panel
static Rect prect;
static PanelState* pstate;
static float cur_x, cur_y, row_h, line_y;
static bool same_line;
static float same_line_x;
static bool in_panel;

static uint32_t fnv(const char* s, uint32_t h = 2166136261u)
{
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

uint32_t ui_make_id(const char* label)
{
    uint32_t seed = id_stack.empty() ? 2166136261u : id_stack.back();
    return fnv(label, seed);
}

std::string ui_visible(const char* label)
{
    const char* hash = strstr(label, "##");
    return hash ? std::string(label, hash) : std::string(label);
}

void ui_init()
{
    g_ui_font = font_create("Consolas", 15, true);
}

void ui_shutdown()
{
    font_free(&g_ui_font);
}

const Font& ui_font()
{
    return g_ui_font;
}

void ui_begin(const Input& input)
{
    g_ui_in = &input;
    g_ui_hot = hot_next;
    hot_next = 0;
    mouse_over_panel = mouse_over_next;
    mouse_over_next = false;
    id_stack.clear();
}

void ui_end()
{
    // a click on nothing drops text focus
    if (g_ui_in->mouse_pressed[MOUSE_LEFT] && !g_ui_hot)
        g_ui_focus = 0;
    // after the widgets, so the one being released still sees itself active
    if (!g_ui_in->mouse_down[MOUSE_LEFT])
        g_ui_active = 0;
}

bool ui_want_mouse()
{
    return mouse_over_panel || g_ui_active != 0;
}

bool ui_want_keyboard()
{
    return g_ui_focus != 0;
}

bool ui_mouse_in(Rect r)
{
    return contains(r, { g_ui_in->mouse_px, g_ui_in->mouse_py });
}

void ui_push_id(const char* s)
{
    id_stack.push_back(ui_make_id(s));
}

void ui_push_id(int n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "#%d", n);
    ui_push_id(buf);
}

void ui_pop_id()
{
    if (!id_stack.empty())
        id_stack.pop_back();
}

void ui_panel_begin(const char* title, Rect r)
{
    uint32_t id = ui_make_id(title);
    pstate = &panels[id];
    prect = r;
    in_panel = true;

    if (ui_mouse_in(r))
        mouse_over_next = true;

    float title_h = g_ui_font.height + 6.f;
    draw_rect(r, C_PANEL);
    draw_rect({ r.x, r.y, r.w, title_h }, C_TITLE);
    draw_text(g_ui_font, ui_visible(title).c_str(), r.x + 6, r.y + 3, C_TEXT);

    float view_h = r.h - title_h;
    if (ui_mouse_in(r) && g_ui_in->wheel != 0)
        pstate->scroll -= g_ui_in->wheel * 40;
    float max_scroll = fmaxf(0, pstate->content_h - view_h + 8);
    pstate->scroll = clampf(pstate->scroll, 0, max_scroll);

    cur_x = r.x + 6;
    cur_y = r.y + title_h + 4 - pstate->scroll;
    line_y = cur_y;
    row_h = g_ui_font.height + 6.f;
    same_line = false;
    ui_push_id(title);
}

void ui_panel_end()
{
    ui_pop_id();
    float title_h = g_ui_font.height + 6.f;
    pstate->content_h = (cur_y + pstate->scroll) - (prect.y + title_h + 4);
    // scrollbar
    float view_h = prect.h - title_h;
    if (pstate->content_h > view_h) {
        float frac = view_h / pstate->content_h;
        float bar_h = fmaxf(20, view_h * frac);
        float t = pstate->scroll / (pstate->content_h - view_h + 8);
        draw_rect({ prect.x + prect.w - 4, prect.y + title_h + t * (view_h - bar_h), 3, bar_h }, C_HOT);
    }
    in_panel = false;
}

Rect ui_panel_rect()
{
    return prect;
}

static Rect last_rect;

Rect ui_last_rect()
{
    return last_rect;
}

// hands out the rect for the next widget and moves the cursor
Rect ui_layout(float w)
{
    Rect r;
    if (same_line) {
        r = { same_line_x, line_y, w, row_h - 2 };
        same_line = false;
    } else {
        r = { cur_x, cur_y, w, row_h - 2 };
        line_y = cur_y;
        cur_y += row_h;
    }
    same_line_x = r.x + r.w + 6;
    last_rect = r;
    return r;
}

float ui_full_width()
{
    return prect.x + prect.w - 10 - (same_line ? same_line_x : cur_x);
}

// widgets scrolled out of the panel still take up space but don't draw or react
bool ui_visible_in_panel(Rect r)
{
    float title_h = g_ui_font.height + 6.f;
    return r.y >= prect.y + title_h - 1 && r.y + r.h <= prect.y + prect.h + 1;
}

bool ui_interact(uint32_t id, Rect r, bool* held = 0)
{
    bool over = ui_mouse_in(r) && ui_visible_in_panel(r) && (!in_panel || ui_mouse_in(prect));
    if (over)
        hot_next = id;
    bool clicked = false;
    if (over && g_ui_in->mouse_pressed[MOUSE_LEFT]) {
        g_ui_active = id;
        g_ui_drag_start = { g_ui_in->mouse_px, g_ui_in->mouse_py };
        g_ui_dragged = false;
    }
    if (g_ui_active == id && g_ui_in->mouse_released[MOUSE_LEFT] && over)
        clicked = true;
    if (held)
        *held = g_ui_active == id;
    return clicked;
}

void ui_same_line()
{
    same_line = true;
}

void ui_separator()
{
    Rect r = ui_layout(prect.w - 12);
    if (ui_visible_in_panel(r))
        draw_rect({ r.x, r.y + r.h / 2, r.w, 1 }, C_HOT);
}

static void vlabel(Color c, const char* fmt, va_list ap)
{
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    Rect r = ui_layout(text_width(g_ui_font, buf));
    if (ui_visible_in_panel(r))
        draw_text(g_ui_font, buf, r.x, r.y + 2, c);
}

void ui_label(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlabel(C_TEXT, fmt, ap);
    va_end(ap);
}

void ui_label_colored(Color c, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlabel(c, fmt, ap);
    va_end(ap);
}

bool ui_button(const char* label)
{
    uint32_t id = ui_make_id(label);
    std::string text = ui_visible(label);
    Rect r = ui_layout(text_width(g_ui_font, text.c_str()) + 14);
    bool held;
    bool clicked = ui_interact(id, r, &held);
    if (ui_visible_in_panel(r)) {
        draw_rect(r, held ? C_ACTIVE : g_ui_hot == id ? C_HOT : C_WIDGET);
        draw_text(g_ui_font, text.c_str(), r.x + 7, r.y + 2, C_TEXT);
    }
    return clicked;
}

bool ui_selectable(const char* label, bool selected)
{
    uint32_t id = ui_make_id(label);
    Rect r = ui_layout(ui_full_width());
    bool clicked = ui_interact(id, r);
    if (ui_visible_in_panel(r)) {
        if (selected || g_ui_hot == id)
            draw_rect(r, selected ? C_SEL : C_HOT);
        draw_text(g_ui_font, ui_visible(label).c_str(), r.x + 4, r.y + 2, C_TEXT);
    }
    return clicked;
}

bool ui_checkbox(const char* label, bool* v)
{
    uint32_t id = ui_make_id(label);
    std::string text = ui_visible(label);
    Rect r = ui_layout(row_h + 4 + text_width(g_ui_font, text.c_str()));
    bool clicked = ui_interact(id, r);
    if (clicked)
        *v = !*v;
    if (ui_visible_in_panel(r)) {
        Rect box = { r.x, r.y + 2, r.h - 4, r.h - 4 };
        draw_rect(box, g_ui_hot == id ? C_HOT : C_WIDGET);
        if (*v)
            draw_rect({ box.x + 3, box.y + 3, box.w - 6, box.h - 6 }, C_ACTIVE);
        draw_text(g_ui_font, text.c_str(), r.x + box.w + 6, r.y + 2, C_TEXT);
    }
    return clicked;
}

bool ui_header(const char* label, bool default_open)
{
    uint32_t id = ui_make_id(label);
    auto it = headers.find(id);
    if (it == headers.end())
        it = headers.insert({ id, default_open }).first;
    Rect r = ui_layout(ui_full_width());
    if (ui_interact(id, r))
        it->second = !it->second;
    if (ui_visible_in_panel(r)) {
        draw_rect(r, g_ui_hot == id ? C_HOT : C_TITLE);
        std::string text = (it->second ? "- " : "+ ") + ui_visible(label);
        draw_text(g_ui_font, text.c_str(), r.x + 4, r.y + 2, C_TEXT);
    }
    return it->second;
}
