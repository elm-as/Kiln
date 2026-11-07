#include <algorithm>
#include <cmath>
#include <cstdio>
#include "game.h"
#include "relics.h"

static float world_height()
{
    return (float)G.scene.map.h * G.tileset.size;
}

void draw_background(Rect view)
{
    Color top = json_color(G.scene.props["sky_top"], rgb(0x0d0b16));
    Color bottom = json_color(G.scene.props["sky_bottom"], rgb(0x1e1830));
    const int bands = 12;
    for (int i = 0; i < bands; i++) {
        float y = floorf(i * VIEW_H / (float)bands);
        float y2 = floorf((i + 1) * VIEW_H / (float)bands);
        draw_rect({ 0, y, (float)VIEW_W, y2 - y }, mix(top, bottom, i / (bands - 1.f)));
    }
    parallax_draw(G.parallax, false, { view.x, view.y }, { (float)VIEW_W, (float)VIEW_H },
                  fmaxf(world_height(), (float)VIEW_H), engine_time());
}

void draw_level(Scene& s, Rect view, float alpha)
{
    World& w = s.world;

    Color back_tint = json_color(s.props["back_tint"], rgb(0x3e3654));
    tilemap_draw(s.back, G.tileset, view, back_tint, true, engine_time());
    tilemap_draw(s.map, G.tileset, view, WHITE, false, engine_time());

    draw_sprites(w, view, alpha);

    particles_draw(false);
}

static void draw_bow(const Player& p, vec2 pos)
{
    int q = sprite_id("quiver");
    if (const SpriteSheet* sh = sprite_get(q)) {
        SpriteOpts o;
        o.origin = sh->origin;
        o.flip_x = p.facing < 0;
        o.angle = p.facing * -0.35f;
        draw_spr_ex(q, 0, pos + vec2{ -p.facing * 7.f, -6 }, o);
    }
    int b = sprite_id("bow");
    if (const SpriteSheet* sh = sprite_get(b)) {
        SpriteOpts o;
        o.origin = sh->origin;
        o.flip_x = p.facing < 0;
        if (p.aiming || p.bow_cool > 0)
            o.angle = p.facing * p.aim;
        // a little kick back right after a shot
        float kick = p.bow_cool > 0.2f ? 1.f : 0.f;
        draw_spr_ex(b, 0, pos + vec2{ p.facing * (6 - kick), -6 }, o);
    }
}

// The weapon in her hand. The spear rests upright like a staff, shoots out
// fast on a thrust and comes back slower. The bow comes with its quiver.
static void draw_weapon(float alpha)
{
    if (!player_alive())
        return;
    World& w = G.scene.world;
    Player& p = *w.get<Player>(G.player);
    Transform& t = *w.get<Transform>(G.player);
    vec2 pos = { lerp(t.prev.x, t.pos.x, alpha), lerp(t.prev.y, t.pos.y, alpha) };
    pos = { floorf(pos.x + 0.5f), floorf(pos.y + 0.5f) };

    if (G.weapon == WEAPON_BOW && G.has[WEAPON_BOW]) {
        draw_bow(p, pos);
        return;
    }
    if (!G.has[WEAPON_SPEAR])
        return;
    int id = sprite_id("spear");
    const SpriteSheet* sh = sprite_get(id);
    if (!sh)
        return;
    SpriteOpts o;
    o.origin = sh->origin;
    if (p.attack <= 0) {
        o.angle = -1.5707963f + p.facing * 0.2f;
        draw_spr_ex(id, 0, pos + vec2{ p.facing * 5.f, 1 }, o);
        return;
    }

    float k = 1 - p.attack / SPEAR_TIME;
    float ext = k < 0.3f ? k / 0.3f : 1 - (k - 0.3f) / 0.7f * 0.6f;
    if (p.attack_down) {
        o.angle = 1.5707963f;
        draw_spr_ex(id, 0, pos + vec2{ 0, floorf(ext * 8) - 6 }, o);
    } else if (p.attack_up) {
        o.angle = -1.5707963f;
        draw_spr_ex(id, 0, pos + vec2{ 0, -8 - floorf(ext * 8) }, o);
    } else {
        o.flip_x = p.facing < 0;
        draw_spr_ex(id, 0, pos + vec2{ p.facing * floorf(ext * 8 - 4), -6 }, o);
    }
}

// where the arrow will go, dotted, stopping at the first wall
static void draw_aim(float alpha)
{
    if (!player_alive())
        return;
    World& w = G.scene.world;
    Player& p = *w.get<Player>(G.player);
    if (!p.aiming)
        return;
    Transform& t = *w.get<Transform>(G.player);
    vec2 pos = { lerp(t.prev.x, t.pos.x, alpha), lerp(t.prev.y, t.pos.y, alpha) };
    vec2 at = pos + vec2{ p.facing * 5.f, -7 };
    vec2 v = vec2{ p.facing * cosf(p.aim), sinf(p.aim) } * ARROW_SPEED;
    for (int i = 1; i <= 16; i++) {
        float s = i * 0.035f;
        vec2 d = at + v * s + vec2{ 0, 0.5f * ARROW_GRAVITY * s * s };
        if (rect_hits_solid(G.scene.map, G.tileset, { d.x - 1, d.y - 1, 2, 2 }))
            break;
        draw_rect({ floorf(d.x), floorf(d.y), 1, 1 }, fade(rgb(0xffd166), 1 - i / 18.f));
    }
}

static void draw_arrows(float alpha)
{
    int id = sprite_id("arrow");
    const SpriteSheet* sh = sprite_get(id);
    if (!sh)
        return;
    for (const Arrow& a : G.arrows) {
        SpriteOpts o;
        o.origin = sh->origin;
        o.angle = a.angle;
        if (a.stuck && a.life < 0.4f)
            o.tint = fade(WHITE, a.life / 0.4f);
        vec2 pos = { lerp(a.prev.x, a.pos.x, alpha), lerp(a.prev.y, a.pos.y, alpha) };
        draw_spr_ex(id, 0, { floorf(pos.x + 0.5f), floorf(pos.y + 0.5f) }, o);
    }
}

static void draw_lights(vec2 cam)
{
    Color ambient = json_color(G.scene.props["ambient"], rgb(0x3a3350));
    render_target_push(&G.light, ambient);
    render_set_view(cam);
    render_set_blend(BLEND_ADD);
    double t = engine_time();
    G.scene.world.each<Light, Transform>([&](Entity e, Light& l, Transform& tr) {
        if (l.radius <= 0)
            return;
        float flick = 1;
        if (l.flicker > 0) {
            // two slow sines and a bit of noise, per entity so they don't pulse in sync
            float k = (float)sin(t * 9.1 + e * 1.7) * 0.5f + (float)sin(t * 23.3 + e * 0.3) * 0.3f + randf(-0.2f, 0.2f);
            flick = 1 + k * l.flicker;
        }
        vec2 p = tr.pos + l.offset;
        draw_light(p, l.radius * flick, l.color);
        // a small hot core makes the source itself read as bright
        draw_light(p, l.radius * 0.25f * flick, fade(l.color, 0.6f));
    });

    if (G.eclat_glow > 0) {
        float k = G.eclat_glow;
        draw_light(G.eclat_pos, ECLAT_RADIUS * 1.3f * fminf(1, k * 1.5f), fade(rgb(0xfff0c0), k));
        draw_light(G.eclat_pos, ECLAT_RADIUS * 0.5f, fade(rgb(0xffffff), k * k));
    }

    // her arrows carry a bit of her fire
    bool thermal_arrow = has_power("feu") || relic_is_equipped("souffle_chaud");
    for (const Arrow& a : G.arrows)
        draw_light(a.pos, (a.stuck ? 10 * a.life : 16) * (thermal_arrow ? 1.8f : 1), rgb(0xff9a3c));

    // tiles that give off light, lava mostly. Only the top of a pool glows,
    // one light per tile underneath would just be a white blob
    int ts = G.tileset.size;
    int x0 = (int)floorf(cam.x / ts) - 2, x1 = (int)floorf((cam.x + VIEW_W) / ts) + 2;
    int y0 = (int)floorf(cam.y / ts) - 2, y1 = (int)floorf((cam.y + VIEW_H) / ts) + 2;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            char c = G.scene.map.get(x, y);
            const TileDef* d = G.tileset.get(c);
            if (!d || d->light <= 0 || G.scene.map.get(x, y - 1) == c)
                continue;
            float flick = 1 + (float)sin(t * 3.1 + x * 0.9) * 0.08f;
            draw_light({ x * (float)ts + ts / 2.f, y * (float)ts + ts / 2.f }, d->light * flick, d->light_color);
        }
    }
    render_target_pop();

    render_set_view({ 0, 0 });
    render_set_blend(BLEND_MULTIPLY);
    draw_canvas(G.light, { 0, 0, (float)VIEW_W, (float)VIEW_H });
    render_set_blend(BLEND_ALPHA);
}

void text_shadow(const Font& f, const char* s, float x, float y, Color c)
{
    draw_text(f, s, x + 1, y + 1, { 0, 0, 0, (uint8_t)(c.a * 0.8f) });
    draw_text(f, s, x, y, c);
}

void text_center(const Font& f, const char* s, float y, Color c)
{
    text_shadow(f, s, floorf((VIEW_W - text_width(f, s)) / 2), y, c);
}

void draw_game(float alpha)
{
    vec2 cam = camera_view(G.camera, alpha);
    Rect view = { cam.x, cam.y, (float)VIEW_W, (float)VIEW_H };

    render_set_view({ 0, 0 });
    draw_background(view);

    render_set_view(cam);
    draw_level(G.scene, view, alpha);
    draw_weapon(alpha);
    draw_arrows(alpha);

    if (G.scene.props["dark"].boolean_or(true))
        draw_lights(cam);

    render_set_view(cam);
    render_set_blend(BLEND_ADD);
    particles_draw(true);
    render_set_blend(BLEND_ALPHA);
    draw_aim(alpha);

    // after the lighting, so the closest layers stay black silhouettes
    render_set_view({ 0, 0 });
    parallax_draw(G.parallax, true, cam, { (float)VIEW_W, (float)VIEW_H }, world_height(), engine_time());
    draw_hud();
    if (G.fade > 0.001f)
        draw_rect({ 0, 0, (float)VIEW_W, (float)VIEW_H }, fade(BLACK, G.fade));
}

void text_center_at(const char* s, float y, Color c)
{
    text_center(G.font, s, y, c);
}

