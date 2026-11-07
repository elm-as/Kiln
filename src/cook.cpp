#include "cook.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include "fs.h"
#include "json.h"

bool cook_sprites(const std::string& dir, Pak* out, std::string* log);
bool cook_sounds(const std::string& dir, Pak* out, std::string* log);
bool cook_music(const std::string& dir, Pak* out, std::string* log);
bool cook_scripts(const std::string& dir, Pak* out, std::string* log);
bool cook_json(const std::string& dir, const std::string& prefix, Pak* out, std::string* log);

void cook_put32(std::vector<uint8_t>& v, uint32_t x)
{
    for (int i = 0; i < 4; i++)
        v.push_back(uint8_t(x >> (i * 8)));
}

uint32_t cook_get32(const uint8_t* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24;
}

bool pak_write(const Pak& pak, const char* path)
{
    std::vector<uint8_t> out;
    out.insert(out.end(), { 'K', 'P', 'A', 'K' });
    cook_put32(out, 1);
    cook_put32(out, (uint32_t)pak.files.size());

    size_t header = out.size();
    for (const auto& f : pak.files)
        header += 2 + f.first.size() + 8;

    uint32_t offset = (uint32_t)header;
    for (const auto& f : pak.files) {
        out.push_back(uint8_t(f.first.size()));
        out.push_back(uint8_t(f.first.size() >> 8));
        out.insert(out.end(), f.first.begin(), f.first.end());
        cook_put32(out, offset);
        cook_put32(out, (uint32_t)f.second.size());
        offset += (uint32_t)f.second.size();
    }
    for (const auto& f : pak.files)
        out.insert(out.end(), f.second.begin(), f.second.end());
    return write_file(path, out.data(), out.size());
}

bool pak_read(const char* path, Pak* pak)
{
    std::vector<uint8_t> d;
    if (!read_file(path, d) || d.size() < 12 || memcmp(d.data(), "KPAK", 4) != 0)
        return false;
    if (cook_get32(&d[4]) != 1) {
        fprintf(stderr, "%s: wrong pak version\n", path);
        return false;
    }
    uint32_t count = cook_get32(&d[8]);
    size_t pos = 12;
    pak->files.clear();
    for (uint32_t i = 0; i < count; i++) {
        if (pos + 2 > d.size())
            return false;
        size_t len = d[pos] | d[pos + 1] << 8;
        pos += 2;
        if (pos + len + 8 > d.size())
            return false;
        std::string name((const char*)&d[pos], len);
        pos += len;
        uint32_t off = cook_get32(&d[pos]);
        uint32_t size = cook_get32(&d[pos + 4]);
        pos += 8;
        if (size_t(off) + size > d.size())
            return false;
        pak->files[name].assign(d.begin() + off, d.begin() + off + size);
    }
    return true;
}

std::vector<uint8_t> pcm_pack(int rate, const std::vector<int16_t>& samples)
{
    std::vector<uint8_t> out = { 'P', 'C', 'M', '1' };
    cook_put32(out, rate);
    cook_put32(out, (uint32_t)samples.size());
    size_t at = out.size();
    out.resize(at + samples.size() * 2);
    if (!samples.empty())
        memcpy(&out[at], samples.data(), samples.size() * 2);
    return out;
}

// ---------------------------------------------------------------------------
// cross checks: names that point at things that don't exist. Each of these
// used to show up as a silently missing sprite or sound at runtime.

static Json pak_json(const Pak& pak, const std::string& name)
{
    Json j;
    auto it = pak.files.find(name);
    if (it != pak.files.end()) {
        std::string err;
        json_parse(std::string(it->second.begin(), it->second.end()), &j, &err);
    }
    return j;
}

static bool has_file(const Pak& pak, const std::string& name)
{
    return pak.files.count(name) != 0;
}

// string literals passed to f( in a script, like sfx("jump")
static std::vector<std::pair<std::string, int>> literal_args(const std::string& src, const char* f)
{
    std::vector<std::pair<std::string, int>> out;
    std::string pat = std::string(f) + "(\"";
    size_t pos = 0;
    while ((pos = src.find(pat, pos)) != std::string::npos) {
        // don't match inside a longer name, like my_sfx(
        if (pos > 0 && (isalnum((unsigned char)src[pos - 1]) || src[pos - 1] == '_')) {
            pos++;
            continue;
        }
        size_t start = pos + pat.size();
        size_t end = src.find('"', start);
        if (end == std::string::npos)
            break;
        int line = 1 + (int)std::count(src.begin(), src.begin() + pos, '\n');
        out.push_back({ src.substr(start, end - start), line });
        pos = end;
    }
    return out;
}

static bool check_references(const Pak& pak, std::string* log)
{
    bool ok = true;
    auto bad = [&](const std::string& msg) {
        *log += msg + "\n";
        ok = false;
    };

    Json atlas = pak_json(pak, "atlas.json");
    const Json& sprites = atlas["sprites"];
    Json prefabs = pak_json(pak, "data/prefabs.json");
    Json fx = pak_json(pak, "data/particles.json");
    Json tiles = pak_json(pak, "data/tileset.json");
    Json story = pak_json(pak, "data/story.json");
    const Json& dialogues = story["dialogues"];
    auto has_dialogue = [&](const char* id) { return !*id || dialogues.has(id); };

    for (const auto& d : dialogues.obj)
        for (const Json& line : d.second.arr)
            if (!story["characters"].has(line["who"].str()))
                bad("data/story.json: " + d.first + ": nobody called '" + line["who"].s + "'");
    for (const auto& c : story["cutscenes"].obj)
        for (const Json& page : c.second.arr)
            if (page["image"].is_str() && !sprites.has(page["image"].str()))
                bad("data/story.json: " + c.first + ": no sprite called '" + page["image"].s + "'");

    auto has_frame = [&](const Json& sp, const std::string& f) {
        for (const Json& n : sp["names"].arr)
            if (n.s == f)
                return true;
        return false;
    };

    for (const auto& pf : prefabs.obj) {
        if (pf.first[0] == '_')
            continue;
        std::string where = "data/prefabs.json: " + pf.first + ": ";
        const Json& c = pf.second;
        const Json& spr = c["Sprite"];
        if (spr["sprite"].is_str()) {
            const Json& sp = sprites[spr["sprite"].str()];
            if (!sp.is_obj())
                bad(where + "no sprite called '" + spr["sprite"].s + "'");
            else if (spr["frame"].is_str() && !has_frame(sp, spr["frame"].s))
                bad(where + "sprite " + spr["sprite"].s + " has no frame '" + spr["frame"].s + "'");
            else if (c["Animator"]["play"].is_str() && !sp["anims"].has(c["Animator"]["play"].str()))
                bad(where + "sprite " + spr["sprite"].s + " has no anim '" + c["Animator"]["play"].s + "'");
        }
        if (c["Script"]["module"].is_str() && !has_file(pak, "scripts/" + c["Script"]["module"].s + ".ks"))
            bad(where + "no script called '" + c["Script"]["module"].s + "'");
        if (c["Emitter"]["fx"].is_str() && !fx.has(c["Emitter"]["fx"].str()))
            bad(where + "no particle effect called '" + c["Emitter"]["fx"].s + "'");
    }

    for (const auto& t : tiles["tiles"].obj) {
        const Json& sp = sprites[t.second["sprite"].str()];
        std::string where = "data/tileset.json: '" + t.first + "': ";
        if (!sp.is_obj()) {
            bad(where + "no sprite called '" + t.second["sprite"].s + "'");
            continue;
        }
        for (const Json& f : t.second["frames"].arr)
            if (f.is_str() && !has_frame(sp, f.s))
                bad(where + "no frame '" + f.s + "'");
        if (t.second["top"].is_str() && !has_frame(sp, t.second["top"].s))
            bad(where + "no frame '" + t.second["top"].s + "'");
        for (const Json& f : t.second["top"].arr)
            if (f.is_str() && !has_frame(sp, f.s))
                bad(where + "no frame '" + f.s + "'");
    }

    for (const auto& f : pak.files) {
        const std::string& name = f.first;
        if (!name.compare(0, 7, "scenes/")) {
            Json scene = pak_json(pak, name);
            if (!has_dialogue(scene["props"]["outro"].str("")))
                bad(name + ": no dialogue called '" + scene["props"]["outro"].s + "'");
            for (const Json& e : scene["entities"].arr) {
                const char* d = e["set"]["Script"]["props"]["dialogue"].str(e["props"]["dialogue"].str(""));
                if (!has_dialogue(d))
                    bad(name + ": no dialogue called '" + std::string(d) + "'");
            }
            const char* song = scene["props"]["music"].str("");
            if (*song && !has_file(pak, std::string("music/") + song + ".pcm"))
                bad(name + ": no music called '" + song + "'");
            int i = 0;
            for (const Json& e : scene["entities"].arr) {
                if (!prefabs.has(e["prefab"].str()))
                    bad(name + ": entity " + std::to_string(i) + " uses unknown prefab '" + e["prefab"].s + "'");
                i++;
            }
            // every row the same width, or the editor and the game disagree on the size
            for (const char* layer : { "tiles", "back" }) {
                const Json& rows = scene[layer];
                for (size_t r = 1; r < rows.size(); r++)
                    if (rows[r].s.size() != rows[0].s.size()) {
                        bad(name + ": " + layer + " row " + std::to_string(r) + " has a different width");
                        break;
                    }
            }
        } else if (!name.compare(0, 8, "scripts/")) {
            std::string src(f.second.begin(), f.second.end());
            for (auto& a : literal_args(src, "sfx"))
                if (!has_file(pak, "sfx/" + a.first + ".pcm"))
                    bad(name + ":" + std::to_string(a.second) + ": no sound called '" + a.first + "'");
            for (auto& a : literal_args(src, "burst"))
                if (!fx.has(a.first.c_str()))
                    bad(name + ":" + std::to_string(a.second) + ": no particle effect called '" + a.first + "'");
            for (auto& a : literal_args(src, "talk"))
                if (!has_dialogue(a.first.c_str()))
                    bad(name + ":" + std::to_string(a.second) + ": no dialogue called '" + a.first + "'");
            for (auto& a : literal_args(src, "spawn"))
                if (!prefabs.has(a.first.c_str()))
                    bad(name + ":" + std::to_string(a.second) + ": no prefab called '" + a.first + "'");
        }
    }
    return ok;
}

bool cook_assets(const std::string& src, Pak* out, std::string* log)
{
    out->files.clear();
    bool ok = true;
    ok &= cook_sprites(path_join(src, "sprites"), out, log);
    ok &= cook_sounds(path_join(src, "sounds"), out, log);
    ok &= cook_music(path_join(src, "music"), out, log);
    ok &= cook_scripts(path_join(src, "scripts"), out, log);
    ok &= cook_json(path_join(src, "scenes"), "scenes/", out, log);
    ok &= cook_json(path_join(src, "data"), "data/", out, log);
    ok &= check_references(*out, log);
    return ok;
}

int64_t assets_newest_mtime(const std::string& src)
{
    static const char* dirs[] = { "sprites", "sounds", "music", "scripts", "scenes", "data" };
    int64_t newest = 0;
    for (const char* d : dirs) {
        std::string dir = path_join(src, d);
        for (const std::string& f : list_files(dir.c_str()))
            newest = std::max(newest, file_mtime(path_join(dir, f).c_str()));
    }
    return newest;
}
