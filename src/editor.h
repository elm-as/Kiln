#pragma once
#include <string>
#include "platform.h"
#include "scene.h"

struct EditorHooks {
    // draw the scene the way the game would, in world coordinates. The editor
    // has already set up the view.
    void (*draw_world)(Scene& s, Rect view);
    const Tileset* tileset;
};

void editor_init(const EditorHooks& hooks);
void editor_open(Scene* s, const std::string& save_path);
void editor_close();
bool editor_is_open();

void editor_frame(const Input& in, float dt);   // input, camera, tools
void editor_draw();                             // call during the screen pass

vec2 editor_camera_center();
void editor_focus(vec2 world);
void editor_fit();
