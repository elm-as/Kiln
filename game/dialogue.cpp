// Dialogue box and cutscenes. Everything they say lives in data/story.json.
//
// Text comes out a letter at a time with a little blip in the speaker's voice.
// Jump or the spear skips to the end of the line, then to the next one.
// LUEUR_SKIP_STORY=1 turns all of it off, for replays.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "game.h"

struct Line {
    std::string who;
    std::string text;
    std::string give;       // a weapon Braise hands over on this line
};

static Json story;

static std::vector<Line> lines;
static size_t line_index;
static std::vector<std::string> wrapped;    // current line, cut to fit the box
static size_t first_row;                    // long lines scroll by pages of rows
static float shown;                         // characters revealed so far
static int blips;

static const int BOX_ROWS = 3;
static const float CHARS_PER_SEC = 45;

static bool skip_story()
{
    static bool skip = getenv("LUEUR_SKIP_STORY") != 0;
    return skip;
}

void story_load()
{
    asset_json("data/story.json", &story);
}

const Json& story_data()
{
    return story;
}

static size_t count_chars(const std::string& s)
{
    size_t n = 0;
    const char* p = s.c_str();
    while (utf8_next(&p))
        n++;
    return n;
}

// the first n characters of s, respecting utf-8
static std::string prefix(const std::string& s, size_t n)
{
    const char* p = s.c_str();
    for (size_t i = 0; i < n && utf8_next(&p); i++)
        ;
    return std::string(s.c_str(), p);
}

// French puts a space before ? ! : ; and », but they must never start a line
static bool hangs_on_previous(const std::string& word)
{
    static const char* marks[] = { "?", "!", ":", ";", "»", "…" };
    for (const char* m : marks)
        if (word.compare(0, strlen(m), m) == 0)
            return true;
    return false;
}

std::vector<std::string> wrap_text(const Font& f, const std::string& text, float width)
{
    std::vector<std::string> out;
    std::string line, word;
    auto flush_word = [&]() {
        if (word.empty())
            return;
        std::string attempt = line.empty() ? word : line + " " + word;
        if (!line.empty() && hangs_on_previous(word) && text_width(f, attempt.c_str()) > width) {
            // take the last word down with it
            size_t cut = line.rfind(' ');
            if (cut != std::string::npos) {
                out.push_back(line.substr(0, cut));
                line = line.substr(cut + 1) + " " + word;
            } else {
                line = attempt;
            }
        } else if (!line.empty() && text_width(f, attempt.c_str()) > width) {
            out.push_back(line);
            line = word;
        } else {
            line = attempt;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') {
            flush_word();
        } else if (c == '\n') {
            flush_word();
            out.push_back(line);
            line.clear();
        } else {
            word += c;
        }
    }
    flush_word();
    if (!line.empty())
        out.push_back(line);
    return out;
}

static const float TEXT_W = 236;

// skipping a dialogue must not skip what it gives
static void give_rest()
{
    for (size_t i = line_index; i < lines.size(); i++)
        if (!lines[i].give.empty()) {
            give_weapon(lines[i].give);
            lines[i].give.clear();
        }
}

static void begin_line()
{
    if (!lines[line_index].give.empty()) {
        give_weapon(lines[line_index].give);
        lines[line_index].give.clear();
    }
    wrapped = wrap_text(G.font, lines[line_index].text, TEXT_W);
    first_row = 0;
    shown = 0;
    blips = 0;
}

bool dialogue_start(const std::string& id)
{
    const Json& d = story["dialogues"][id.c_str()];
    if (!d.is_arr() || d.size() == 0) {
        fprintf(stderr, "story: no dialogue called %s\n", id.c_str());
        return false;
    }
    lines.clear();
    for (const Json& l : d.arr)
        lines.push_back({ l["who"].str(), l["text"].str(), l["give"].str("") });
    line_index = 0;
    if (skip_story()) {
        give_rest();
        lines.clear();
        return false;
    }
    begin_line();
    return true;
}

bool dialogue_active()
{
    return line_index < lines.size();
}

static size_t visible_chars()
{
    size_t n = 0;
    for (size_t r = first_row; r < wrapped.size() && r < first_row + BOX_ROWS; r++)
        n += count_chars(wrapped[r]);
    return n;
}

static bool wants_next(const Input& in)
{
    return input_confirm(in) || input_attack(in);
}

void dialogue_frame(const Input& in, float dt)
{
    if (!dialogue_active())
        return;
    if (input_skip(in)) {
        give_rest();
        lines.clear();
        line_index = 0;
        return;
    }

    size_t total = visible_chars();
    float before = shown;
    shown = fminf(shown + dt * CHARS_PER_SEC, (float)total);

    // a blip every other letter, in the speaker's voice
    const Json& who = story["characters"][lines[line_index].who.c_str()];
    int now = (int)shown / 2;
    if (now > blips && (int)before < (int)total) {
        blips = now;
        audio_play(sfx("voice"), 0.35f, who["voice"].f(1) * randf(0.92f, 1.08f));
    }

    if (!wants_next(in))
        return;
    if (shown < total) {
        shown = (float)total;
        return;
    }
    audio_play(sfx("menu"), 0.25f, 1.3f);
    if (first_row + BOX_ROWS < wrapped.size()) {
        first_row += BOX_ROWS;
        shown = 0;
        blips = 0;
        return;
    }
    line_index++;
    if (dialogue_active())
        begin_line();
}

static void draw_portrait(const std::string& name, float x, float y, float scale)
{
    int id = sprite_id("portraits");
    const SpriteSheet* s = sprite_get(id);
    if (!s)
        return;
    int frame = s->frame(name.c_str());
    if (frame < 0)
        return;
    SpriteOpts o;
    o.scale = { scale, scale };
    draw_spr_ex(id, frame, { x, y }, o);
}

void dialogue_draw()
{
    if (!dialogue_active())
        return;
    const Line& l = lines[line_index];
    const Json& who = story["characters"][l.who.c_str()];
    Color name_color = json_color(who["color"], WHITE);

    Rect box = { 6, VIEW_H - 64.f, VIEW_W - 12.f, 58 };
    draw_rect(box, { 10, 8, 16, 230 });
    draw_rect_lines(box, 1, fade(name_color, 0.6f));

    draw_rect({ box.x + 5, box.y + 5, 48, 48 }, { 24, 20, 34, 255 });
    draw_portrait(who["portrait"].str(), box.x + 5, box.y + 5, 2);

    float tx = box.x + 60;
    draw_text(G.font, who["name"].str(l.who.c_str()), tx, box.y + 3, name_color);

    size_t left = (size_t)shown;
    for (size_t r = first_row; r < wrapped.size() && r < first_row + BOX_ROWS; r++) {
        size_t n = count_chars(wrapped[r]);
        std::string part = left >= n ? wrapped[r] : prefix(wrapped[r], left);
        left = left >= n ? left - n : 0;
        draw_text(G.font, part.c_str(), tx, box.y + 17 + (r - first_row) * 12.f, rgb(0xf0e8f8));
    }

    // a little arrow once the line is out
    if (shown >= visible_chars() && fmod(engine_time(), 0.8) < 0.5)
        draw_rect({ box.x + box.w - 10, box.y + box.h - 8, 4, 3 }, name_color);
}
