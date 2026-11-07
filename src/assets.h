#pragma once
#include <string>
#include <vector>
#include "audio.h"
#include "json.h"
#include "render.h"

struct Anim {
    std::string name;
    float fps = 8;
    bool loop = true;
    std::vector<int> frames;
};

struct SpriteSheet {
    std::string name;
    int w = 0, h = 0;
    vec2 origin;
    std::vector<Rect> frames;           // in the atlas
    std::vector<std::string> frame_names;
    std::vector<Anim> anims;

    int anim(const char* name) const;   // -1 if missing
    int frame(const char* name) const;
};

// Dev: if src_dir exists the assets are cooked in memory from source, and
// assets_poll() recooks when a file changes. Shipping: loaded from the pak.
bool assets_init(const char* src_dir, const char* pak_path);
void assets_shutdown();
bool assets_dev();
bool assets_poll();                     // true when something got reloaded
int assets_generation();                // bumps on every reload
const std::string& assets_log();        // cook errors, empty when all good

const std::vector<uint8_t>* asset_data(const std::string& name);
std::string asset_text(const std::string& name);
bool asset_json(const std::string& name, Json* out);
std::vector<std::string> asset_list(const std::string& prefix);     // names under "scenes/" etc

// ids are stable across reloads, they're handed out by name
int sprite_id(const char* name);        // -1 if there's no such sprite
const SpriteSheet* sprite_get(int id);
const std::vector<SpriteSheet>& sprite_all();
Texture atlas_texture();

Sound sfx(const char* name);
Sound music(const char* name);

// draws frame of sprite with its origin at pos
void draw_spr(int sprite, int frame, vec2 pos, bool flip_x = false, Color tint = WHITE, float flash = 0);
void draw_spr_ex(int sprite, int frame, vec2 pos, const SpriteOpts& o);
