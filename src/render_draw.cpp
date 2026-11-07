#include "gl.h"
#include <cstdio>
#include <cstdlib>
#include "render.h"
#include "image.h"

extern Texture g_render_white, g_render_circle, g_render_light;
void render_push_quad(GLuint tex, const vec2 p[4], float u0, float v0, float u1, float v1, Color c, float flash = 0);
uint32_t render_current_fbo();
uint32_t render_batch_tex();

Texture texture_create(int w, int h, const uint8_t* rgba, bool smooth)
{
    Texture t = { 0, w, h };
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    // whatever the batch had bound is still what flush will bind, nothing to restore
    return t;
}

Texture texture_load(const char* path)
{
    int w, h;
    uint8_t* px = image_load(path, &w, &h);
    if (!px)
        return g_render_white;
    Texture t = texture_create(w, h, px);
    free(px);
    return t;
}

void texture_free(Texture* t)
{
    if (!t->id || t->id == g_render_white.id)
        return;
    if (t->id == render_batch_tex())
        render_flush();
    glDeleteTextures(1, &t->id);
    t->id = 0;
}

Canvas canvas_create(int w, int h, bool smooth)
{
    Canvas c = {};
    c.tex = texture_create(w, h, 0, smooth);
    glGenFramebuffers(1, &c.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, c.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c.tex.id, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "canvas %dx%d: framebuffer incomplete\n", w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, render_current_fbo());
    return c;
}

void canvas_free(Canvas* c)
{
    if (c->fbo)
        glDeleteFramebuffers(1, &c->fbo);
    texture_free(&c->tex);
    c->fbo = 0;
}

void draw_rect(Rect r, Color c)
{
    vec2 p[4] = {
        { r.x, r.y }, { r.x + r.w, r.y }, { r.x + r.w, r.y + r.h }, { r.x, r.y + r.h }
    };
    render_push_quad(g_render_white.id, p, 0, 0, 1, 1, c);
}

void draw_rect_lines(Rect r, float t, Color c)
{
    draw_rect({ r.x, r.y, r.w, t }, c);
    draw_rect({ r.x, r.y + r.h - t, r.w, t }, c);
    draw_rect({ r.x, r.y + t, t, r.h - 2 * t }, c);
    draw_rect({ r.x + r.w - t, r.y + t, t, r.h - 2 * t }, c);
}

void draw_line(vec2 a, vec2 b, float t, Color c)
{
    vec2 d = b - a;
    float len = length(d);
    if (len <= 0)
        return;
    vec2 n = vec2{ -d.y, d.x } * (t * 0.5f / len);
    vec2 p[4] = { a + n, b + n, b - n, a - n };
    render_push_quad(g_render_white.id, p, 0, 0, 1, 1, c);
}

void draw_circle(vec2 c, float r, Color col)
{
    draw_sprite(g_render_circle, {}, { c.x - r, c.y - r, r * 2, r * 2 }, col);
}

void draw_circle_lines(vec2 c, float r, float t, Color col)
{
    int n = clampf(r * 0.8f, 12, 64);
    vec2 prev = { c.x + r, c.y };
    for (int i = 1; i <= n; i++) {
        float a = i * 6.2831853f / n;
        vec2 p = { c.x + cosf(a) * r, c.y + sinf(a) * r };
        draw_line(prev, p, t, col);
        prev = p;
    }
}

void draw_light(vec2 c, float r, Color col)
{
    draw_sprite(g_render_light, {}, { c.x - r, c.y - r, r * 2, r * 2 }, col);
}

void draw_sprite(Texture t, Rect src, Rect dst, Color tint)
{
    if (src.w == 0 && src.h == 0)
        src = { 0, 0, (float)t.w, (float)t.h };
    vec2 p[4] = {
        { dst.x, dst.y }, { dst.x + dst.w, dst.y }, { dst.x + dst.w, dst.y + dst.h }, { dst.x, dst.y + dst.h }
    };
    render_push_quad(t.id, p, src.x / t.w, src.y / t.h, (src.x + src.w) / t.w, (src.y + src.h) / t.h, tint);
}

void draw_sprite_ex(Texture t, Rect src, vec2 pos, const SpriteOpts& o)
{
    if (src.w == 0 && src.h == 0)
        src = { 0, 0, (float)t.w, (float)t.h };

    // corners relative to the pivot, before rotation
    float x0 = -o.origin.x * o.scale.x;
    float y0 = -o.origin.y * o.scale.y;
    float x1 = x0 + src.w * o.scale.x;
    float y1 = y0 + src.h * o.scale.y;
    if (o.flip_x) {
        // mirror around the pivot, not around the sprite center
        float a = -x1, b = -x0;
        x0 = a;
        x1 = b;
    }
    if (o.flip_y) {
        float a = -y1, b = -y0;
        y0 = a;
        y1 = b;
    }

    vec2 p[4] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
    for (vec2& v : p) {
        if (o.angle != 0)
            v = rotate(v, o.angle);
        v += pos;
    }

    float u0 = src.x / t.w, v0 = src.y / t.h;
    float u1 = (src.x + src.w) / t.w, v1 = (src.y + src.h) / t.h;
    if (o.flip_x) { float tmp = u0; u0 = u1; u1 = tmp; }
    if (o.flip_y) { float tmp = v0; v0 = v1; v1 = tmp; }
    render_push_quad(t.id, p, u0, v0, u1, v1, o.tint, o.flash);
}

void draw_canvas(const Canvas& c, Rect dst, Color tint)
{
    // framebuffer textures are bottom up
    vec2 p[4] = {
        { dst.x, dst.y }, { dst.x + dst.w, dst.y }, { dst.x + dst.w, dst.y + dst.h }, { dst.x, dst.y + dst.h }
    };
    render_push_quad(c.tex.id, p, 0, 1, 1, 0, tint);
}

static const uint32_t extra_glyphs[] = {
    0x2019, 0x2018, 0x201c, 0x201d, 0x2026, 0x2013, 0x2014, 0x0152, 0x0153, 0x20ac,
};

int glyph_slot(uint32_t cp)
{
    if ((cp >= 32 && cp < 127) || (cp >= 160 && cp < 256))
        return (int)cp;
    for (int i = 0; i < (int)(sizeof(extra_glyphs) / sizeof(extra_glyphs[0])); i++)
        if (extra_glyphs[i] == cp)
            return 128 + i;
    return -1;
}

uint32_t glyph_codepoint(int slot)
{
    if (slot >= 128 && slot < 128 + (int)(sizeof(extra_glyphs) / sizeof(extra_glyphs[0])))
        return extra_glyphs[slot - 128];
    return (uint32_t)slot;
}

// lenient: a broken byte comes back as itself instead of stopping everything
uint32_t utf8_next(const char** str)
{
    const unsigned char* s = (const unsigned char*)*str;
    if (!*s)
        return 0;
    uint32_t c = *s++;
    int extra = 0;
    if (c >= 0xf0 && c < 0xf8) { c &= 0x07; extra = 3; }
    else if (c >= 0xe0) { c &= 0x0f; extra = 2; }
    else if (c >= 0xc0) { c &= 0x1f; extra = 1; }
    for (int i = 0; i < extra; i++) {
        if ((*s & 0xc0) != 0x80)
            break;
        c = c << 6 | (*s++ & 0x3f);
    }
    *str = (const char*)s;
    return c;
}

static const Glyph& glyph_for(const Font& f, uint32_t cp)
{
    int slot = glyph_slot(cp);
    if (slot < 0 || f.glyphs[slot].w == 0)
        slot = '?';
    return f.glyphs[slot];
}

float text_width(const Font& f, const char* s)
{
    float w = 0, line = 0;
    while (uint32_t cp = utf8_next(&s)) {
        if (cp == '\n') {
            line = 0;
            continue;
        }
        line += glyph_for(f, cp).w;
        if (line > w)
            w = line;
    }
    return w;
}

void draw_text(const Font& f, const char* s, float x, float y, Color c)
{
    float cx = x;
    float fh = (float)f.height;
    while (uint32_t cp = utf8_next(&s)) {
        if (cp == '\n') {
            cx = x;
            y += fh;
            continue;
        }
        const Glyph& g = glyph_for(f, cp);
        if (cp != ' ')
            draw_sprite(f.tex, { (float)g.x, (float)g.y, (float)g.w, fh }, { cx, y, (float)g.w, fh }, c);
        cx += g.w;
    }
}

