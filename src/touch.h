#pragma once
#include "platform.h"

// Touch controls subsystem for mobile / Android.
// Translates touches into virtual pad / buttons and renders on-screen HUD.

enum TouchButtonId {
    TOUCH_BTN_JUMP,
    TOUCH_BTN_ATTACK,
    TOUCH_BTN_DASH,
    TOUCH_BTN_ECLAT,
    TOUCH_BTN_SWAP,
    TOUCH_BTN_MENU,
    TOUCH_BTN_COUNT
};

void touch_init();
void touch_set_enabled(bool enabled);
bool touch_is_enabled();

// Controls whether gameplay controls (joystick & action buttons) are active
void touch_set_gameplay_enabled(bool enabled);
bool touch_is_gameplay_enabled();

// Controls individual button visibility & interaction
void touch_set_button_enabled(TouchButtonId btn, bool enabled);
bool touch_is_button_enabled(TouchButtonId btn);

// Translates active touches (or simulated mouse) into virtual pad stick & buttons
void touch_update(Input* in);

// Renders on-screen virtual stick and buttons when touch is active
void touch_draw();
