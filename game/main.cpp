// Lueur: a small platformer about a flame relighting a cave. Built on kiln.
//
// F1 opens the editor on the current level (dev builds, when assets/ is
// next to the working directory). In the editor F5 plays from the start
// marker, shift+F5 from the middle of the view, and F5 again comes back.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game.h"
#include "relics.h"
#include "title.h"
#include "touch.h"

static void show_end()
{
    G.mode = MODE_END;
    music_play(music("title"), 0.5f, 2);
}

static void start_game()
{
    G.replay = false;
    G.run_time = 0;
    G.run_deaths = 0;
    G.run_embers = G.run_embers_total = 0;
    G.has[WEAPON_SPEAR] = G.has[WEAPON_BOW] = false;
    G.powers.clear();
    relics_reset();
    G.weapon = WEAPON_SPEAR;
    G.quiver = 0;
    if (!level_load(0))
        fprintf(stderr, "couldn't load the first level\n");
}

static void continue_game()
{
    G.replay = false;
    int level = saved_level();
    if (level < 0 || !load_saved_run()) {
        start_game();
        return;
    }
    weapons_for_level(level);
    if (!level_load(level))
        start_game();
}

static void load_next_level()
{
    if (G.next_level >= 0)
        level_load(G.next_level);
    G.next_level = -1;
}

// after the clear screen: next level, next world through its cutscene, or the end
static void advance()
{
    int next = G.level + 1;
    if (next >= (int)G.levels.size()) {
        cutscene_start("fin", show_end);
        return;
    }
    int world = G.level_world[next];
    if (world != G.level_world[G.level] && !G.worlds[world].intro.empty()) {
        G.next_level = next;
        cutscene_start(G.worlds[world].intro, load_next_level);
        return;
    }
    level_load(next);
}

static void init()
{
    scripting_init();
    register_engine_components();
    register_game_components();
    register_natives();

    if (!assets_init("assets", "game.pak")) {
        fprintf(stderr, "no assets found (need assets/ or game.pak)\n");
        engine_quit();
        return;
    }
    game_load_data();
    game_actions();
    options_load();
    options_apply();
    load_progress();
    load_parallax(Json());
    G.assets_gen = assets_generation();

    G.font = font_create("Consolas", 12);
    G.big = font_create("Consolas", 28);
    G.light = canvas_create(VIEW_W, VIEW_H);
    ui_init();

    EditorHooks hooks = {};
    hooks.draw_world = editor_draw_world;
    hooks.tileset = &G.tileset;
    editor_init(hooks);

    title_build_menu();

    // LUEUR_LEVEL=n jumps straight into a level, handy with KILN_SHOT
    if (const char* lv = getenv("LUEUR_LEVEL")) {
        start_game();
        weapons_for_level(atoi(lv));
        if (G.has[WEAPON_BOW])
            G.quiver = 6;
        level_load(atoi(lv));
        G.fade = 0;
        tween_clear();
        // LUEUR_EDIT=1 opens that level in the editor instead, zoomed to fit
        if (getenv("LUEUR_EDIT")) {
            open_editor();
            editor_fit();
        }
    } else {
        music_play(music("title"), 0.5f, 2);
    }
}

static void on_reload()
{
    game_load_data();
    scripting_reload();
    G.assets_gen = assets_generation();
}

static void frame(const Input& in, float dt)
{
    (void)dt;
    if (assets_poll())
        on_reload();

    bool dev = assets_dev();

    if (G.mode == MODE_PLAY) {
        touch_set_gameplay_enabled(true);
        touch_set_button_enabled(TOUCH_BTN_JUMP, true);
        touch_set_button_enabled(TOUCH_BTN_ATTACK, G.has[WEAPON_SPEAR] || G.has[WEAPON_BOW]);
        touch_set_button_enabled(TOUCH_BTN_DASH, has_power("elan"));
        touch_set_button_enabled(TOUCH_BTN_ECLAT, has_power("eclat"));
        touch_set_button_enabled(TOUCH_BTN_SWAP, G.has[WEAPON_SPEAR] && G.has[WEAPON_BOW]);
        touch_set_button_enabled(TOUCH_BTN_MENU, true);
    } else {
        touch_set_gameplay_enabled(false);
        touch_set_button_enabled(TOUCH_BTN_MENU, false);
    }

    switch (G.mode) {
    case MODE_TITLE:
        G.title_time += dt;
        {
            int item = title_update(in, dt);
            if (item >= 0) {
                audio_play(sfx("select"), 0.5f);
                if (item == ITEM_CONTINUE) {
                    continue_game();
                } else if (item == ITEM_PLAY) {
                    cutscene_start("intro", start_game);
                } else if (item == ITEM_WORLDS) {
                    select_open();
                } else if (item == ITEM_OPTIONS) {
                    options_open(MODE_TITLE);
                } else if (item == ITEM_EDITOR) {
                    if (level_load(0))
                        open_editor();
                } else {
                    engine_quit();
                }
            }
        }
        break;

    case MODE_STORY:
        cutscene_frame(in, dt);
        break;

    case MODE_PLAY:
        // while somebody talks the world waits
        if (dialogue_active()) {
            dialogue_frame(in, dt);
            if (!dialogue_active() && G.clear_after_talk) {
                G.clear_after_talk = false;
                enter_clear();
            }
            break;
        }
        if (G.playtest && (in.pressed[KEY_F5] || in.pressed[KEY_F1] || in.pressed[KEY_ESCAPE])) {
            stop_playtest();
        } else if (dev && in.pressed[KEY_F1]) {
            open_editor();
        } else if (input_back(in)) {
            G.mode = MODE_PAUSE;
            G.menu = 0;
            audio_play(sfx("menu"), 0.4f);
        } else if (in.pressed[KEY_R]) {
            level_load(G.level);
        }
        break;

    case MODE_OPTIONS:
        options_frame(in);
        break;

    case MODE_SELECT:
        select_frame(in);
        break;

    case MODE_PAUSE:
        pause_frame(in);
        break;

    case MODE_CLEAR:
        if (input_confirm(in)) {
            audio_play(sfx("select"), 0.5f);
            if (G.playtest) {
                stop_playtest();
            } else if (G.replay) {
                // back to the list it came from
                select_open();
                title_build_menu();
                music_play(music("title"), 0.5f, 1);
            } else {
                advance();
            }
        }
        break;

    case MODE_END:
        if (input_confirm(in)) {
            G.mode = MODE_TITLE;
            G.menu = 0;
            title_build_menu();
        }
        break;

    case MODE_EDIT:
        if (in.pressed[KEY_F5] && !ui_want_keyboard())
            start_playtest(in.down[KEY_SHIFT]);
        else if (in.pressed[KEY_F1] && !ui_want_keyboard())
            start_playtest(false);
        else
            editor_frame(in, dt);
        break;
    }
}

static void update(const Input& in, float dt)
{
    tween_update(dt);
    if (G.mode == MODE_PLAY && !dialogue_active())
        world_update(in, dt);
}

static void dev_overlay()
{
    const std::string& err = script_error();
    const std::string& log = assets_log();
    if (err.empty() && log.empty())
        return;
    render_screen_begin();
    render_present_canvas();
    float y = 8;
    if (!err.empty()) {
        ui_text_at(8, y, err.c_str(), { 255, 120, 110, 255 });
        y += 20;
    }
    if (!log.empty())
        ui_text_at(8, y, log.c_str(), { 255, 170, 110, 255 });
}

static void draw(float alpha)
{
    switch (G.mode) {
    case MODE_TITLE:
        draw_title();
        break;
    case MODE_STORY:
        cutscene_draw();
        break;
    case MODE_PLAY:
        draw_game(dialogue_active() ? 1 : alpha);
        dialogue_draw();
        break;
    case MODE_PAUSE:
        draw_game(1);
        draw_pause();
        break;
    case MODE_CLEAR:
        draw_game(1);
        draw_clear();
        break;
    case MODE_END:
        draw_end();
        break;
    case MODE_SELECT:
        draw_title();
        select_draw();
        break;
    case MODE_OPTIONS:
        if (options_from() == MODE_PAUSE)
            draw_game(1);
        else
            draw_title();
        options_draw();
        break;
    case MODE_EDIT:
        render_screen_begin();
        editor_draw();
        return;
    }
    if (assets_dev())
        dev_overlay();
}

static void shutdown()
{
    font_free(&G.font);
    font_free(&G.big);
    canvas_free(&G.light);
    ui_shutdown();
    scripting_shutdown();
    assets_shutdown();
}

int main()
{
    GameDesc g = {};
    g.title = "Lueur";
    g.width = VIEW_W;
    g.height = VIEW_H;
    g.scale = 4;
    g.tick_rate = 120;
    g.clear = rgb(0x0d0b14);
    g.init = init;
    g.frame = frame;
    g.update = update;
    g.draw = draw;
    g.shutdown = shutdown;
    return engine_run(g);
}
