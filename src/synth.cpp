#include "synth.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

static const float TAU = 6.2831853f;

static uint32_t noise_state = 0x1234567;

float synth_noise()
{
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;
    return (noise_state >> 8) * (2.0f / 16777216.0f) - 1;
}

bool synth_parse_wave(const std::string& s, Wave* w)
{
    if (s == "square") *w = WAVE_SQUARE;
    else if (s == "saw") *w = WAVE_SAW;
    else if (s == "triangle") *w = WAVE_TRIANGLE;
    else if (s == "sine") *w = WAVE_SINE;
    else if (s == "noise") *w = WAVE_NOISE;
    else if (s == "drum") *w = WAVE_DRUM;
    else return false;
    return true;
}

// one step of a basic oscillator. phase is in cycles. hold is the
// sample-and-hold value noise uses
float synth_osc(Wave w, double& phase, float freq, float duty, float& hold)
{
    double before = phase;
    phase += freq / SYNTH_RATE;
    if (w == WAVE_NOISE && floor(phase) != floor(before))
        hold = synth_noise();
    phase -= floor(phase);
    float p = (float)phase;
    switch (w) {
    case WAVE_SQUARE: return p < duty ? 1.f : -1.f;
    case WAVE_SAW: return 2 * p - 1;
    case WAVE_TRIANGLE: return p < 0.5f ? 4 * p - 1 : 3 - 4 * p;
    case WAVE_SINE: return sinf(p * TAU);
    case WAVE_NOISE: return hold;
    default: return 0;
    }
}

std::vector<std::string> synth_split(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok)
        out.push_back(tok);
    return out;
}

// '#' starts a comment only at the start of a word, C#4 is a note
std::string synth_strip_comment(const std::string& line)
{
    for (size_t i = 0; i < line.size(); i++)
        if (line[i] == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t'))
            return line.substr(0, i);
    return line;
}

bool synth_to_float(const std::string& s, float* out)
{
    char* end;
    *out = strtof(s.c_str(), &end);
    return end != s.c_str() && *end == 0;
}

// ---------------------------------------------------------------------------
// sound effects

bool sfx_parse(const std::string& text, SfxParams* p, std::string* err)
{
    std::istringstream in(text);
    std::string line;
    int lineno = 0;
    auto fail = [&](const char* msg) {
        char buf[128];
        snprintf(buf, sizeof(buf), "line %d: %s", lineno, msg);
        *err = buf;
        return false;
    };

    while (std::getline(in, line)) {
        lineno++;
        std::vector<std::string> t = synth_split(synth_strip_comment(line));
        if (t.empty())
            continue;
        const std::string& k = t[0];
        std::vector<float> v;
        for (size_t i = 1; i < t.size() && k != "wave"; i++) {
            if (t[i] == "->")
                continue;
            float f;
            if (!synth_to_float(t[i], &f))
                return fail("expected a number");
            v.push_back(f);
        }

        if (k == "wave") {
            if (t.size() < 2 || !synth_parse_wave(t[1], &p->wave) || p->wave == WAVE_DRUM)
                return fail("unknown wave");
        } else if (k == "freq") {
            if (v.empty()) return fail("freq needs a value");
            p->freq = v[0];
            p->freq_end = v.size() > 1 ? v[1] : v[0];
        } else if (k == "duty" && v.size() == 1) {
            p->duty = v[0];
        } else if (k == "attack" && v.size() == 1) {
            p->attack = v[0];
        } else if (k == "sustain" && v.size() == 1) {
            p->sustain = v[0];
        } else if (k == "decay" && v.size() == 1) {
            p->decay = v[0];
        } else if (k == "volume" && v.size() == 1) {
            p->volume = v[0];
        } else if (k == "vibrato" && v.size() == 2) {
            p->vib_speed = v[0];
            p->vib_depth = v[1];
        } else if (k == "arp" && v.size() == 2) {
            p->arp_mul = v[0];
            p->arp_time = v[1];
        } else {
            return fail("unknown key or wrong number of values");
        }
    }
    return true;
}

std::vector<int16_t> sfx_render(const SfxParams& p)
{
    float dur = p.attack + p.sustain + p.decay;
    int n = int(dur * SYNTH_RATE);
    std::vector<int16_t> out(n > 0 ? n : 0);

    double phase = 0;
    float hold = 0;
    for (int i = 0; i < n; i++) {
        float t = i / (float)SYNTH_RATE;
        float f = p.freq + (p.freq_end - p.freq) * (t / dur);
        if (p.arp_time > 0 && t >= p.arp_time)
            f *= p.arp_mul;
        if (p.vib_depth > 0)
            f *= 1 + sinf(t * p.vib_speed * TAU) * p.vib_depth;

        float env;
        if (p.attack > 0 && t < p.attack)
            env = t / p.attack;
        else if (t < p.attack + p.sustain)
            env = 1;
        else if (p.decay > 0)
            env = 1 - (t - p.attack - p.sustain) / p.decay;
        else
            env = 0;

        float s = synth_osc(p.wave, phase, f, p.duty, hold) * env * p.volume;
        out[i] = int16_t(fmaxf(-1, fminf(1, s)) * 32767);
    }
    return out;
}
