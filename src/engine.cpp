#include "engine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "audio.h"
#include "touch.h"

static bool want_quit;
static float hitstop;
static uint64_t ticks;
static uint32_t rng = 0x9e3779b9;

uint32_t next_component_id()
{
    static uint32_t n;
    return n++;
}

static uint32_t xorshift()
{
    uint32_t x = rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng = x;
}

float randf(float lo, float hi)
{
    return lo + (hi - lo) * (xorshift() >> 8) * (1.0f / 16777216.0f);
}

int randi(int lo, int hi)
{
    return lo + int(xorshift() % uint32_t(hi - lo + 1));
}

void engine_quit()
{
    want_quit = true;
}

void engine_hitstop(float seconds)
{
    if (seconds > hitstop)
        hitstop = seconds;
}

double engine_time()
{
    return platform_time();
}

uint64_t engine_ticks()
{
    return ticks;
}

static void screenshot()
{
    char name[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(name, sizeof(name), "shot%03d.png", i);
        FILE* f = fopen(name, "rb");
        if (!f)
            break;
        fclose(f);
    }
    if (render_screenshot(name))
        printf("saved %s\n", name);
}

// KILN_INPUT=file plays back a list of key presses, one per line:
//
//   120 right down        tick, key, down/up
//   180 space down
//   200 space up
//   600 shot out.png      screenshot at that tick
//   900 quit
//
// Ticks are fixed updates, so a run replays the same way every time.
// It's how the levels get checked without sitting there playing them.

struct ReplayEvent {
    uint64_t tick;
    int key;            // -1 for shot / quit
    bool down;
    std::string arg;
};

static std::vector<ReplayEvent> replay;
static size_t replay_pos;

static int key_by_name(const char* s)
{
    static const struct { const char* name; int key; } names[] = {
        { "left", KEY_LEFT }, { "right", KEY_RIGHT }, { "up", KEY_UP }, { "down", KEY_DOWN },
        { "space", KEY_SPACE }, { "enter", KEY_ENTER }, { "escape", KEY_ESCAPE }, { "tab", KEY_TAB },
        { "shift", KEY_SHIFT }, { "ctrl", KEY_CTRL },
    };
    for (const auto& n : names)
        if (!strcmp(s, n.name))
            return n.key;
    if (s[0] >= 'a' && s[0] <= 'z' && !s[1])
        return KEY_A + (s[0] - 'a');
    if (s[0] >= '0' && s[0] <= '9' && !s[1])
        return KEY_0 + (s[0] - '0');
    if (s[0] == 'f' && s[1]) {
        int n = atoi(s + 1);
        if (n >= 1 && n <= 12)
            return KEY_F1 + n - 1;
    }
    return KEY_NONE;
}

static void load_replay(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "replay: can't open %s\n", path);
        return;
    }
    char line[256];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        n++;
        unsigned long long tick;
        char a[64] = "", b[200] = "";
        if (line[0] == '#' || sscanf(line, "%llu %63s %199s", &tick, a, b) < 2)
            continue;
        ReplayEvent ev = { tick, -1, false, "" };
        if (!strcmp(a, "shot") || !strcmp(a, "quit")) {
            ev.arg = !strcmp(a, "quit") ? "" : b;
        } else {
            ev.key = key_by_name(a);
            ev.down = !strcmp(b, "down");
            if (ev.key == KEY_NONE) {
                fprintf(stderr, "replay line %d: no key called %s\n", n, a);
                continue;
            }
        }
        replay.push_back(ev);
    }
    fclose(f);
}

// returns false when the replay asks to quit
static bool apply_replay(Input& in)
{
    while (replay_pos < replay.size() && replay[replay_pos].tick <= ticks) {
        const ReplayEvent& ev = replay[replay_pos++];
        if (ev.key >= 0) {
            if (ev.down && !in.down[ev.key])
                in.pressed[ev.key] = true;
            if (!ev.down && in.down[ev.key])
                in.released[ev.key] = true;
            in.down[ev.key] = ev.down;
        } else if (!ev.arg.empty()) {
            // the frame isn't drawn yet, so take it after this tick's draw
            return true;
        } else {
            return false;
        }
    }
    return true;
}

static void clear_edges(Input& in)
{
    memset(in.pressed, 0, sizeof(in.pressed));
    memset(in.released, 0, sizeof(in.released));
    memset(in.mouse_pressed, 0, sizeof(in.mouse_pressed));
    memset(in.mouse_released, 0, sizeof(in.mouse_released));
    memset(in.pad_pressed, 0, sizeof(in.pad_pressed));
    in.wheel = 0;
    in.text_len = 0;
}

// held state comes from src, edges pile up so a tap between two ticks isn't lost
static void merge(Input& dst, const Input& src)
{
    Input edges = dst;
    dst = src;
    for (int i = 0; i < KEY_COUNT; i++) {
        dst.pressed[i] = edges.pressed[i] || src.pressed[i];
        dst.released[i] = edges.released[i] || src.released[i];
    }
    for (int i = 0; i < MOUSE_COUNT; i++) {
        dst.mouse_pressed[i] = edges.mouse_pressed[i] || src.mouse_pressed[i];
        dst.mouse_released[i] = edges.mouse_released[i] || src.mouse_released[i];
    }
    for (int i = 0; i < PAD_COUNT; i++)
        dst.pad_pressed[i] = edges.pad_pressed[i] || src.pad_pressed[i];
    dst.wheel = edges.wheel + src.wheel;
}

int engine_run(const GameDesc& g)
{
    int scale = g.scale > 0 ? g.scale : 2;
    int sw, sh;
    platform_screen_size(&sw, &sh);
    while (scale > 1 && (g.width * scale > sw * 0.9 || g.height * scale > sh * 0.9))
        scale--;

    if (!platform_init(g.title, g.width * scale, g.height * scale))
        return 1;
    render_init(g.width, g.height);
    audio_init();
    touch_init();

    rng = uint32_t(time(0)) | 1;
    if (g.init)
        g.init();

    // KILN_SHOT=out.png grabs a frame (KILN_SHOT_FRAME, default 60) and exits,
    // so rendering can be checked from a script
    const char* auto_shot = getenv("KILN_SHOT");
    int shot_frame = getenv("KILN_SHOT_FRAME") ? atoi(getenv("KILN_SHOT_FRAME")) : 60;

    if (const char* rp = getenv("KILN_INPUT"))
        load_replay(rp);
    // replay keys live here, the real keyboard still works alongside. What the
    // ticks got is also handed to the next frame, so menus and dialogue see it
    Input held = {};
    Input replay_frame = {};
    std::vector<std::string> pending_shots;

    const double dt = 1.0 / (g.tick_rate > 0 ? g.tick_rate : 60);
    double acc = 0;
    double last = platform_time();
    int frame = 0;
    Input raw = {}, tick = {};

    while (!want_quit && platform_poll(&raw)) {
        vec2 m = render_screen_to_canvas(raw.mouse_px, raw.mouse_py);
        raw.mouse_x = m.x;
        raw.mouse_y = m.y;
        for (int i = 0; i < raw.touch_count; i++) {
            vec2 tc = render_screen_to_canvas(raw.touches[i].px, raw.touches[i].py);
            raw.touches[i].x = tc.x;
            raw.touches[i].y = tc.y;
        }
        touch_update(&raw);

        // during a replay the real keyboard and mouse stay out of it, whatever
        // gets typed while the window happens to have focus would change the run
        if (!replay.empty()) {
            memset(raw.down, 0, sizeof(raw.down));
            memset(raw.mouse_down, 0, sizeof(raw.mouse_down));
            memset(raw.pad_down, 0, sizeof(raw.pad_down));
            clear_edges(raw);
            raw.pad_lx = raw.pad_ly = 0;
        }

        if (raw.pressed[KEY_F11])
            platform_toggle_fullscreen();
        bool shot = raw.pressed[KEY_F2];

        Input frame_in = raw;
        merge(tick, raw);
        clear_edges(raw);

        double now = platform_time();
        double elapsed = now - last;
        last = now;
        // coming back from a breakpoint or a window drag, don't try to catch up on all of it
        if (elapsed > 0.25)
            elapsed = 0.25;

        for (int k = 0; k < KEY_COUNT; k++) {
            frame_in.down[k] = frame_in.down[k] || replay_frame.down[k];
            frame_in.pressed[k] = frame_in.pressed[k] || replay_frame.pressed[k];
            frame_in.released[k] = frame_in.released[k] || replay_frame.released[k];
        }
        clear_edges(replay_frame);

        if (g.frame)
            g.frame(frame_in, (float)elapsed);

        if (hitstop > 0) {
            hitstop -= (float)elapsed;
            acc = 0;
        } else {
            acc += elapsed;
        }

        while (acc >= dt) {
            if (!replay.empty()) {
                size_t before = replay_pos;
                if (!apply_replay(held))
                    want_quit = true;
                for (size_t i = before; i < replay_pos; i++)
                    if (replay[i].key < 0 && !replay[i].arg.empty())
                        pending_shots.push_back(replay[i].arg);
                // the replay owns the keys outright: OR-ing with the frame's
                // state would hold a released key until the next frame,
                // and how many ticks share a frame depends on the machine
                for (int k = 0; k < KEY_COUNT; k++) {
                    tick.down[k] = held.down[k];
                    tick.pressed[k] = tick.pressed[k] || held.pressed[k];
                    tick.released[k] = tick.released[k] || held.released[k];
                    replay_frame.down[k] = held.down[k];
                    replay_frame.pressed[k] = replay_frame.pressed[k] || held.pressed[k];
                    replay_frame.released[k] = replay_frame.released[k] || held.released[k];
                }
                clear_edges(held);
            }
            if (g.update)
                g.update(tick, (float)dt);
            clear_edges(tick);
            ticks++;
            acc -= dt;
        }

        render_frame_begin();
        render_canvas_begin(g.clear);
        if (g.draw)
            g.draw((float)(acc / dt));
        render_set_view({ 0, 0 });
        touch_draw();
        render_frame_end();

        if (shot)
            screenshot();
        // several can land on one frame when a lot of ticks run at once
        for (const std::string& path : pending_shots)
            render_screenshot(path.c_str());
        pending_shots.clear();
        if (auto_shot && ++frame == shot_frame) {
            render_screenshot(auto_shot);
            want_quit = true;
        }

        platform_swap();
    }

    if (g.shutdown)
        g.shutdown();
    audio_shutdown();
    render_shutdown();
    platform_shutdown();
    return 0;
}
