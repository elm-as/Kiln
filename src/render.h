#pragma once
#include <cstdint>
#include "vec.h"

struct Color {
    uint8_t r, g, b, a;
};

inline Color rgb(uint32_t hex, uint8_t a = 255)
{
    return { uint8_t(hex >> 16), uint8_t(hex >> 8), uint8_t(hex), a };
}

inline Color fade(Color c, float amount)
{
    c.a = uint8_t(c.a * clampf(amount, 0, 1));
    return c;
}

inline Color mix(Color a, Color b, float t)
{
    t = clampf(t, 0, 1);
    return { uint8_t(a.r + (b.r - a.r) * t), uint8_t(a.g + (b.g - a.g) * t),
             uint8_t(a.b + (b.b - a.b) * t), uint8_t(a.a + (b.a - a.a) * t) };
}

const Color WHITE = { 255, 255, 255, 255 };
const Color BLACK = { 0, 0, 0, 255 };
const Color CLEAR = { 0, 0, 0, 0 };

struct Texture {
    unsigned id;
    int w, h;
};

// offscreen render target
struct Canvas {
    unsigned fbo;
    Texture tex;
};

enum BlendMode { BLEND_ALPHA, BLEND_ADD, BLEND_MULTIPLY, BLEND_NONE };

struct Glyph {
    short x, y, w;
};

// Glyphs cover ASCII and Latin-1 (enough for French), plus a few typographic
// extras like the curly apostrophe and the ellipsis parked in the unused
// 128-159 slots. Text is UTF-8.
struct Font {
    Texture tex;
    int height;
    Glyph glyphs[256];
};

int glyph_slot(uint32_t codepoint);     // -1 if the font has no such glyph
uint32_t glyph_codepoint(int slot);
uint32_t utf8_next(const char** s);     // 0 at the end of the string

// A frame goes: frame_begin, canvas pass (the fixed size game image, y down),
// optionally a screen pass in window pixels for tools, frame_end. If the game
// never switches to the screen pass, frame_end presents the canvas by itself.
void render_init(int canvas_w, int canvas_h);
void render_shutdown();
void render_frame_begin();
void render_frame_end();
void render_canvas_begin(Color clear);
void render_screen_begin();
void render_present_canvas();

void render_target_push(Canvas* c, Color clear);
void render_target_pop();

// pos is the world point at the top left of the target, scale is pixels per unit
void render_set_view(vec2 pos, float scale = 1);
void render_set_blend(BlendMode m);
void render_flush();

vec2 render_target_size();
vec2 render_canvas_size();
Rect render_canvas_rect();                  // where the canvas sits in the window
vec2 render_screen_to_canvas(float px, float py);
bool render_screenshot(const char* path);

Texture texture_create(int w, int h, const uint8_t* rgba, bool smooth = false);
Texture texture_load(const char* path);
void texture_free(Texture* t);

Canvas canvas_create(int w, int h, bool smooth = false);
void canvas_free(Canvas* c);

struct SpriteOpts {
    vec2 origin;                // pivot in source pixels
    vec2 scale = { 1, 1 };
    float angle = 0;
    Color tint = WHITE;
    bool flip_x = false;
    bool flip_y = false;
    float flash = 0;            // 0..1 towards solid white
};

void draw_rect(Rect r, Color c);
void draw_rect_lines(Rect r, float thickness, Color c);
void draw_line(vec2 a, vec2 b, float thickness, Color c);
void draw_circle(vec2 center, float radius, Color c);
void draw_circle_lines(vec2 center, float radius, float thickness, Color c);
void draw_light(vec2 center, float radius, Color c);    // soft falloff, meant for BLEND_ADD
// src in texels, pass {} for the whole texture
void draw_sprite(Texture t, Rect src, Rect dst, Color tint = WHITE);
void draw_sprite_ex(Texture t, Rect src, vec2 pos, const SpriteOpts& o);
void draw_canvas(const Canvas& c, Rect dst, Color tint = WHITE);

Font font_create(const char* face, int px, bool smooth = false);
void font_free(Font* f);
float text_width(const Font& f, const char* s);
void draw_text(const Font& f, const char* s, float x, float y, Color c = WHITE);
