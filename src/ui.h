#pragma once
#include <string>
#include "platform.h"
#include "render.h"

// Immediate mode widgets for the editor, drawn in window pixels during the
// screen pass. Panels are fixed rectangles with a vertical layout and wheel
// scrolling. Widget ids come from the label, so give things that share a
// label a "##something" suffix, or wrap them in ui_push_id.

void ui_init();
void ui_shutdown();
void ui_begin(const Input& in);
void ui_end();

bool ui_want_mouse();       // the mouse is over a panel or dragging a widget
bool ui_want_keyboard();    // a text field has focus

void ui_panel_begin(const char* title, Rect r);
void ui_panel_end();
Rect ui_panel_rect();
Rect ui_last_rect();         // where the last widget went, for drawing extras on it

void ui_push_id(const char* s);
void ui_push_id(int n);
void ui_pop_id();

void ui_same_line();
void ui_separator();
void ui_label(const char* fmt, ...);
void ui_label_colored(Color c, const char* fmt, ...);
bool ui_button(const char* label);
bool ui_selectable(const char* label, bool selected);
bool ui_checkbox(const char* label, bool* v);
bool ui_header(const char* label, bool default_open = true);
bool ui_drag_float(const char* label, float* v, float speed = 0.5f);
bool ui_drag_int(const char* label, int* v, float speed = 0.2f);
bool ui_drag_vec2(const char* label, vec2* v, float speed = 0.5f);
bool ui_input_text(const char* label, std::string* s);
bool ui_color(const char* label, Color* c);

// small text readout at a screen position, outside any panel
void ui_text_at(float x, float y, const char* s, Color c = WHITE);
const Font& ui_font();
