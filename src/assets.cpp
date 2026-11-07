#include "assets.h"
#include <cstdio>
#include <cstring>
#include <map>
#include "cook.h"
#include "fs.h"
#include "platform.h"

static Pak pak;
static std::string src;
static bool dev;
static int generation;
static std::string log_text;
static int64_t cooked_mtime;
static double next_poll;

static std::vector<SpriteSheet> sheets;     // index = id, never shrinks
static std::map<std::string, int> sheet_ids;
static Texture atlas;
static std::map<std::string, Sound> sfx_cache, music_cache;

int SpriteSheet::anim(const char* n) const
{
    for (size_t i = 0; i < anims.size(); i++)
        if (anims[i].name == n)
            return (int)i;
    return -1;
}

int SpriteSheet::frame(const char* n) const
{
    for (size_t i = 0; i < frame_names.size(); i++)
        if (frame_names[i] == n)
            return (int)i;
    return -1;
}

static void load_atlas()
{
    const std::vector<uint8_t>* px = asset_data("atlas.rgba");
    Json meta;
    if (!px || px->size() < 8 || !asset_json("atlas.json", &meta)) {
        fprintf(stderr, "assets: no atlas\n");
        return;
    }
    uint32_t w, h;
    memcpy(&w, px->data(), 4);
    memcpy(&h, px->data() + 4, 4);
    if (px->size() < 8 + size_t(w) * h * 4)
        return;

    texture_free(&atlas);
    atlas = texture_create((int)w, (int)h, px->data() + 8);

    // sprites that vanished keep their slot but lose their frames
    for (SpriteSheet& s : sheets) {
        s.frames.clear();
        s.anims.clear();
    }

    for (const auto& kv : meta["sprites"].obj) {
        const Json& j = kv.second;
        int id = sprite_id(kv.first.c_str());
        if (id < 0) {
            id = (int)sheets.size();
            sheets.emplace_back();
            sheet_ids[kv.first] = id;
        }
        SpriteSheet& s = sheets[id];
        s.name = kv.first;
        s.w = j["size"][0].i();
        s.h = j["size"][1].i();
        s.origin = { j["origin"][0].f(), j["origin"][1].f() };
        s.frames.clear();
        for (const Json& f : j["frames"].arr)
            s.frames.push_back({ f[0].f(), f[1].f(), (float)s.w, (float)s.h });
        s.frame_names.clear();
        for (const Json& n : j["names"].arr)
            s.frame_names.push_back(n.s);
        s.anims.clear();
        for (const auto& a : j["anims"].obj) {
            Anim an;
            an.name = a.first;
            an.fps = a.second["fps"].f(8);
            an.loop = a.second["loop"].boolean_or(true);
            for (const Json& f : a.second["frames"].arr)
                an.frames.push_back(f.i());
            s.anims.push_back(an);
        }
    }
}

static void loaded()
{
    load_atlas();
    sfx_cache.clear();
    music_cache.clear();
    generation++;
}

static bool cook_now()
{
    log_text.clear();
    Pak fresh;
    bool ok = cook_assets(src, &fresh, &log_text);
    if (!log_text.empty())
        fprintf(stderr, "%s", log_text.c_str());
    // keep going with whatever cooked, a typo in one sprite shouldn't take
    // down the whole game
    pak = std::move(fresh);
    return ok;
}

bool assets_init(const char* src_dir, const char* pak_path)
{
    if (src_dir && dir_exists(src_dir)) {
        dev = true;
        src = src_dir;
        double t0 = platform_time();
        cooked_mtime = assets_newest_mtime(src);
        cook_now();
        printf("assets: cooked %s in %.0f ms\n", src_dir, (platform_time() - t0) * 1000);
    } else if (!pak_read(pak_path, &pak)) {
        fprintf(stderr, "assets: couldn't open %s\n", pak_path);
        return false;
    }
    loaded();
    return true;
}

void assets_shutdown()
{
    texture_free(&atlas);
    sfx_cache.clear();
    music_cache.clear();
    pak.files.clear();
}

bool assets_dev() { return dev; }
int assets_generation() { return generation; }
const std::string& assets_log() { return log_text; }

bool assets_poll()
{
    if (!dev)
        return false;
    double now = platform_time();
    if (now < next_poll)
        return false;
    next_poll = now + 0.5;

    int64_t m = assets_newest_mtime(src);
    if (m == cooked_mtime)
        return false;
    cooked_mtime = m;
    double t0 = platform_time();
    cook_now();
    loaded();
    printf("assets: reloaded in %.0f ms\n", (platform_time() - t0) * 1000);
    return true;
}

const std::vector<uint8_t>* asset_data(const std::string& name)
{
    auto it = pak.files.find(name);
    return it == pak.files.end() ? 0 : &it->second;
}

std::string asset_text(const std::string& name)
{
    const std::vector<uint8_t>* d = asset_data(name);
    return d ? std::string(d->begin(), d->end()) : std::string();
}

bool asset_json(const std::string& name, Json* out)
{
    const std::vector<uint8_t>* d = asset_data(name);
    if (!d)
        return false;
    std::string err;
    if (!json_parse(std::string(d->begin(), d->end()), out, &err)) {
        fprintf(stderr, "%s: %s\n", name.c_str(), err.c_str());
        return false;
    }
    return true;
}

std::vector<std::string> asset_list(const std::string& prefix)
{
    std::vector<std::string> out;
    for (auto it = pak.files.lower_bound(prefix); it != pak.files.end(); ++it) {
        if (it->first.compare(0, prefix.size(), prefix) != 0)
            break;
        out.push_back(it->first.substr(prefix.size()));
    }
    return out;
}

int sprite_id(const char* name)
{
    auto it = sheet_ids.find(name);
    return it == sheet_ids.end() ? -1 : it->second;
}

const SpriteSheet* sprite_get(int id)
{
    if (id < 0 || id >= (int)sheets.size() || sheets[id].frames.empty())
        return 0;
    return &sheets[id];
}

const std::vector<SpriteSheet>& sprite_all()
{
    return sheets;
}

Texture atlas_texture()
{
    return atlas;
}

static Sound load_sound(std::map<std::string, Sound>& cache, const std::string& dir, const char* name)
{
    auto it = cache.find(name);
    if (it != cache.end())
        return it->second;
    Sound s;
    const std::vector<uint8_t>* d = asset_data(dir + name + ".pcm");
    if (d)
        s = sound_from_pcm(d->data(), d->size());
    else
        fprintf(stderr, "no sound called %s%s\n", dir.c_str(), name);
    cache[name] = s;    // cache misses too, so a typo only complains once
    return s;
}

Sound sfx(const char* name)
{
    return load_sound(sfx_cache, "sfx/", name);
}

Sound music(const char* name)
{
    return load_sound(music_cache, "music/", name);
}

void draw_spr(int sprite, int frame, vec2 pos, bool flip_x, Color tint, float flash)
{
    SpriteOpts o;
    o.flip_x = flip_x;
    o.tint = tint;
    o.flash = flash;
    const SpriteSheet* s = sprite_get(sprite);
    if (s)
        o.origin = s->origin;
    draw_spr_ex(sprite, frame, pos, o);
}

void draw_spr_ex(int sprite, int frame, vec2 pos, const SpriteOpts& o)
{
    const SpriteSheet* s = sprite_get(sprite);
    if (!s)
        return;
    if (frame < 0 || frame >= (int)s->frames.size())
        frame = 0;
    draw_sprite_ex(atlas, s->frames[frame], pos, o);
}
