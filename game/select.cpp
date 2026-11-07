// Picking a world, then a level in it, to play again. Only what's been
// reached shows up. Levels played from here don't move the save point and
// come back here when they're done.

#include <cstdio>
#include <cstring>
#include "game.h"

static int world_sel;
static int level_sel;
static bool in_levels;

// the worlds that have at least one level reached
static std::vector<int> open_worlds()
{
    std::vector<int> out;
    int first = 0;
    for (size_t w = 0; w < G.worlds.size(); w++) {
        if (first <= G.reached)
            out.push_back((int)w);
        first += (int)G.worlds[w].levels.size();
    }
    return out;
}

static int first_level(int world)
{
    int n = 0;
    for (int w = 0; w < world; w++)
        n += (int)G.worlds[w].levels.size();
    return n;
}

void select_open()
{
    in_levels = false;
    world_sel = 0;
    level_sel = 0;
    G.mode = MODE_SELECT;
}

static bool nav(const Input& in, int* sel, int count)
{
    bool up = in.pressed[KEY_UP] || action_pressed(in, ACT_UP) || in.pad_pressed[PAD_UP];
    bool down = in.pressed[KEY_DOWN] || action_pressed(in, ACT_DOWN) || in.pad_pressed[PAD_DOWN];
    if ((up || down) && count > 0) {
        *sel = (*sel + (down ? 1 : count - 1)) % count;
        audio_play(sfx("menu"), 0.4f);
        return true;
    }
    return false;
}

static void play(int level)
{
    G.replay = true;
    G.run_time = 0;
    G.run_deaths = 0;
    G.run_embers = G.run_embers_total = 0;
    G.has[WEAPON_SPEAR] = G.has[WEAPON_BOW] = false;
    G.powers.clear();
    weapons_for_level(level);
    G.quiver = G.has[WEAPON_BOW] ? 6 : 0;
    level_load(level);
}

void select_frame(const Input& in)
{
    std::vector<int> worlds = open_worlds();
    bool back = input_back(in) || in.pad_pressed[PAD_B];
    bool ok = in.pressed[KEY_ENTER] || in.pressed[KEY_SPACE] || in.pad_pressed[PAD_A];
    if (!in_levels) {
        nav(in, &world_sel, (int)worlds.size() + 1);
        if (back || (ok && world_sel == (int)worlds.size())) {
            audio_play(sfx("select"), 0.5f);
            G.mode = MODE_TITLE;
            G.menu = 0;
            return;
        }
        if (ok) {
            audio_play(sfx("select"), 0.5f);
            in_levels = true;
            level_sel = 0;
        }
        return;
    }
    int w = worlds[world_sel];
    int first = first_level(w);
    int count = 0;
    for (size_t i = 0; i < G.worlds[w].levels.size(); i++)
        if (first + (int)i <= G.reached)
            count++;
    nav(in, &level_sel, count + 1);
    if (back || (ok && level_sel == count)) {
        audio_play(sfx("select"), 0.5f);
        in_levels = false;
        return;
    }
    if (ok) {
        audio_play(sfx("select"), 0.5f);
        play(first + level_sel);
    }
}

static void line(const char* left, const char* right, float y, bool on, Color right_color)
{
    Color c = on ? rgb(0xffd166) : rgb(0x8a7fa0);
    if (on)
        draw_text(G.font, ">", 36, y, c);
    draw_text(G.font, left, 48, y, c);
    if (right)
        draw_text(G.font, right, 210, y, right_color);
}

void select_draw()
{
    draw_rect({ 0, 0, (float)VIEW_W, (float)VIEW_H }, { 8, 6, 14, 252 });
    std::vector<int> worlds = open_worlds();
    char buf[64];
    if (!in_levels) {
        draw_text(G.big, "MONDES", 48, 4, rgb(0xfff4c2));
        for (size_t i = 0; i < worlds.size(); i++) {
            int w = worlds[i];
            int got = 0, total = 0;
            world_embers(w, &got, &total);
            bool full = total > 0 && got >= total;
            if (total > 0)
                snprintf(buf, sizeof(buf), "%s%d / %d", full ? "* " : "", got, total);
            else
                snprintf(buf, sizeof(buf), "-");
            line(G.worlds[w].name.c_str(), buf, 36 + i * 11.f, world_sel == (int)i,
                 full ? rgb(0xffd166) : rgb(0xd8d0e8));
        }
        line("retour", nullptr, 36 + worlds.size() * 11.f + 4, world_sel == (int)worlds.size(), WHITE);
        // what collecting everything there gives
        if (world_sel < (int)worlds.size()) {
            const char* what = bonus_text(G.worlds[worlds[world_sel]].id.c_str());
            if (*what) {
                int got = 0, total = 0;
                world_embers(worlds[world_sel], &got, &total);
                Color c = got >= total ? rgb(0xffd166) : rgb(0x6a6080);
                text_center_at(got >= total ? "bonus gagné :" : "toutes les braises donnent :", VIEW_H - 30.f, c);
                text_center_at(what, VIEW_H - 18.f, c);
            }
        }
        return;
    }
    int w = worlds[world_sel];
    int first = first_level(w);
    draw_text(G.big, G.worlds[w].name.c_str(), 48, 4, rgb(0xfff4c2));
    int n = 0;
    for (size_t i = 0; i < G.worlds[w].levels.size(); i++) {
        int level = first + (int)i;
        if (level > G.reached)
            break;
        int total = G.level_embers[level];
        int got = best_embers(level);
        if (total > 0)
            snprintf(buf, sizeof(buf), "%d / %d", got, total);
        else
            snprintf(buf, sizeof(buf), "-");
        line(G.level_titles[level].c_str(), buf, 40 + n * 12.f, level_sel == n,
             total > 0 && got >= total ? rgb(0xffd166) : rgb(0xd8d0e8));
        n++;
    }
    line("retour", nullptr, 40 + n * 12.f + 4, level_sel == n, WHITE);
}
