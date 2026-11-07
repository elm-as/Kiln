#include "cook.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include "fs.h"
#include "image.h"
#include "json.h"

struct SpriteSrc {
    std::string name;
    int w = 0, h = 0;
    int ox = 0, oy = 0;
    std::vector<std::vector<uint8_t>> frames;   // rgba
    std::vector<std::string> frame_names;
    Json anims = Json::object();
};

typedef std::map<char, uint32_t> Palette;

void cook_put32(std::vector<uint8_t>& v, uint32_t x);
bool cook_parse_palette_line(const std::vector<std::string>& t, Palette& pal);
std::vector<std::string> cook_words(const std::string& line);
bool cook_is_comment(const std::string& line);
bool cook_parse_pix(const std::string& text, const Palette& base, SpriteSrc& s, std::string& err);

static std::vector<uint8_t> cut(const uint8_t* px, int iw, int ih, int x, int y, int w, int h)
{
    std::vector<uint8_t> f(size_t(w) * h * 4, 0);
    for (int row = 0; row < h; row++) {
        if (y + row < 0 || y + row >= ih)
            continue;
        for (int col = 0; col < w; col++) {
            if (x + col < 0 || x + col >= iw)
                continue;
            memcpy(&f[(size_t(row) * w + col) * 4], px + (size_t(y + row) * iw + x + col) * 4, 4);
        }
    }
    return f;
}

// anim frames given as indices or frame names
static bool json_frames(const SpriteSrc& s, const Json& list, Json& out, std::string& err)
{
    out = Json::array();
    for (const Json& f : list.arr) {
        int index = -1;
        if (f.is_str()) {
            auto it = std::find(s.frame_names.begin(), s.frame_names.end(), f.s);
            if (it == s.frame_names.end()) {
                err = "no frame called '" + f.s + "'";
                return false;
            }
            index = int(it - s.frame_names.begin());
        } else {
            index = f.i(-1);
        }
        if (index < 0 || index >= (int)s.frames.size()) {
            err = "anim frame " + std::to_string(index) + " out of range, there are " + std::to_string(s.frames.size());
            return false;
        }
        out.push(Json::number(index));
    }
    return true;
}

static bool load_grid(const uint8_t* px, int iw, int ih, const Json& meta, SpriteSrc& s, std::string& err)
{
    if (meta["size"].size() == 2) {
        s.w = meta["size"][0].i();
        s.h = meta["size"][1].i();
    }
    if (s.w <= 0 || s.h <= 0 || iw % s.w || ih % s.h) {
        err = "image size isn't a multiple of the frame size";
        return false;
    }
    s.ox = meta["origin"][0].i();
    s.oy = meta["origin"][1].i();
    for (int fy = 0; fy < ih / s.h; fy++) {
        for (int fx = 0; fx < iw / s.w; fx++) {
            size_t n = s.frames.size();
            const Json& name = meta["names"][(int)n];
            s.frame_names.push_back(name.is_str() ? name.s : std::to_string(n));
            s.frames.push_back(cut(px, iw, ih, fx * s.w, fy * s.h, s.w, s.h));
        }
    }
    for (const auto& kv : meta["anims"].obj) {
        Json anim = Json::object();
        anim.set("fps", Json::number(kv.second["fps"].f(8)));
        anim.set("loop", Json::boolean(kv.second["loop"].boolean_or(true)));
        Json frames;
        if (!json_frames(s, kv.second["frames"], frames, err)) {
            err = "anim " + kv.first + ": " + err;
            return false;
        }
        anim.set("frames", frames);
        s.anims.set(kv.first.c_str(), anim);
    }
    return true;
}

static bool load_aseprite(const uint8_t* px, int iw, int ih, const Json& meta, SpriteSrc& s, std::string& err)
{
    // the frames come as an array or as an object keyed by file name
    std::vector<std::pair<std::string, const Json*>> frames;
    if (meta["frames"].is_arr()) {
        for (const Json& f : meta["frames"].arr)
            frames.push_back({ f["filename"].str(""), &f });
    } else {
        for (const auto& kv : meta["frames"].obj)
            frames.push_back({ kv.first, &kv.second });
    }
    if (frames.empty()) {
        err = "aseprite json without frames";
        return false;
    }

    // every frame gets the untrimmed size, the trimmed pixels go back where
    // they were in it
    const Json& first = *frames[0].second;
    s.w = first["sourceSize"]["w"].i(first["frame"]["w"].i());
    s.h = first["sourceSize"]["h"].i(first["frame"]["h"].i());
    if (s.w <= 0 || s.h <= 0) {
        err = "invalid aseprite frame size";
        return false;
    }
    std::vector<int> ms;
    for (auto& f : frames) {
        const Json& j = *f.second;
        const Json& r = j["frame"];
        int fx = r["x"].i(), fy = r["y"].i(), fw = r["w"].i(), fh = r["h"].i();
        int dx = j["spriteSourceSize"]["x"].i(0), dy = j["spriteSourceSize"]["y"].i(0);
        if (fx < 0 || fy < 0 || fw <= 0 || fh <= 0 || fx + fw > iw || fy + fh > ih) {
            err = "frame " + f.first + " is outside the image";
            return false;
        }
        std::vector<uint8_t> trimmed = cut(px, iw, ih, fx, fy, fw, fh);
        std::vector<uint8_t> full(size_t(s.w) * s.h * 4, 0);
        for (int y = 0; y < fh && dy + y < s.h; y++) {
            if (dy + y < 0) continue;
            for (int x = 0; x < fw && dx + x < s.w; x++) {
                if (dx + x < 0) continue;
                memcpy(&full[(size_t(dy + y) * s.w + dx + x) * 4], &trimmed[(size_t(y) * fw + x) * 4], 4);
            }
        }
        s.frame_names.push_back(std::to_string(s.frames.size()));
        s.frames.push_back(std::move(full));
        ms.push_back(j["duration"].i(100));
    }

    const Json& slice = meta["meta"]["slices"][0]["keys"][0];
    if (slice["pivot"].is_obj()) {
        s.ox = slice["bounds"]["x"].i() + slice["pivot"]["x"].i();
        s.oy = slice["bounds"]["y"].i() + slice["pivot"]["y"].i();
    } else if (meta["origin"].size() == 2) {
        s.ox = meta["origin"][0].i();
        s.oy = meta["origin"][1].i();
    } else {
        s.ox = s.w / 2;
        s.oy = s.h;
    }

    for (const Json& tag : meta["meta"]["frameTags"].arr) {
        int from = tag["from"].i(), to = tag["to"].i();
        if (from < 0 || to >= (int)s.frames.size() || from > to) {
            err = "tag " + std::string(tag["name"].str("")) + " points past the frames";
            return false;
        }
        std::vector<int> order;
        for (int k = from; k <= to; k++)
            order.push_back(k);
        std::string dir = tag["direction"].str("forward");
        if (dir == "reverse") {
            std::reverse(order.begin(), order.end());
        } else if (dir == "pingpong") {
            for (int k = to - 1; k > from; k--)
                order.push_back(k);
        }
        Json list = Json::array();
        for (int k : order)
            list.push(Json::number(k));
        Json anim = Json::object();
        anim.set("fps", Json::number(1000.0 / std::max(1, ms[from])));
        // newer Aseprite writes "repeat": "1" for a tag that plays once
        anim.set("loop", Json::boolean(tag["repeat"].str("0") != std::string("1")));
        anim.set("frames", list);
        s.anims.set(tag["name"].str("anim"), anim);
    }
    return true;
}

static bool load_image_sprite(const std::string& path, SpriteSrc& s, std::string& err)
{
    int iw, ih;
    uint8_t* px = image_load(path.c_str(), &iw, &ih);
    if (!px) {
        err = "can't decode image";
        return false;
    }
    s.w = iw;
    s.h = ih;

    std::string side = path.substr(0, path.size() - path_ext(path).size()) + ".json";
    std::string text;
    Json meta = Json::object();
    if (read_text(side.c_str(), text)) {
        std::string jerr;
        if (!json_parse(text, &meta, &jerr)) {
            free(px);
            err = path_stem(side) + ".json: " + jerr;
            return false;
        }
    }
    bool ok = meta["meta"].is_obj() ? load_aseprite(px, iw, ih, meta, s, err) : load_grid(px, iw, ih, meta, s, err);
    free(px);
    return ok;
}

struct PackItem {
    int sprite, frame;
    int w, h;
    int x, y;
};

// shelf packer: tallest first, fill rows left to right
static void pack(std::vector<PackItem>& items, int* out_w, int* out_h)
{
    std::sort(items.begin(), items.end(), [](const PackItem& a, const PackItem& b) {
        return a.h != b.h ? a.h > b.h : a.w > b.w;
    });

    long area = 0;
    int widest = 0;
    for (const PackItem& it : items) {
        area += long(it.w + 1) * (it.h + 1);
        widest = std::max(widest, it.w + 1);
    }
    int w = 128;
    while (long(w) * w < area * 5 / 4 || w < widest)
        w *= 2;

    int x = 1, y = 1, shelf = 0;
    for (PackItem& it : items) {
        if (x + it.w + 1 > w) { x = 1; y += shelf + 1; shelf = 0; }
        it.x = x; it.y = y;
        x += it.w + 1;
        shelf = std::max(shelf, it.h);
    }
    *out_w = w;
    *out_h = (y + shelf + 1 + 3) & ~3;
}

bool cook_sprites(const std::string& dir, Pak* out, std::string* log)
{
    bool ok = true;
    Palette base;
    base['.'] = 0;
    std::string pal_text;
    if (read_text(path_join(dir, "palette.txt").c_str(), pal_text)) {
        std::istringstream in(pal_text);
        std::string line;
        int n = 0;
        while (std::getline(in, line)) {
            n++;
            if (cook_is_comment(line))
                continue;
            std::vector<std::string> t = cook_words(line);
            if (!t.empty() && !cook_parse_palette_line(t, base)) {
                *log += "sprites/palette.txt: line " + std::to_string(n) + ": expected '<char> <#rrggbb>'\n";
                ok = false;
            }
        }
    }

    std::vector<SpriteSrc> sprites;
    for (const std::string& f : list_files(dir.c_str())) {
        std::string ext = path_ext(f);
        std::string path = path_join(dir, f);
        SpriteSrc s;
        s.name = path_stem(f);
        std::string err;
        bool good;
        if (ext == ".pix") {
            std::string text;
            read_text(path.c_str(), text);
            good = cook_parse_pix(text, base, s, err);
        } else if (ext == ".png" || ext == ".tga") {
            good = load_image_sprite(path, s, err);
        } else {
            continue;
        }
        if (!good) {
            *log += "sprites/" + f + ": " + err + "\n";
            ok = false;
            continue;
        }
        sprites.push_back(std::move(s));
    }

    std::vector<PackItem> items;
    for (size_t i = 0; i < sprites.size(); i++)
        for (size_t k = 0; k < sprites[i].frames.size(); k++)
            items.push_back({ (int)i, (int)k, sprites[i].w, sprites[i].h, 0, 0 });

    int aw, ah;
    pack(items, &aw, &ah);

    std::vector<uint8_t> atlas(size_t(aw) * ah * 4, 0);
    std::vector<std::vector<std::pair<int, int>>> where(sprites.size());
    for (size_t i = 0; i < sprites.size(); i++)
        where[i].resize(sprites[i].frames.size());

    for (const PackItem& it : items) {
        const std::vector<uint8_t>& px = sprites[it.sprite].frames[it.frame];
        for (int y = 0; y < it.h; y++)
            memcpy(&atlas[(size_t(it.y + y) * aw + it.x) * 4], &px[size_t(y) * it.w * 4], size_t(it.w) * 4);
        where[it.sprite][it.frame] = { it.x, it.y };
    }

    Json meta = Json::object();
    meta.set("w", Json::number(aw));
    meta.set("h", Json::number(ah));
    Json& js = meta.set("sprites", Json::object());
    for (size_t i = 0; i < sprites.size(); i++) {
        const SpriteSrc& s = sprites[i];
        Json j = Json::object(), size = Json::array(), origin = Json::array(), frames = Json::array(), names = Json::array();
        size.push(Json::number(s.w)); size.push(Json::number(s.h));
        origin.push(Json::number(s.ox)); origin.push(Json::number(s.oy));
        for (auto& xy : where[i]) {
            Json p = Json::array();
            p.push(Json::number(xy.first)); p.push(Json::number(xy.second));
            frames.push(p);
        }
        for (const std::string& n : s.frame_names)
            names.push(Json::string(n));
        j.set("size", size); j.set("origin", origin);
        j.set("frames", frames); j.set("names", names); j.set("anims", s.anims);
        js.set(s.name.c_str(), j);
    }

    std::vector<uint8_t> blob;
    cook_put32(blob, aw);
    cook_put32(blob, ah);
    blob.insert(blob.end(), atlas.begin(), atlas.end());
    out->files["atlas.rgba"] = std::move(blob);
    std::string mj = json_write(meta, false);
    out->files["atlas.json"].assign(mj.begin(), mj.end());
    return ok;
}
