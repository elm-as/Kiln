// Full-screen story cutscenes between worlds, loaded from data/story.json.

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "game.h"

struct Page {
    std::string text;
    std::string image, anim;
    float scale = 2;
    Color glow = CLEAR;
};

static std::vector<Page> pages;
static size_t page_index;
static float page_time;
static float page_shown;
static int page_blips;
static void (*cutscene_done)();

static const float CUTSCENE_CHARS_PER_SEC = 36.0f;

static bool skip_cutscene()
{
    static bool skip = getenv("LUEUR_SKIP_STORY") != 0;
    return skip;
}

static size_t utf8_count(const std::string& s)
{
    size_t n = 0;
    const char* p = s.c_str();
    while (utf8_next(&p))
        n++;
    return n;
}

static std::string utf8_prefix(const std::string& s, size_t n)
{
    const char* p = s.c_str();
    for (size_t i = 0; i < n && utf8_next(&p); i++)
        ;
    return std::string(s.c_str(), p);
}

void cutscene_start(const std::string& id, void (*done)())
{
    cutscene_done = done;
    pages.clear();
    const Json& c = story_data()["cutscenes"][id.c_str()];
    if (!skip_cutscene()) {
        for (const Json& p : c.arr) {
            Page pg;
            pg.text = p["text"].str();
            pg.image = p["image"].str();
            pg.anim = p["anim"].str();
            pg.scale = p["scale"].f(2);
            pg.glow = json_color(p["glow"], CLEAR);
            pages.push_back(pg);
        }
    }
    page_index = 0;
    page_time = 0;
    page_shown = 0;
    page_blips = 0;
    if (pages.empty()) {
        if (done)
            done();
        return;
    }
    G.mode = MODE_STORY;
}

static void cutscene_finish()
{
    pages.clear();
    if (cutscene_done)
        cutscene_done();
}

void cutscene_frame(const Input& in, float dt)
{
    if (page_index >= pages.size())
        return;
    page_time += dt;
    if (page_time > 0.6f) {
        size_t total = utf8_count(pages[page_index].text);
        float before = page_shown;
        page_shown = fminf(page_shown + dt * CUTSCENE_CHARS_PER_SEC, (float)total);
        if ((int)page_shown / 3 > page_blips && before < total) {
            page_blips = (int)page_shown / 3;
            audio_play(sfx("voice"), 0.18f, randf(0.55f, 0.65f));
        }
    }
    if (input_skip(in)) {
        cutscene_finish();
        return;
    }
    if (!(input_confirm(in) || input_attack(in)))
        return;
    size_t total = utf8_count(pages[page_index].text);
    if (page_shown < total) {
        page_shown = (float)total;
        page_time = fmaxf(page_time, 0.6f);
        return;
    }
    audio_play(sfx("menu"), 0.25f, 1.3f);
    page_index++;
    page_time = 0;
    page_shown = 0;
    page_blips = 0;
    if (page_index >= pages.size())
        cutscene_finish();
}

void cutscene_draw()
{
    render_set_view({ 0, 0 });
    draw_rect({ 0, 0, (float)VIEW_W, (float)VIEW_H }, rgb(0x08060d));
    if (page_index >= pages.size())
        return;
    const Page& p = pages[page_index];
    float in = clampf(page_time / 0.6f, 0, 1);

    vec2 center = { VIEW_W / 2.f, 78 };
    if (p.glow.a) {
        render_set_blend(BLEND_ADD);
        float breathe = 1 + sinf((float)engine_time() * 3) * 0.04f;
        draw_light(center, 70 * breathe * in, fade(p.glow, 0.55f));
        render_set_blend(BLEND_ALPHA);
    }
    int id = sprite_id(p.image.c_str());
    if (const SpriteSheet* s = sprite_get(id)) {
        int frame = 0;
        int a = s->anim(p.anim.c_str());
        if (a >= 0 && !s->anims[a].frames.empty()) {
            const Anim& an = s->anims[a];
            frame = an.frames[(size_t)(engine_time() * an.fps) % an.frames.size()];
        }
        SpriteOpts o;
        o.origin = { s->w / 2.f, s->h / 2.f };
        o.scale = { p.scale, p.scale };
        o.tint = fade(WHITE, in);
        vec2 at = { center.x, fminf(center.y, 112 - s->h * p.scale / 2) };
        draw_spr_ex(id, frame, at, o);
    }

    std::vector<std::string> rows = wrap_text(G.font, p.text, 280);
    float y = 118;
    size_t left = (size_t)page_shown;
    for (const std::string& row : rows) {
        size_t n = utf8_count(row);
        std::string part = left >= n ? row : utf8_prefix(row, left);
        left = left >= n ? left - n : 0;
        float w = text_width(G.font, row.c_str());
        draw_text(G.font, part.c_str(), floorf((VIEW_W - w) / 2), y, rgb(0xe8e0f0));
        y += 13;
    }

    if (page_shown >= utf8_count(p.text) && fmod(engine_time(), 1.0) < 0.6)
        draw_text(G.font, "»", VIEW_W - 16.f, VIEW_H - 16.f, rgb(0x8a7fa0));
}

