// XInput gamepad polling for Win32, dynamically loaded so no XInput DLL is required at startup.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <cstring>
#include "platform.h"

struct XPad {
    WORD buttons;
    BYTE lt, rt;
    SHORT lx, ly, rx, ry;
};

struct XState {
    DWORD packet;
    XPad pad;
};

typedef DWORD (WINAPI *XInputGetStateFn)(DWORD, XState*);
static XInputGetStateFn xinput_get_state;
static double pad_retry_at;
static bool pad_was_connected;

void gamepad_init_win32()
{
    const char* names[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (const char* n : names) {
        HMODULE lib = LoadLibraryA(n);
        if (lib) {
            xinput_get_state = (XInputGetStateFn)(void*)GetProcAddress(lib, "XInputGetState");
            if (xinput_get_state)
                return;
        }
    }
}

static float stick(SHORT v, float dead)
{
    float f = v / 32767.f;
    if (f < -1) f = -1;
    if (fabsf(f) < dead) return 0;
    return (f - (f > 0 ? dead : -dead)) / (1 - dead);
}

void gamepad_poll_win32(Input* in)
{
    if (!xinput_get_state)
        return;
    // XInputGetState on an empty slot takes ages, so only look for a new pad
    // every couple of seconds
    double now = platform_time();
    if (!pad_was_connected && now < pad_retry_at)
        return;

    XState st;
    if (xinput_get_state(0, &st) != ERROR_SUCCESS) {
        if (pad_was_connected)
            memset(in->pad_down, 0, sizeof(in->pad_down));
        pad_was_connected = false;
        in->pad_connected = false;
        in->pad_lx = in->pad_ly = 0;
        pad_retry_at = now + 2;
        return;
    }
    pad_was_connected = true;
    in->pad_connected = true;

    static const WORD bits[PAD_COUNT] = {
        0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0020, 0x0010,
        0x0001, 0x0002, 0x0004, 0x0008,
    };
    for (int i = 0; i < PAD_COUNT; i++) {
        bool d = (st.pad.buttons & bits[i]) != 0;
        if (d && !in->pad_down[i])
            in->pad_pressed[i] = true;
        in->pad_down[i] = d;
    }
    in->pad_lx = stick(st.pad.lx, 0.25f);
    in->pad_ly = -stick(st.pad.ly, 0.25f);
}

