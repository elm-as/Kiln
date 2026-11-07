// Tracker-style song parser and synthesizer (.song files).

#include "synth.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>

static const float TAU = 6.2831853f;

struct Inst {
    Wave wave = WAVE_SQUARE;
    float vol = 0.25f;
    float attack = 0.005f, decay = 0.2f, sustain = 0.5f, release = 0.08f;
    float duty = 0.5f;
    float vibrato = 0;
};

struct PatternLine {
    std::string inst;
    std::vector<std::string> tokens;
};

struct Voice {
    const Inst* inst = 0;
    bool on = false;
    bool released = false;
    float freq = 0;
    char drum = 0;
    float t = 0;
    float rel_t = 0;
    float rel_level = 0;
    double phase = 0;
    float hold = 0;
    float prev_noise = 0;
};

static float note_freq(const std::string& tok, bool* ok)
{
    static const int semis[7] = { 9, 11, 0, 2, 4, 5, 7 };     // A B C D E F G
    *ok = false;
    if (tok.size() < 2)
        return 0;
    char c = (char)toupper((unsigned char)tok[0]);
    if (c < 'A' || c > 'G')
        return 0;
    int semi = semis[c - 'A'];
    size_t i = 1;
    if (tok[i] == '#') { semi++; i++; }
    else if (tok[i] == 'b') { semi--; i++; }
    if (i >= tok.size() || !isdigit((unsigned char)tok[i]))
        return 0;
    int octave = atoi(tok.c_str() + i);
    int midi = (octave + 1) * 12 + semi;
    *ok = true;
    return 440.f * powf(2.f, (midi - 69) / 12.f);
}

static float envelope(Voice& v)
{
    const Inst& in = *v.inst;
    float lvl;
    if (v.t < in.attack)
        lvl = v.t / in.attack;
    else if (v.t < in.attack + in.decay)
        lvl = 1 - (1 - in.sustain) * ((v.t - in.attack) / in.decay);
    else
        lvl = in.sustain;

    if (v.released) {
        float k = 1 - v.rel_t / in.release;
        if (k <= 0) {
            v.on = false;
            return 0;
        }
        return v.rel_level * k;
    }
    if (in.sustain <= 0 && v.t >= in.attack + in.decay)
        v.on = false;
    return lvl;
}

static float drum_sample(Voice& v)
{
    float t = v.t;
    switch (v.drum) {
    case 'k': {
        float f = 45 + 110 * expf(-t * 30);
        v.phase += f / SYNTH_RATE;
        if (t > 0.5f) v.on = false;
        return sinf(float(v.phase) * TAU) * expf(-t * 9);
    }
    case 's':
        if (t > 0.4f) v.on = false;
        return synth_noise() * expf(-t * 18) * 0.7f + sinf(t * 185 * TAU) * expf(-t * 25) * 0.5f;
    case 'h':
    case 'o': {
        float n = synth_noise();
        float hp = n - v.prev_noise;    // crude highpass, takes the thud out
        v.prev_noise = n;
        float decay = v.drum == 'h' ? 55.f : 12.f;
        if (t > 0.6f) v.on = false;
        return hp * expf(-t * decay) * 0.35f;
    }
    case 'c':
        if (t > 1.5f) v.on = false;
        return synth_noise() * expf(-t * 3.5f) * 0.35f;
    }
    v.on = false;
    return 0;
}

bool song_render(const std::string& text, std::vector<int16_t>* out, std::string* err)
{
    float tempo = 120;
    int rows_per_beat = 4;
    std::map<std::string, Inst> insts;
    std::map<std::string, std::vector<PatternLine>> patterns;
    std::vector<std::string> order;

    std::istringstream in(text);
    std::string line;
    int lineno = 0;
    std::vector<PatternLine>* cur = 0;

    auto fail = [&](const std::string& msg) {
        char buf[64];
        snprintf(buf, sizeof(buf), "line %d: ", lineno);
        *err = buf + msg;
        return false;
    };

    while (std::getline(in, line)) {
        lineno++;
        std::vector<std::string> t = synth_split(synth_strip_comment(line));
        if (t.empty())
            continue;
        const std::string& k = t[0];

        if (k == "tempo" && t.size() == 2) {
            tempo = (float)atof(t[1].c_str());
            cur = 0;
        } else if (k == "rows" && t.size() == 2) {
            rows_per_beat = atoi(t[1].c_str());
            cur = 0;
        } else if (k == "inst") {
            if (t.size() < 3)
                return fail("inst needs a name and a wave");
            Inst ins;
            if (!synth_parse_wave(t[2], &ins.wave))
                return fail("unknown wave '" + t[2] + "'");
            for (size_t i = 3; i + 1 < t.size(); i += 2) {
                float v;
                if (!synth_to_float(t[i + 1], &v))
                    return fail("bad value for " + t[i]);
                if (t[i] == "vol") ins.vol = v;
                else if (t[i] == "attack") ins.attack = fmaxf(v, 0.001f);
                else if (t[i] == "decay") ins.decay = fmaxf(v, 0.001f);
                else if (t[i] == "sustain") ins.sustain = v;
                else if (t[i] == "release") ins.release = fmaxf(v, 0.001f);
                else if (t[i] == "duty") ins.duty = v;
                else if (t[i] == "vibrato") ins.vibrato = v;
                else return fail("unknown inst setting '" + t[i] + "'");
            }
            insts[t[1]] = ins;
            cur = 0;
        } else if (k == "pattern" && t.size() == 2) {
            cur = &patterns[t[1]];
            cur->clear();
        } else if (k == "order") {
            order.insert(order.end(), t.begin() + 1, t.end());
            cur = 0;
        } else if (cur) {
            if (!insts.count(k))
                return fail("unknown instrument '" + k + "'");
            PatternLine pl;
            pl.inst = k;
            for (size_t i = 1; i < t.size(); i++)
                if (t[i] != "|")
                    pl.tokens.push_back(t[i]);
            cur->push_back(pl);
        } else {
            return fail("don't know what '" + k + "' is");
        }
    }

    if (order.empty())
        return fail("no order line");
    if (tempo <= 0 || rows_per_beat <= 0)
        return fail("bad tempo or rows");

    double row_len = SYNTH_RATE * 60.0 / (tempo * rows_per_beat);
    std::vector<Voice> voices;
    std::vector<float> mix;
    double pos = 0;

    for (const std::string& name : order) {
        auto it = patterns.find(name);
        if (it == patterns.end())
            return fail("order uses unknown pattern '" + name + "'");
        const std::vector<PatternLine>& pat = it->second;
        if (voices.size() < pat.size())
            voices.resize(pat.size());

        size_t nrows = 0;
        for (const PatternLine& pl : pat)
            if (pl.tokens.size() > nrows)
                nrows = pl.tokens.size();

        for (size_t row = 0; row < nrows; row++) {
            for (size_t k = 0; k < pat.size(); k++) {
                const PatternLine& pl = pat[k];
                if (row >= pl.tokens.size())
                    continue;
                const std::string& tok = pl.tokens[row];
                Voice& v = voices[k];
                const Inst* ins = &insts[pl.inst];
                if (tok == ".")
                    continue;
                if (tok == "-") {
                    if (v.on && !v.released) {
                        v.rel_level = envelope(v);
                        v.released = true;
                        v.rel_t = 0;
                    }
                    continue;
                }
                if (ins->wave == WAVE_DRUM) {
                    if (tok.size() != 1 || !strchr("ksohc", tok[0]))
                        return fail("pattern " + name + ": bad drum '" + tok + "'");
                    v = Voice();
                    v.inst = ins;
                    v.on = true;
                    v.drum = tok[0];
                    continue;
                }
                bool ok;
                float f = note_freq(tok, &ok);
                if (!ok)
                    return fail("pattern " + name + ": bad note '" + tok + "'");
                double keep_phase = v.phase;
                v = Voice();
                v.inst = ins;
                v.on = true;
                v.freq = f;
                v.phase = keep_phase;
            }

            size_t start = (size_t)pos;
            pos += row_len;
            size_t end = (size_t)pos;
            if (mix.size() < end)
                mix.resize(end, 0);

            for (Voice& v : voices) {
                if (!v.on)
                    continue;
                const Inst& ins = *v.inst;
                for (size_t i = start; i < end && v.on; i++) {
                    float s;
                    if (ins.wave == WAVE_DRUM) {
                        s = drum_sample(v);
                    } else {
                        float f = v.freq;
                        if (ins.vibrato > 0 && v.t > 0.15f)
                            f *= 1 + sinf(v.t * 5.5f * TAU) * ins.vibrato;
                        s = synth_osc(ins.wave, v.phase, f, ins.duty, v.hold) * envelope(v);
                    }
                    mix[i] += s * ins.vol;
                    v.t += 1.f / SYNTH_RATE;
                    if (v.released)
                        v.rel_t += 1.f / SYNTH_RATE;
                }
            }
        }
    }

    out->resize(mix.size());
    for (size_t i = 0; i < mix.size(); i++)
        (*out)[i] = int16_t(tanhf(mix[i]) * 32000);
    return true;
}

