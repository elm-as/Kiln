#pragma once
#include <cstdint>

enum Key {
    KEY_NONE,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
    KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_SPACE, KEY_ENTER, KEY_ESCAPE, KEY_TAB, KEY_BACKSPACE, KEY_DELETE,
    KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN,
    KEY_SHIFT, KEY_CTRL, KEY_ALT,
    KEY_COUNT
};

enum MouseButton { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_COUNT };

enum PadButton {
    PAD_A, PAD_B, PAD_X, PAD_Y, PAD_LB, PAD_RB, PAD_BACK, PAD_START,
    PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
    PAD_COUNT
};

const int MAX_TOUCHES = 8;
enum TouchPhase { TOUCH_DOWN, TOUCH_MOVE, TOUCH_UP, TOUCH_CANCEL };

struct TouchPoint {
    int id = -1;
    float x = 0, y = 0;     // canvas coords, filled in by the engine
    float px = 0, py = 0;   // window pixels
    TouchPhase phase = TOUCH_CANCEL;
};

struct Input {
    bool down[KEY_COUNT];
    bool pressed[KEY_COUNT];
    bool released[KEY_COUNT];

    float mouse_x, mouse_y;     // canvas coords, filled in by the engine
    float mouse_px, mouse_py;   // window pixels
    float wheel;
    bool mouse_down[MOUSE_COUNT];
    bool mouse_pressed[MOUSE_COUNT];
    bool mouse_released[MOUSE_COUNT];

    char text[32];              // characters typed since last time, for text fields
    int text_len;

    bool pad_connected;
    bool pad_down[PAD_COUNT];
    bool pad_pressed[PAD_COUNT];
    float pad_lx, pad_ly;       // -1..1, deadzone already applied, y down

    TouchPoint touches[MAX_TOUCHES];
    int touch_count = 0;
};

bool platform_init(const char* title, int w, int h);
void platform_shutdown();
bool platform_poll(Input* in);  // only adds edges, clearing them is the caller's job
void platform_swap();
void platform_window_size(int* w, int* h);
void platform_screen_size(int* w, int* h);
void platform_toggle_fullscreen();
bool platform_fullscreen();
void platform_set_title(const char* title);
double platform_time();
void platform_sleep(int ms);
