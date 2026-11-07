// Title screen: atmospheric presentation, procedural embers, and touch/mouse menus.
#include <cmath>
#include <cstdio>
#include <cstring>
#include "title.h"

static const char* title_items[6];
static int title_ids[6];
static int title_count;

void title_build_menu()
{
    title_count = 0;
    auto add = [](const char* label, int id) {
        title_items[title_count] = label;
        title_ids[title_count++] = id;
    };
    bool saved = saved_level() >= 0;
    if (saved)
        add("continuer", ITEM_CONTINUE);
    add(saved ? "nouvelle partie" : "jouer", ITEM_PLAY);
    if (G.reached >= 0)
        add("mondes", ITEM_WORLDS);
    add("options", ITEM_OPTIONS);
    if (assets_dev())
        add("editeur", ITEM_EDITOR);
    add("quitter", ITEM_QUIT);

    if (G.menu >= title_count)
        G.menu = 0;
}

static Rect title_item_rect(int i)
{
    float cx = VIEW_W / 2.f;
    float start_y = (title_count >= 6) ? 82.f : ((title_count == 5) ? 84.f : 87.f);
    float spacing = (title_count >= 6) ? 12.0f : ((title_count == 5) ? 13.0f : 14.0f);
    float iy = start_y + i * spacing;
    float tw = text_width(G.font, title_items[i]);
    float bw = fmaxf(tw + 30.f, 96.f);
    return { floorf(cx - bw / 2.f), iy, bw, 13.f };
}

static bool is_point_inside(vec2 p, Rect r)
{
    return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
}

int title_update(const Input& in, float dt)
{
    (void)dt;
    if (title_count == 0)
        title_build_menu();

    // 1. Mouse hover & click
    vec2 mouse = { in.mouse_x, in.mouse_y };
    for (int i = 0; i < title_count; i++) {
        Rect r = title_item_rect(i);
        r.x -= 4.f;
        r.w += 8.f;
        if (!is_point_inside(mouse, r))
            continue;
        if (G.menu != i) {
            G.menu = i;
            audio_play(sfx("menu"), 0.35f);
        }
        if (in.mouse_pressed[MOUSE_LEFT])
            return title_ids[i];
    }

    // 2. Touch tap
    for (int t = 0; t < in.touch_count; t++) {
        if (in.touches[t].phase != TOUCH_DOWN)
            continue;
        vec2 tp = { in.touches[t].x, in.touches[t].y };
        for (int i = 0; i < title_count; i++) {
            Rect r = title_item_rect(i);
            r.x -= 6.f;
            r.w += 12.f;
            r.y -= 2.f;
            r.h += 4.f;
            if (!is_point_inside(tp, r))
                continue;
            G.menu = i;
            return title_ids[i];
        }
    }

    // 3. Keyboard and Gamepad D-pad
    bool up = in.pressed[KEY_UP] || action_pressed(in, ACT_UP) || in.pad_pressed[PAD_UP];
    bool down = in.pressed[KEY_DOWN] || action_pressed(in, ACT_DOWN) || in.pad_pressed[PAD_DOWN];
    if (up || down) {
        G.menu = (G.menu + (down ? 1 : title_count - 1)) % title_count;
        audio_play(sfx("menu"), 0.4f);
    }

    // 4. Confirm key
    if (input_confirm(in))
        return title_ids[G.menu];

    return -1;
}

static void draw_ambient_cinders(float t)
{
    render_set_blend(BLEND_ADD);
    const int count = 18;
    for (int i = 0; i < count; i++) {
        float seed_x = (float)((i * 47) % VIEW_W);
        float drift = sinf(t * 1.4f + i * 1.8f) * 10.f;
        float x = fmodf(seed_x + drift + VIEW_W, (float)VIEW_W);
        float spd = 12.f + (float)((i * 7) % 20);
        float y = fmodf(190.f - (t * spd + i * 14.f), 200.f) - 10.f;
        if (y < -4.f || y > 184.f)
            continue;
        float fade_a = sinf(clampf(y / (float)VIEW_H, 0.05f, 0.95f) * 3.14159f);
        uint8_t a = (uint8_t)(fade_a * 190.f);
        draw_light({ x, y }, 4.f + (i % 3), { 255, 150, 50, (uint8_t)(a * 0.5f) });
        draw_rect({ floorf(x), floorf(y), 1.f, 1.f }, { 255, 230, 150, a });
    }
    render_set_blend(BLEND_ALPHA);
}

static void draw_mascot(float t)
{
    float bob = sinf(t * 2.8f) * 1.6f;
    vec2 p = { VIEW_W / 2.f, 68.f + bob };

    render_set_blend(BLEND_ADD);
    draw_light(p - vec2{ 0, 14 }, 46.f + sinf(t * 6.f) * 3.f, { 255, 130, 50, 130 });
    draw_light(p - vec2{ 0, 14 }, 14.f, { 255, 220, 150, 190 });
    render_set_blend(BLEND_ALPHA);

    int id = sprite_id("player");
    if (const SpriteSheet* s = sprite_get(id)) {
        int a = s->anim("idle");
        int frame = a >= 0 ? s->anims[a].frames[(int)(t * s->anims[a].fps) % s->anims[a].frames.size()] : 0;
        SpriteOpts o;
        o.origin = s->origin;
        o.scale = { 2, 2 };
        draw_spr_ex(id, frame, p, o);
    }

    render_set_blend(BLEND_ADD);
    for (int k = 0; k < 2; k++) {
        float sy = fmodf(t * 18.f + k * 8.f, 14.f);
        float sx = sinf(t * 8.f + k * 3.f) * 2.5f;
        draw_rect({ floorf(p.x + sx), floorf(p.y - 28.f - sy), 1.f, 1.f }, { 255, 240, 180, (uint8_t)(200 - sy * 13) });
    }
    render_set_blend(BLEND_ALPHA);
}

static void draw_context_chip(float t, float y)
{
    (void)t;
    int id = title_ids[G.menu];
    char text[128] = "";
    Color c = rgb(0xb0a4c8);

    if (id == ITEM_CONTINUE) {
        char sbuf[96] = "";
        if (save_summary(sbuf, sizeof(sbuf))) {
            snprintf(text, sizeof(text), "reprise : %s", sbuf);
            c = rgb(0xffd166);
        } else {
            snprintf(text, sizeof(text), "reprendre l'aventure");
        }
    } else if (id == ITEM_PLAY) {
        snprintf(text, sizeof(text), "allumer la flamme dans l'abime");
    } else if (id == ITEM_WORLDS) {
        snprintf(text, sizeof(text), "explorer les cavernes decouvertes");
    } else if (id == ITEM_OPTIONS) {
        snprintf(text, sizeof(text), "audio, plein ecran et commandes");
    } else if (id == ITEM_EDITOR) {
        snprintf(text, sizeof(text), "editeur de niveaux integre (F1)");
    } else if (id == ITEM_QUIT) {
        snprintf(text, sizeof(text), "eteindre la lueur et quitter");
    }

    if (text[0]) {
        float tw = text_width(G.font, text);
        float pw = tw + 16.f;
        float px = floorf((VIEW_W - pw) / 2.f);
        draw_rect({ px, y, pw, 13.f }, { 12, 10, 18, 190 });
        draw_rect_lines({ px, y, pw, 13.f }, 1, { 42, 34, 56, 140 });
        text_center(G.font, text, y + 1.f, c);
    }
}

void draw_title()
{
    float t = (float)engine_time();
    Rect view = { t * 20.f, 0, (float)VIEW_W, (float)VIEW_H };
    render_set_view({ 0, 0 });
    draw_background(view);
    parallax_draw(G.parallax, true, { view.x, 0 }, { (float)VIEW_W, (float)VIEW_H }, VIEW_H, t);

    draw_ambient_cinders(t);

    // Header glow & Title
    render_set_blend(BLEND_ADD);
    draw_light({ VIEW_W / 2.f, 16.f }, 68.f + sinf(t * 3.f) * 4.f, { 255, 130, 45, 95 });
    render_set_blend(BLEND_ALPHA);

    text_center(G.big, "LUEUR", 6.f, rgb(0xffb070));
    text_center(G.font, "une petite flamme dans un grand noir", 28.f, rgb(0x9a8fb0));

    draw_mascot(t);

    // Menu options
    float start_y = (title_count >= 6) ? 82.f : ((title_count == 5) ? 84.f : 87.f);
    float spacing = (title_count >= 6) ? 12.0f : ((title_count == 5) ? 13.0f : 14.0f);

    for (int i = 0; i < title_count; i++) {
        Rect r = title_item_rect(i);
        bool sel = (G.menu == i);

        if (sel) {
            draw_rect(r, { 28, 18, 38, 225 });
            draw_rect_lines(r, 1, rgb(0xffb070, 210));

            render_set_blend(BLEND_ADD);
            draw_light({ VIEW_W / 2.f, r.y + 6.f }, 30.f, { 255, 160, 60, 45 });
            render_set_blend(BLEND_ALPHA);

            float pip_pulse = sinf(t * 7.f) * 1.5f;
            draw_rect({ r.x - 7.f - pip_pulse, r.y + 5.f, 3.f, 3.f }, rgb(0xffd166));
            draw_rect({ r.x + r.w + 4.f + pip_pulse, r.y + 5.f, 3.f, 3.f }, rgb(0xffd166));

            text_center(G.font, title_items[i], r.y + 1.f, rgb(0xfff4c2));
        } else {
            draw_rect(r, { 14, 10, 20, 110 });
            draw_rect_lines(r, 1, { 38, 30, 50, 100 });
            text_center(G.font, title_items[i], r.y + 1.f, rgb(0x8a7fa0));
        }
    }

    // Context description chip
    float chip_y = start_y + title_count * spacing + 3.f;
    draw_context_chip(t, chip_y);

    // Footer bar
    text_center(G.font, action_text("{haut}/{bas} nav   {saut} valider").c_str(),
                VIEW_H - 11.f, rgb(0x605678));
    text_shadow(G.font, "v1.2", 8.f, VIEW_H - 11.f, rgb(0x423a54));
}

