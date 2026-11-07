// HUD, banners, messages and in-game indicators.
#include <cmath>
#include <cstdio>
#include "hud.h"
#include "game.h"
#include "touch.h"

static void ember_icon(float x, float y)
{
    int id = sprite_id("ember");
    const SpriteSheet* s = sprite_get(id);
    if (!s || s->frames.empty())
        return;
    int frame = (int)(engine_time() * 8) % (int)s->frames.size();
    SpriteOpts o;
    draw_spr_ex(id, frame, { x, y }, o);
}

static void hud_icon(const char* sprite, vec2 pos, float angle, bool lit)
{
    int id = sprite_id(sprite);
    const SpriteSheet* s = sprite_get(id);
    if (!s)
        return;
    SpriteOpts o;
    o.origin = s->origin;
    o.angle = angle;
    o.tint = lit ? WHITE : fade(WHITE, 0.35f);
    draw_spr_ex(id, 0, pos, o);
}

static void draw_weapon_hud()
{
    float x = VIEW_W - 6.f, y = VIEW_H - 8.f;
    if (touch_is_enabled()) {
        char t_buf[16];
        int s = (int)G.level_time;
        snprintf(t_buf, sizeof(t_buf), "%d:%02d", s / 60, s % 60);
        x = VIEW_W - 32.f - text_width(G.font, t_buf) - 12.f;
        y = 14.f;
    }
    if (G.has[WEAPON_BOW]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", G.quiver);
        x -= text_width(G.font, buf);
        text_shadow(G.font, buf, x, y - 7, G.quiver > 0 ? rgb(0xd8d0e8) : rgb(0xb8323a));
        hud_icon("arrow", { x - 8, y - 1 }, -0.785f, true);
        hud_icon("bow", { x - 22, y - 1 }, 0, G.weapon == WEAPON_BOW);
        x -= 32;
    }
    if (G.has[WEAPON_SPEAR]) {
        hud_icon("spear", { x - 14, y + 5 }, -0.785f, G.weapon == WEAPON_SPEAR);
        x -= 20;
    }
    if (has_power("eclat"))
        hud_icon("sun_icon", { x - 8, y - 2 }, 0, G.eclat_cool <= 0);
}

static void draw_boss_bar()
{
    if (G.boss_name.empty() || G.level_time <= 3.6f)
        return;
    float bw = 140, bx = floorf((VIEW_W - bw) / 2), by = VIEW_H - 9.f;
    text_center(G.font, G.boss_name.c_str(), VIEW_H - 22.f, rgb(0xb8e0ff));
    draw_rect({ bx - 1, by - 1, bw + 2, 7 }, { 10, 8, 16, 220 });
    draw_rect({ bx, by, floorf(bw * G.boss_hp), 5 }, mix(rgb(0xff6040), rgb(0x9ad0ff), G.boss_hp));
}

static void draw_level_banner()
{
    if (G.banner.empty())
        return;
    float t = G.level_time;
    float x;
    if (t < 0.3f) x = -200;
    else if (t < 0.9f) x = lerp(-200, 10, ease(EASE_OUT_BACK, (t - 0.3f) / 0.6f));
    else if (t < 3.2f) x = 10;
    else x = lerp(10, -200, ease(EASE_IN_QUAD, (t - 3.2f) / 0.5f));

    if (x <= -199)
        return;
    float w = text_width(G.font, G.banner.c_str());
    draw_rect({ x - 4, VIEW_H - 26.f, w + 12, 16 }, { 10, 8, 16, 200 });
    draw_rect({ x - 4, VIEW_H - 26.f, 2, 16 }, rgb(0xff9a3c));
    text_shadow(G.font, G.banner.c_str(), x + 2, VIEW_H - 24.f, rgb(0xfff4c2));
}

static void draw_popup_message()
{
    if (G.message_time <= 0 || G.message.empty())
        return;
    std::vector<std::string> rows = wrap_text(G.font, G.message, VIEW_W - 40.f);
    float w = 0;
    for (const std::string& r : rows)
        w = fmaxf(w, text_width(G.font, r.c_str()));
    float mx = floorf((VIEW_W - w) / 2);
    draw_rect({ mx - 6, 26, w + 12, rows.size() * 13.f + 5 }, { 10, 8, 16, 190 });
    for (size_t i = 0; i < rows.size(); i++) {
        float rx = floorf((VIEW_W - text_width(G.font, rows[i].c_str())) / 2);
        text_shadow(G.font, rows[i].c_str(), rx, 29 + i * 13.f, rgb(0xfff4c2));
    }
}

void draw_hud()
{
    char buf[64];
    if (G.embers_total > 0) {
        ember_icon(6, 5);
        snprintf(buf, sizeof(buf), "%d/%d", G.embers, G.embers_total);
        text_shadow(G.font, buf, 17, 3, rgb(0xffd166));
    }

    if (current_world().cold > 0) {
        Rect bar = { 6, 17, 44, 4 };
        draw_rect({ bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2 }, { 10, 8, 16, 200 });
        Color c = mix(rgb(0x5fa8e8), rgb(0xff9a3c), G.heat);
        if (G.heat < 0.3f && fmodf((float)engine_time() * 6, 1) < 0.5f)
            c = rgb(0xffffff);
        draw_rect({ bar.x, bar.y, floorf(bar.w * G.heat), bar.h }, c);
    }

    float right_margin = touch_is_enabled() ? 32.f : 6.f;
    int secs = (int)G.level_time;
    snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
    float tw = text_width(G.font, buf);
    text_shadow(G.font, buf, VIEW_W - right_margin - tw, 3, rgb(0xd8d0e8));
    if (G.deaths > 0) {
        snprintf(buf, sizeof(buf), "x%d", G.deaths);
        text_shadow(G.font, buf, VIEW_W - right_margin - text_width(G.font, buf), 15, rgb(0xb8323a));
    }

    draw_weapon_hud();
    draw_boss_bar();
    draw_level_banner();
    draw_popup_message();
}
