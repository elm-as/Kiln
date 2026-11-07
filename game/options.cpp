// Options: volumes, fullscreen and the keys, kept in options.json next to
// the save. The keys themselves live in the engine (src/actions.h); this
// only says which actions Lueur has and draws the screens. The pad keeps
// its own fixed layout.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game.h"

Options O;

// in the same order as the ACT_ enum
void game_actions()
{
    action_define("gauche", "gauche", { KEY_LEFT, KEY_Q, KEY_A });
    action_define("droite", "droite", { KEY_RIGHT, KEY_D });
    action_define("haut", "haut", { KEY_UP, KEY_Z, KEY_W });
    action_define("bas", "bas", { KEY_DOWN, KEY_S });
    action_define("saut", "saut", { KEY_SPACE, KEY_K, KEY_C });
    action_define("arme", "arme", { KEY_X, KEY_J });
    action_define("changer", "changer d'arme", { KEY_E, KEY_L });
    action_define("elan", "élan", { KEY_SHIFT, KEY_F });
    action_define("eclat", "éclat", { KEY_V, KEY_I });
    action_define("pause", "pause", { KEY_P, KEY_TAB, KEY_ESCAPE });
}

void options_defaults()
{
    O.music = 8;
    O.sfx = 8;
    O.fullscreen = false;
    actions_reset();
}

void options_apply()
{
    audio_groups(O.music / 10.f, O.sfx / 10.f);
    if (O.fullscreen != platform_fullscreen())
        platform_toggle_fullscreen();
}

void options_load()
{
    options_defaults();
    std::string text, err;
    Json j;
    if (!read_text(options_path().c_str(), text) || !json_parse(text, &j, &err))
        return;
    O.music = clampi(j["musique"].i(O.music), 0, 10);
    O.sfx = clampi(j["effets"].i(O.sfx), 0, 10);
    O.fullscreen = j["plein_ecran"].boolean_or(false);
    actions_load(j["touches"]);
}

void options_save()
{
    Json j = Json::object();
    j.set("musique", Json::number(O.music));
    j.set("effets", Json::number(O.sfx));
    j.set("plein_ecran", Json::boolean(O.fullscreen));
    j.set("touches", actions_save());
    std::string text = json_write(j);
    write_file(options_path().c_str(), text.data(), text.size());
}

// ---------------------------------------------------------------------------
// the screens: options, and the list of keys under it

enum { PAGE_MAIN, PAGE_KEYS };
static int page;
static int sel;
static int waiting = -1;    // action waiting for a key press
static Mode back_to;

void options_open(Mode from)
{
    back_to = from;
    page = PAGE_MAIN;
    sel = 0;
    waiting = -1;
    O.fullscreen = platform_fullscreen();
    G.mode = MODE_OPTIONS;
}

Mode options_from()
{
    return back_to;
}

static void close_options()
{
    O.fullscreen = platform_fullscreen();
    options_save();
    G.mode = back_to;
    G.menu = 0;
}

static bool nav_up(const Input& in)
{
    return in.pressed[KEY_UP] || action_pressed(in, ACT_UP) || in.pad_pressed[PAD_UP];
}

static bool nav_down(const Input& in)
{
    return in.pressed[KEY_DOWN] || action_pressed(in, ACT_DOWN) || in.pad_pressed[PAD_DOWN];
}

static bool nav_left(const Input& in)
{
    return in.pressed[KEY_LEFT] || action_pressed(in, ACT_LEFT) || in.pad_pressed[PAD_LEFT];
}

static bool nav_right(const Input& in)
{
    return in.pressed[KEY_RIGHT] || action_pressed(in, ACT_RIGHT) || in.pad_pressed[PAD_RIGHT];
}

static bool nav_ok(const Input& in)
{
    return in.pressed[KEY_ENTER] || in.pressed[KEY_SPACE] || in.pad_pressed[PAD_A];
}

static bool nav_back(const Input& in)
{
    return action_pressed(in, ACT_PAUSE) || in.pad_pressed[PAD_B] || in.pad_pressed[PAD_START];
}

static void move(const Input& in, int count)
{
    if (nav_up(in) || nav_down(in)) {
        sel = (sel + (nav_down(in) ? 1 : count - 1)) % count;
        audio_play(sfx("menu"), 0.4f);
    }
}

const int MAIN_ITEMS = 5;      // musique, effets, plein écran, touches, retour
const int KEY_ITEMS = ACT_COUNT + 2;   // the actions, par défaut, retour

void options_frame(const Input& in)
{
    if (page == PAGE_KEYS && waiting >= 0) {
        for (int k = 1; k < KEY_COUNT; k++) {
            if (!in.pressed[k] || k == KEY_F11)
                continue;
            action_bind(waiting, k);
            audio_play(sfx("select"), 0.5f);
            waiting = -1;
            return;
        }
        return;
    }

    bool clicked = in.mouse_pressed[MOUSE_LEFT];
    float click_x = in.mouse_x, click_y = in.mouse_y;
    for (int t = 0; t < in.touch_count; t++) {
        if (in.touches[t].phase == TOUCH_DOWN) {
            clicked = true;
            click_x = in.touches[t].x;
            click_y = in.touches[t].y;
            break;
        }
    }

    if (page == PAGE_MAIN) {
        if (clicked) {
            for (int i = 0; i < MAIN_ITEMS; i++) {
                float iy = 60.f + i * 14.f;
                if (click_y >= iy - 4.f && click_y < iy + 12.f && click_x >= 40.f && click_x <= 280.f) {
                    sel = i;
                    if (i == 0) {
                        int step = click_x < 170.f ? -1 : 1;
                        O.music = clampi(O.music + step, 0, 10);
                        options_apply();
                        audio_play(sfx("menu"), 0.4f);
                    } else if (i == 1) {
                        int step = click_x < 170.f ? -1 : 1;
                        O.sfx = clampi(O.sfx + step, 0, 10);
                        options_apply();
                        audio_play(sfx("ember"), 0.6f);
                    } else if (i == 2) {
                        audio_play(sfx("select"), 0.5f);
                        platform_toggle_fullscreen();
                        O.fullscreen = platform_fullscreen();
                    } else if (i == 3) {
                        audio_play(sfx("select"), 0.5f);
                        page = PAGE_KEYS;
                        sel = 0;
                    } else if (i == 4) {
                        audio_play(sfx("select"), 0.5f);
                        close_options();
                        return;
                    }
                }
            }
        }
        move(in, MAIN_ITEMS);
        int step = nav_right(in) ? 1 : nav_left(in) ? -1 : 0;
        if (sel == 0 && step) {
            O.music = clampi(O.music + step, 0, 10);
            options_apply();
            audio_play(sfx("menu"), 0.4f);
        } else if (sel == 1 && step) {
            O.sfx = clampi(O.sfx + step, 0, 10);
            options_apply();
            audio_play(sfx("ember"), 0.6f);
        } else if (nav_ok(in)) {
            audio_play(sfx("select"), 0.5f);
            if (sel == 2) {
                platform_toggle_fullscreen();
                O.fullscreen = platform_fullscreen();
            } else if (sel == 3) {
                page = PAGE_KEYS;
                sel = 0;
            } else if (sel == 4) {
                close_options();
            }
        } else if (nav_back(in)) {
            close_options();
        }
        return;
    }

    if (clicked && click_y >= 160.f && click_y <= 180.f && click_x >= 40.f && click_x <= 280.f) {
        audio_play(sfx("select"), 0.5f);
        page = PAGE_MAIN;
        sel = 3;
        return;
    }

    move(in, KEY_ITEMS);
    if (nav_ok(in)) {
        audio_play(sfx("select"), 0.5f);
        if (sel < ACT_COUNT) {
            waiting = sel;
        } else if (sel == ACT_COUNT) {
            actions_reset();
        } else {
            page = PAGE_MAIN;
            sel = 3;
        }
    } else if (nav_back(in)) {
        page = PAGE_MAIN;
        sel = 3;
    }
}

static void row(const char* left, const char* right, float y, bool on)
{
    Color c = on ? rgb(0xffd166) : rgb(0x8a7fa0);
    if (on)
        draw_text(G.font, ">", 50, y, c);
    draw_text(G.font, left, 62, y, c);
    if (right)
        draw_text(G.font, right, 170, y, on ? rgb(0xfff4c2) : rgb(0xd8d0e8));
}

static const char* bar(int v)
{
    static char buf[32];
    int n = 0;
    for (int i = 0; i < 10; i++)
        buf[n++] = i < v ? '|' : '.';
    snprintf(buf + n, sizeof(buf) - n, " %d", v);
    return buf;
}

void options_draw()
{
    draw_rect({ 0, 0, (float)VIEW_W, (float)VIEW_H }, { 8, 6, 14, 252 });
    if (page == PAGE_MAIN) {
        draw_text(G.big, "OPTIONS", 62, 16, rgb(0xfff4c2));
        row("musique", bar(O.music), 60, sel == 0);
        row("effets", bar(O.sfx), 74, sel == 1);
        row("plein écran", platform_fullscreen() ? "oui" : "non", 88, sel == 2);
        row("touches", nullptr, 102, sel == 3);
        row("retour", nullptr, 116, sel == 4);
        draw_text(G.font, "gauche/droite règle le volume", 62, 150, rgb(0x5a5070));
        return;
    }
    draw_text(G.big, "TOUCHES", 62, 6, rgb(0xfff4c2));
    for (int a = 0; a < ACT_COUNT; a++) {
        char keys[64] = "";
        if (waiting == a) {
            snprintf(keys, sizeof(keys), "appuie sur une touche…");
        } else {
            for (int i = 0; i < ACTION_KEYS; i++) {
                int k = action_keys(a)[i];
                if (!k)
                    continue;
                if (keys[0])
                    strncat(keys, "  ", sizeof(keys) - strlen(keys) - 1);
                strncat(keys, key_name(k), sizeof(keys) - strlen(keys) - 1);
            }
        }
        row(action_label(a), keys, 38 + a * 11.f, sel == a);
    }
    row("par défaut", nullptr, 38 + ACT_COUNT * 11.f + 4, sel == ACT_COUNT);
    row("retour", nullptr, 38 + ACT_COUNT * 11.f + 15, sel == ACT_COUNT + 1);
}
