#include "touch.h"
#include <cmath>
#include <cstdlib>
#include "render.h"

enum ButtonIcon {
    ICON_JUMP,
    ICON_ATTACK,
    ICON_DASH,
    ICON_ECLAT,
    ICON_SWAP,
    ICON_MENU
};

struct VirtualButton {
    int pad_btn;
    int key;
    vec2 pos;
    float radius;
    const char* label;
    ButtonIcon icon;
    bool enabled;
    bool down;
    bool prev;
};

static bool touch_enabled = false;
static bool touch_sim_mouse = false;
static bool touch_gameplay_enabled = true;

static const vec2 STICK_DEFAULT_ANCHOR = { 45.0f, 135.0f };
static bool stick_active = false;
static int stick_id = -1;
static vec2 stick_anchor = STICK_DEFAULT_ANCHOR;
static vec2 stick_pos = STICK_DEFAULT_ANCHOR;
static const float STICK_RADIUS = 26.0f;

static bool stick_held_left = false, stick_held_right = false;
static bool stick_held_up = false, stick_held_down = false;

static const int BUTTON_COUNT = 6;
static VirtualButton all_buttons[BUTTON_COUNT] = {
    { PAD_A,     KEY_SPACE,  { 288, 146 }, 16, "A", ICON_JUMP,   true,  false, false },
    { PAD_X,     KEY_X,      { 260, 154 }, 14, "X", ICON_ATTACK, false, false, false },
    { PAD_LB,    KEY_SHIFT,  { 292, 114 }, 13, "D", ICON_DASH,   false, false, false },
    { PAD_B,     KEY_V,      { 234, 158 }, 12, "E", ICON_ECLAT,  false, false, false },
    { PAD_Y,     KEY_E,      { 264, 124 }, 12, "W", ICON_SWAP,   false, false, false },
    { PAD_START, KEY_ESCAPE, { 306,  14 }, 10, "P", ICON_MENU,   true,  false, false },
};

void touch_init()
{
    touch_enabled = (getenv("KILN_TOUCH") != nullptr);
    touch_sim_mouse = (getenv("KILN_TOUCH_MOUSE") != nullptr);
}

void touch_set_enabled(bool enabled) { touch_enabled = enabled; }
bool touch_is_enabled() { return touch_enabled; }
bool touch_is_gameplay_enabled() { return touch_gameplay_enabled; }

void touch_set_gameplay_enabled(bool enabled)
{
    touch_gameplay_enabled = enabled;
    if (!enabled) {
        stick_active = false;
        stick_id = -1;
        stick_anchor = stick_pos = STICK_DEFAULT_ANCHOR;
    }
}

void touch_set_button_enabled(TouchButtonId btn, bool enabled)
{
    if (btn < 0 || btn >= TOUCH_BTN_COUNT) return;
    all_buttons[btn].enabled = enabled;
    if (!enabled) all_buttons[btn].down = all_buttons[btn].prev = false;
}

bool touch_is_button_enabled(TouchButtonId btn)
{
    return (btn >= 0 && btn < TOUCH_BTN_COUNT) && all_buttons[btn].enabled;
}

static float dist_sq(vec2 a, vec2 b)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Synchronizes synthetic key down/up states so released keys are not stuck down
static void update_stick_key(Input* in, int key, bool want, bool* held)
{
    if (want != *held) {
        in->down[key] = want;
        if (want) in->pressed[key] = true;
        else in->released[key] = true;
        *held = want;
    } else if (want) {
        in->down[key] = true;
    }
}

// Releases all virtual joystick keys and resets analog stick axes to neutral
static void release_stick(Input* in)
{
    update_stick_key(in, KEY_LEFT, false, &stick_held_left);
    update_stick_key(in, KEY_RIGHT, false, &stick_held_right);
    update_stick_key(in, KEY_UP, false, &stick_held_up);
    update_stick_key(in, KEY_DOWN, false, &stick_held_down);
    if (!in->pad_connected)
        in->pad_lx = in->pad_ly = 0.0f;
    stick_active = false;
    stick_id = -1;
    stick_anchor = stick_pos = STICK_DEFAULT_ANCHOR;
}

static void update_stick(const TouchPoint* touches, int count, Input* in)
{
    if (!touch_gameplay_enabled) {
        release_stick(in);
        return;
    }

    if (!stick_active) {
        for (int i = 0; i < count; i++) {
            if (touches[i].phase == TOUCH_UP || touches[i].phase == TOUCH_CANCEL)
                continue;
            if (touches[i].x < 150.0f && touches[i].y > 35.0f) {
                stick_active = true;
                stick_id = touches[i].id;
                float d_sq = dist_sq({ touches[i].x, touches[i].y }, STICK_DEFAULT_ANCHOR);
                stick_anchor = (d_sq <= (STICK_RADIUS + 12.0f) * (STICK_RADIUS + 12.0f))
                    ? STICK_DEFAULT_ANCHOR : vec2{ touches[i].x, touches[i].y };
                stick_pos = { touches[i].x, touches[i].y };
                break;
            }
        }
        if (!stick_active) {
            release_stick(in);
            return;
        }
    }

    bool found = false;
    vec2 cur = stick_anchor;
    for (int i = 0; i < count; i++) {
        if (touches[i].id != stick_id)
            continue;
        if (touches[i].phase == TOUCH_UP || touches[i].phase == TOUCH_CANCEL)
            break;
        found = true;
        cur = { touches[i].x, touches[i].y };
        break;
    }

    if (!found) {
        release_stick(in);
        return;
    }

    vec2 diff = cur - stick_anchor;
    float len = sqrtf(diff.x * diff.x + diff.y * diff.y);
    if (len > STICK_RADIUS && len > 0.001f)
        diff = diff * (STICK_RADIUS / len);
    stick_pos = stick_anchor + diff;

    float nx = diff.x / STICK_RADIUS;
    float ny = diff.y / STICK_RADIUS;
    in->pad_lx = nx;
    in->pad_ly = ny;

    update_stick_key(in, KEY_LEFT, nx < -0.3f, &stick_held_left);
    update_stick_key(in, KEY_RIGHT, nx > 0.3f, &stick_held_right);
    update_stick_key(in, KEY_UP, ny < -0.4f, &stick_held_up);
    update_stick_key(in, KEY_DOWN, ny > 0.4f, &stick_held_down);
}

static void release_btn(VirtualButton& btn, Input* in)
{
    if (btn.down || btn.prev) {
        in->pad_down[btn.pad_btn] = false;
        in->down[btn.key] = false;
        in->released[btn.key] = true;
        btn.down = false;
        btn.prev = false;
    }
}

static void update_buttons(const TouchPoint* touches, int count, Input* in)
{
    for (int b = 0; b < BUTTON_COUNT; b++) {
        VirtualButton& btn = all_buttons[b];
        if ((!touch_gameplay_enabled && b != TOUCH_BTN_MENU) || !btn.enabled) {
            release_btn(btn, in);
            continue;
        }

        btn.prev = btn.down;
        btn.down = false;

        float r_sq = (btn.radius + 8.0f) * (btn.radius + 8.0f);
        for (int i = 0; i < count; i++) {
            if (touches[i].phase == TOUCH_UP || touches[i].phase == TOUCH_CANCEL)
                continue;
            if (touches[i].x >= 140.0f && dist_sq({ touches[i].x, touches[i].y }, btn.pos) <= r_sq) {
                btn.down = true;
                break;
            }
        }

        if (btn.down) {
            in->pad_down[btn.pad_btn] = true;
            in->down[btn.key] = true;
            if (!btn.prev) {
                in->pad_pressed[btn.pad_btn] = true;
                in->pressed[btn.key] = true;
            }
        } else if (btn.prev) {
            in->pad_down[btn.pad_btn] = false;
            in->down[btn.key] = false;
            in->released[btn.key] = true;
        }
    }
}

void touch_update(Input* in)
{
    TouchPoint active_touches[MAX_TOUCHES];
    int count = 0;

    if (in->touch_count > 0) {
        touch_enabled = true;
        count = in->touch_count;
        for (int i = 0; i < count && i < MAX_TOUCHES; i++)
            active_touches[i] = in->touches[i];
    } else if (touch_enabled || touch_sim_mouse) {
        if (in->mouse_down[MOUSE_LEFT] || in->mouse_released[MOUSE_LEFT]) {
            TouchPoint pt;
            pt.id = 0;
            pt.x = in->mouse_x;
            pt.y = in->mouse_y;
            pt.px = in->mouse_px;
            pt.py = in->mouse_py;
            pt.phase = in->mouse_pressed[MOUSE_LEFT] ? TOUCH_DOWN
                     : (in->mouse_released[MOUSE_LEFT] ? TOUCH_UP : TOUCH_MOVE);
            active_touches[0] = pt;
            count = 1;
        }
    }

    if (!touch_enabled && count == 0)
        return;

    update_stick(active_touches, count, in);
    update_buttons(active_touches, count, in);
}

static void draw_btn_icon(ButtonIcon icon, vec2 c, float r, Color col)
{
    float s = r * 0.5f;
    switch (icon) {
    case ICON_JUMP:
        draw_line({ c.x - s * 0.85f, c.y + s * 0.8f }, { c.x + s * 0.85f, c.y + s * 0.8f }, 1.5f, col);
        draw_line({ c.x, c.y + s * 0.5f }, { c.x, c.y - s * 0.85f }, 2.0f, col);
        draw_line({ c.x - s * 0.65f, c.y - s * 0.2f }, { c.x, c.y - s * 0.9f }, 1.8f, col);
        draw_line({ c.x + s * 0.65f, c.y - s * 0.2f }, { c.x, c.y - s * 0.9f }, 1.8f, col);
        break;
    case ICON_ATTACK:
        draw_line({ c.x - s * 0.7f, c.y + s * 0.7f }, { c.x + s * 0.85f, c.y - s * 0.85f }, 2.2f, col);
        draw_line({ c.x + s * 0.6f, c.y - s * 0.95f }, { c.x + s * 0.95f, c.y - s * 0.6f }, 1.5f, col);
        draw_line({ c.x - s * 0.65f, c.y + s * 0.25f }, { c.x - s * 0.25f, c.y + s * 0.65f }, 2.0f, col);
        draw_circle({ c.x - s * 0.85f, c.y + s * 0.85f }, 1.3f, col);
        break;
    case ICON_DASH:
        draw_line({ c.x - s * 0.8f, c.y - s * 0.75f }, { c.x - s * 0.1f, c.y }, 2.0f, col);
        draw_line({ c.x - s * 0.8f, c.y + s * 0.75f }, { c.x - s * 0.1f, c.y }, 2.0f, col);
        draw_line({ c.x + s * 0.05f, c.y - s * 0.75f }, { c.x + s * 0.75f, c.y }, 2.0f, col);
        draw_line({ c.x + s * 0.05f, c.y + s * 0.75f }, { c.x + s * 0.75f, c.y }, 2.0f, col);
        break;
    case ICON_ECLAT:
        draw_circle(c, 2.0f, col);
        draw_line({ c.x - s * 0.95f, c.y }, { c.x + s * 0.95f, c.y }, 1.8f, col);
        draw_line({ c.x, c.y - s * 0.95f }, { c.x, c.y + s * 0.95f }, 1.8f, col);
        draw_line({ c.x - s * 0.6f, c.y - s * 0.6f }, { c.x + s * 0.6f, c.y + s * 0.6f }, 1.3f, col);
        draw_line({ c.x - s * 0.6f, c.y + s * 0.6f }, { c.x + s * 0.6f, c.y - s * 0.6f }, 1.3f, col);
        break;
    case ICON_SWAP:
        draw_line({ c.x - s * 0.75f, c.y - s * 0.35f }, { c.x + s * 0.65f, c.y - s * 0.35f }, 1.6f, col);
        draw_line({ c.x + s * 0.2f, c.y - s * 0.8f }, { c.x + s * 0.75f, c.y - s * 0.35f }, 1.6f, col);
        draw_line({ c.x + s * 0.2f, c.y + s * 0.1f }, { c.x + s * 0.75f, c.y - s * 0.35f }, 1.6f, col);
        draw_line({ c.x + s * 0.75f, c.y + s * 0.35f }, { c.x - s * 0.65f, c.y + s * 0.35f }, 1.6f, col);
        draw_line({ c.x - s * 0.2f, c.y + s * 0.8f }, { c.x - s * 0.75f, c.y + s * 0.35f }, 1.6f, col);
        draw_line({ c.x - s * 0.2f, c.y - s * 0.1f }, { c.x - s * 0.75f, c.y + s * 0.35f }, 1.6f, col);
        break;
    case ICON_MENU:
        draw_rect({ c.x - 4.2f, c.y - r * 0.4f, 2.0f, r * 0.8f }, col);
        draw_rect({ c.x + 2.2f, c.y - r * 0.4f, 2.0f, r * 0.8f }, col);
        break;
    }
}

static void draw_btn(const VirtualButton& btn)
{
    Color ring = btn.down ? rgb(0xffd166, 220) : rgb(0xffffff, 85);
    Color fill = btn.down ? rgb(0xff9a3c, 130) : rgb(0x181224, 75);
    Color icon_col = btn.down ? rgb(0xfff4d0, 255) : rgb(0xffffff, 210);
    draw_circle(btn.pos, btn.radius, fill);
    draw_circle_lines(btn.pos, btn.radius, 1.4f, ring);
    draw_circle_lines(btn.pos, btn.radius * 0.82f, 1.0f, fade(ring, 0.4f));
    draw_btn_icon(btn.icon, btn.pos, btn.radius, icon_col);
}

void touch_draw()
{
    if (!touch_enabled)
        return;

    if (touch_gameplay_enabled) {
        Color stick_ring = stick_active ? rgb(0xffd166, 170) : rgb(0xffffff, 60);
        draw_circle_lines(stick_anchor, STICK_RADIUS, 1.5f, stick_ring);
        draw_circle_lines(stick_anchor, STICK_RADIUS * 0.4f, 1.0f, fade(stick_ring, 0.4f));

        Color arrow_col = fade(stick_ring, 0.6f);
        float pad = STICK_RADIUS * 0.72f;
        static const float dirs[4][2] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
        for (const auto& d : dirs) {
            vec2 tip = { stick_anchor.x + d[0] * pad, stick_anchor.y + d[1] * pad };
            vec2 base = { tip.x - d[0] * 3.0f, tip.y - d[1] * 3.0f };
            vec2 perp = { -d[1] * 3.0f, d[0] * 3.0f };
            draw_line({ base.x - perp.x, base.y - perp.y }, tip, 1.3f, arrow_col);
            draw_line({ base.x + perp.x, base.y + perp.y }, tip, 1.3f, arrow_col);
        }

        Color knob_fill = stick_active ? rgb(0xffa834, 180) : rgb(0xffffff, 80);
        draw_circle(stick_pos, 10.0f, knob_fill);
        draw_circle_lines(stick_pos, 10.0f, 1.2f, WHITE);
    }

    for (int b = 0; b < BUTTON_COUNT; b++) {
        if (!touch_gameplay_enabled && b != TOUCH_BTN_MENU)
            continue;
        if (!all_buttons[b].enabled)
            continue;
        draw_btn(all_buttons[b]);
    }
}
