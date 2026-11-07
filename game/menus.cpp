// Pause menu, level clear screen, ending screen, and editor playtest transitions.

#include <cmath>
#include <cstdio>
#include "game.h"
#include "relics.h"
#include "title.h"

const char* pause_items[] = { "reprendre", "recommencer le niveau", "options", "retour au titre" };

static Rect pause_item_rect(int i)
{
    float cx = VIEW_W / 2.f;
    float tw = text_width(G.font, pause_items[i]);
    float bw = fmaxf(tw + 28.f, 96.f);
    return { floorf(cx - bw / 2.f), 58.f + i * 14.f - 2.f, bw, 13.f };
}

static void menu_move(const Input& in, int count)
{
    bool up = in.pressed[KEY_UP] || action_pressed(in, ACT_UP) || in.pad_pressed[PAD_UP];
    bool down = in.pressed[KEY_DOWN] || action_pressed(in, ACT_DOWN) || in.pad_pressed[PAD_DOWN];
    if (up || down) {
        G.menu = (G.menu + (down ? 1 : count - 1)) % count;
        audio_play(sfx("menu"), 0.4f);
    }
}

static std::string scene_path()
{
    return "assets/scenes/" + G.scene.name + ".json";
}

void open_editor()
{
    Json j;
    if (asset_json("scenes/" + G.scene.name + ".json", &j))
        scene_load(G.scene, j);
    G.player = NO_ENTITY;
    particles_clear();
    music_stop(0.3f);
    editor_open(&G.scene, scene_path());
    G.mode = MODE_EDIT;
    G.playtest = false;
}

void start_playtest(bool from_view)
{
    G.playtest_snapshot = json_write(scene_save(G.scene), false);
    vec2 here = editor_camera_center();
    editor_close();
    G.playtest = true;
    weapons_for_level(1000);
    G.quiver = QUIVER_MAX;
    level_begin();
    if (from_view && player_alive()) {
        G.spawn = here;
        G.scene.world.get<Transform>(G.player)->pos = here;
        G.scene.world.get<Transform>(G.player)->prev = here;
    }
}

void stop_playtest()
{
    Json j;
    std::string err;
    if (json_parse(G.playtest_snapshot, &j, &err))
        scene_load(G.scene, j);
    G.player = NO_ENTITY;
    particles_clear();
    music_stop(0.2f);
    editor_open(&G.scene, scene_path());
    G.mode = MODE_EDIT;
    G.playtest = false;
}

void pause_frame(const Input& in)
{
    menu_move(in, 4);

    int clicked_item = -1;
    vec2 m = { in.mouse_x, in.mouse_y };
    for (int i = 0; i < 4; i++) {
        Rect r = pause_item_rect(i);
        r.x -= 4.f; r.w += 8.f; r.y -= 2.f; r.h += 4.f;
        if (m.x >= r.x && m.x <= r.x + r.w && m.y >= r.y && m.y <= r.y + r.h) {
            if (G.menu != i && (in.mouse_px != 0 || in.mouse_py != 0)) {
                G.menu = i;
                audio_play(sfx("menu"), 0.3f);
            }
            if (in.mouse_pressed[MOUSE_LEFT]) {
                clicked_item = i;
                break;
            }
        }
        for (int t = 0; t < in.touch_count; t++) {
            if (in.touches[t].phase == TOUCH_DOWN) {
                float tx = in.touches[t].x, ty = in.touches[t].y;
                if (tx >= r.x && tx <= r.x + r.w && ty >= r.y && ty <= r.y + r.h) {
                    G.menu = i;
                    clicked_item = i;
                    break;
                }
            }
        }
        if (clicked_item >= 0)
            break;
    }

    if (clicked_item >= 0 || input_confirm(in)) {
        int choice = clicked_item >= 0 ? clicked_item : G.menu;
        audio_play(sfx("select"), 0.5f);
        if (choice == 0) {
            G.mode = MODE_PLAY;
        } else if (choice == 1) {
            level_load(G.level);
            G.mode = MODE_PLAY;
        } else if (choice == 2) {
            options_open(MODE_PAUSE);
        } else {
            G.mode = MODE_TITLE;
            G.menu = 0;
            title_build_menu();
            music_play(music("title"), 0.5f, 1);
        }
        return;
    }
    if (input_back(in)) {
        G.mode = MODE_PLAY;
        return;
    }

    int toggle_slot = -1;
    for (int k = 0; k < 5; k++) {
        if (in.pressed[KEY_1 + k])
            toggle_slot = k;
    }
    if (in.mouse_pressed[MOUSE_LEFT] && m.y >= 130.f && m.y < 180.f)
        toggle_slot = (int)((m.y - 130.f) / 10.f);
    if (toggle_slot < 0)
        return;

    int shown = 0;
    for (int i = 0; i < relics_count(); i++) {
        if (!relic_state(i).unlocked)
            continue;
        if (shown == toggle_slot) {
            bool next_eq = !relic_state(i).equipped;
            if (relic_equip(relic_def(i).id.c_str(), next_eq)) {
                audio_play(sfx("select"), 0.4f);
                save_game();
            }
            break;
        }
        shown++;
    }
}

static void draw_menu_list(const char* const* items, int n, float y)
{
    float cx = VIEW_W / 2.f;
    for (int i = 0; i < n; i++) {
        bool sel = G.menu == i;
        float tw = text_width(G.font, items[i]);
        float bw = fmaxf(tw + 28.f, 96.f);
        Rect r = { floorf(cx - bw / 2.f), y + i * 14.f - 2.f, bw, 11.f };
        if (sel) {
            draw_rect(r, { 28, 18, 38, 220 });
            draw_rect_lines(r, 1, rgb(0xffb070, 190));
            text_center(G.font, items[i], y + i * 14.f, rgb(0xfff4c2));
        } else {
            draw_rect(r, { 14, 10, 20, 110 });
            text_center(G.font, items[i], y + i * 14.f, rgb(0x8a7fa0));
        }
    }
}

void draw_pause()
{
    draw_rect({ 0, 0, (float)VIEW_W, (float)VIEW_H }, { 8, 6, 14, 185 });
    text_center(G.big, "PAUSE", 24, rgb(0xfff4c2));
    draw_menu_list(pause_items, 4, 58);

    int used = relic_slots_used();
    char r_buf[64];
    snprintf(r_buf, sizeof(r_buf), "reliques  %d/%d  (touches 1-5 ou clic)", used, relic_slots_max());
    text_center(G.font, r_buf, 118, rgb(0xffd166));

    float ry = 130;
    int shown = 0;
    for (int i = 0; i < relics_count(); i++) {
        if (!relic_state(i).unlocked)
            continue;
        shown++;
        char line_buf[96];
        snprintf(line_buf, sizeof(line_buf), "[%c] %d. %s",
                 relic_state(i).equipped ? 'x' : ' ', shown, relic_def(i).name.c_str());
        Color c = relic_state(i).equipped ? rgb(0xfff4c2) : rgb(0x8a7fa0);
        text_center(G.font, line_buf, ry, c);
        ry += 10;
    }
}

void draw_clear()
{
    draw_rect({ 0, 0, (float)VIEW_W, (float)VIEW_H }, { 8, 6, 14, 200 });
    text_center(G.big, G.world_done ? "PORTAIL" : "ALLUMÉ", 30, rgb(0xffb070));
    char buf[64];
    text_center(G.font, G.banner.c_str(), 64, rgb(0xd8d0e8));
    snprintf(buf, sizeof(buf), "braises  %d / %d", G.embers, G.embers_total);
    text_center(G.font, buf, 86, rgb(0xffd166));
    int s = (int)G.level_time;
    snprintf(buf, sizeof(buf), "temps    %d:%02d.%d", s / 60, s % 60, (int)((G.level_time - s) * 10));
    text_center(G.font, buf, 100, rgb(0xd8d0e8));
    snprintf(buf, sizeof(buf), "morts    %d", G.deaths);
    text_center(G.font, buf, 114, rgb(0xd8d0e8));
    if (!G.bonus_msg.empty())
        text_center(G.font, G.bonus_msg.c_str(), 130, rgb(0xffd166));
    if (fmod(engine_time(), 1.0) < 0.7)
        text_center(G.font, "appuie sur saut", 150, rgb(0x8a7fa0));
}

void draw_end()
{
    render_set_view({ 0, 0 });
    Rect view = { (float)(engine_time() * 8), 0, (float)VIEW_W, (float)VIEW_H };
    draw_background(view);
    render_set_blend(BLEND_ADD);
    for (int i = 0; i < 5; i++) {
        float x = 50.f + i * 55, y = 150 + sinf((float)engine_time() * 2 + i) * 3;
        draw_light({ x, y }, 40, { 255, 140, 60, 120 });
    }
    render_set_blend(BLEND_ALPHA);

    text_center(G.big, "LE JOUR SE LÈVE", 24, rgb(0xffd166));
    char buf[64];
    snprintf(buf, sizeof(buf), "braises  %d / %d", G.run_embers, G.run_embers_total);
    text_center(G.font, buf, 70, rgb(0xffd166));
    int rs = (int)G.run_time;
    snprintf(buf, sizeof(buf), "temps    %d:%02d.%d", rs / 60, rs % 60, (int)((G.run_time - rs) * 10));
    text_center(G.font, buf, 84, rgb(0xd8d0e8));
    snprintf(buf, sizeof(buf), "morts    %d", G.run_deaths);
    text_center(G.font, buf, 98, rgb(0xd8d0e8));
    text_center(G.font, "merci d’avoir joué", 126, rgb(0x8a7fa0));
    if (fmod(engine_time(), 1.0) < 0.7)
        text_center(G.font, "appuie sur saut", 150, rgb(0x5a5070));
}

void editor_draw_world(Scene& s, Rect view)
{
    int ts = G.tileset.size;
    draw_rect({ 0, 0, (float)s.map.w * ts, (float)s.map.h * ts }, rgb(0x16121f));
    draw_level(s, view, 1);
    s.world.each<Sprite, Transform>([&](Entity, Sprite& spr, Transform& t) {
        if (spr.visible) return;
        const SpriteSheet* sh = sprite_get(spr.sprite);
        if (!sh) return;
        SpriteOpts o;
        o.origin = sh->origin;
        o.tint = { 255, 255, 255, 170 };
        draw_spr_ex(spr.sprite, spr.frame, t.pos + spr.offset, o);
    });
    s.world.each<Light, Transform>([&](Entity, Light& l, Transform& t) {
        if (l.radius > 0)
            draw_circle_lines(t.pos + l.offset, l.radius, 0.5f, fade(l.color, 0.25f));
    });
}

