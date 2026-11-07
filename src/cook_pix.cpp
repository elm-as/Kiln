#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "json.h"

struct SpriteSrc {
    std::string name;
    int w = 0, h = 0;
    int ox = 0, oy = 0;
    std::vector<std::vector<uint8_t>> frames;   // rgba
    std::vector<std::string> frame_names;
    Json anims = Json::object();
};

typedef std::map<char, uint32_t> Palette;     // 0 means transparent, else 0xRRGGBBAA

static bool parse_color(const std::string& s, uint32_t* out)
{
    if (s == "none" || s == "clear") {
        *out = 0;
        return true;
    }
    const char* p = s.c_str();
    if (*p == '#')
        p++;
    if (strlen(p) != 6)
        return false;
    char* end;
    unsigned long v = strtoul(p, &end, 16);
    if (*end)
        return false;
    *out = uint32_t(v) << 8 | 0xff;
    return true;
}

bool cook_parse_palette_line(const std::vector<std::string>& t, Palette& pal)
{
    if (t.size() != 2 || t[0].size() != 1)
        return false;
    uint32_t c;
    if (!parse_color(t[1], &c))
        return false;
    pal[t[0][0]] = c;
    return true;
}

std::vector<std::string> cook_words(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string w;
    while (ss >> w)
        out.push_back(w);
    return out;
}

static std::string rstrip(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    return s;
}

bool cook_is_comment(const std::string& line)
{
    size_t i = line.find_first_not_of(" \t");
    return i == std::string::npos || (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/');
}

// anim frame lists: indices, ranges like 2-5, or frame names
static bool anim_frames(const SpriteSrc& s, const std::vector<std::string>& toks, size_t from, Json& out, std::string& err)
{
    out = Json::array();
    for (size_t i = from; i < toks.size(); i++) {
        const std::string& t = toks[i];
        size_t dash = t.find('-');
        if (isdigit((unsigned char)t[0]) && dash != std::string::npos) {
            int a = atoi(t.c_str()), b = atoi(t.c_str() + dash + 1);
            for (int k = a; a <= b ? k <= b : k >= b; k += a <= b ? 1 : -1)
                out.push(Json::number(k));
        } else if (isdigit((unsigned char)t[0])) {
            out.push(Json::number(atoi(t.c_str())));
        } else {
            auto it = std::find(s.frame_names.begin(), s.frame_names.end(), t);
            if (it == s.frame_names.end()) {
                err = "no frame called '" + t + "'";
                return false;
            }
            out.push(Json::number(double(it - s.frame_names.begin())));
        }
    }
    for (const Json& f : out.arr) {
        if (f.i() < 0 || f.i() >= (int)s.frames.size()) {
            err = "anim frame out of range";
            return false;
        }
    }
    return true;
}

// .pix is pixel art as text:
//
//   size 8 8
//   origin 4 8
//   palette             (optional, adds to / overrides sprites/palette.txt)
//   x #ff0044
//   frame idle
//   ..xxxx..
//   ...
//   anim idle 4 loop idle blink
bool cook_parse_pix(const std::string& text, const Palette& base, SpriteSrc& s, std::string& err)
{
    Palette pal = base;
    std::vector<std::string> lines;
    {
        std::istringstream in(text);
        std::string l;
        while (std::getline(in, l))
            lines.push_back(rstrip(l));
    }

    struct AnimLine { std::vector<std::string> toks; int line; };
    std::vector<AnimLine> anim_lines;
    bool in_palette = false;

    for (size_t i = 0; i < lines.size(); i++) {
        char where[32];
        snprintf(where, sizeof(where), "line %d: ", int(i + 1));
        if (cook_is_comment(lines[i]))
            continue;
        std::vector<std::string> t = cook_words(lines[i]);
        if (t.empty())
            continue;
        const std::string& k = t[0];

        if (k == "size" && t.size() == 3) {
            s.w = atoi(t[1].c_str());
            s.h = atoi(t[2].c_str());
            in_palette = false;
        } else if (k == "origin" && t.size() == 3) {
            s.ox = atoi(t[1].c_str());
            s.oy = atoi(t[2].c_str());
            in_palette = false;
        } else if (k == "palette" && t.size() == 1) {
            in_palette = true;
        } else if (k == "frame") {
            in_palette = false;
            if (s.w <= 0 || s.h <= 0) {
                err = where + std::string("frame before size");
                return false;
            }
            std::vector<uint8_t> px(size_t(s.w) * s.h * 4, 0);
            for (int y = 0; y < s.h; y++) {
                size_t li = i + 1 + y;
                if (li >= lines.size()) {
                    err = where + std::string("frame is missing rows");
                    return false;
                }
                const std::string& row = lines[li];
                if ((int)row.size() != s.w) {
                    snprintf(where, sizeof(where), "line %d: ", int(li + 1));
                    err = where + std::string("row is ") + std::to_string(row.size()) +
                          " wide, expected " + std::to_string(s.w);
                    return false;
                }
                for (int x = 0; x < s.w; x++) {
                    auto it = pal.find(row[x]);
                    if (it == pal.end()) {
                        snprintf(where, sizeof(where), "line %d: ", int(li + 1));
                        err = where + std::string("'") + row[x] + "' is not in the palette";
                        return false;
                    }
                    uint32_t c = it->second;
                    uint8_t* p = &px[(size_t(y) * s.w + x) * 4];
                    p[0] = uint8_t(c >> 24);
                    p[1] = uint8_t(c >> 16);
                    p[2] = uint8_t(c >> 8);
                    p[3] = uint8_t(c);
                }
            }
            s.frame_names.push_back(t.size() > 1 ? t[1] : std::to_string(s.frames.size()));
            s.frames.push_back(std::move(px));
            i += s.h;
        } else if (k == "anim") {
            in_palette = false;
            anim_lines.push_back({ t, int(i + 1) });
        } else if (in_palette && cook_parse_palette_line(t, pal)) {
            // ok
        } else {
            err = where + std::string("what is '") + k + "'?";
            return false;
        }
    }

    if (s.frames.empty()) {
        err = "no frames";
        return false;
    }

    // anims last so they can name frames defined further down
    for (const AnimLine& a : anim_lines) {
        if (a.toks.size() < 4) {
            err = "line " + std::to_string(a.line) + ": anim <name> <fps> <loop|once> frames...";
            return false;
        }
        Json anim = Json::object();
        anim.set("fps", Json::number(atof(a.toks[2].c_str())));
        anim.set("loop", Json::boolean(a.toks[3] == "loop"));
        Json frames;
        std::string ferr;
        if (!anim_frames(s, a.toks, 4, frames, ferr)) {
            err = "line " + std::to_string(a.line) + ": " + ferr;
            return false;
        }
        anim.set("frames", frames);
        s.anims.set(a.toks[1].c_str(), anim);
    }
    return true;
}
