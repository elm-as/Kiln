#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Everything here renders to mono int16 at SYNTH_RATE.
const int SYNTH_RATE = 22050;

enum Wave { WAVE_SQUARE, WAVE_SAW, WAVE_TRIANGLE, WAVE_SINE, WAVE_NOISE, WAVE_DRUM };

// .sfx files: one "key value..." per line
//   wave square          square saw triangle sine noise
//   freq 440 -> 880      start and end, slides linearly
//   duty 0.25
//   attack 0.01          seconds
//   sustain 0.05
//   decay 0.2
//   volume 0.5
//   vibrato 8 0.03       speed in Hz, depth as a fraction of the pitch
//   arp 1.5 0.06         multiply the pitch by 1.5 after 0.06s
struct SfxParams {
    Wave wave = WAVE_SQUARE;
    float freq = 440, freq_end = 440;
    float duty = 0.5f;
    float attack = 0.005f, sustain = 0.05f, decay = 0.15f;
    float volume = 0.5f;
    float vib_speed = 0, vib_depth = 0;
    float arp_mul = 1, arp_time = 0;
};

bool sfx_parse(const std::string& text, SfxParams* p, std::string* err);
std::vector<int16_t> sfx_render(const SfxParams& p);

// .song files, see assets/music for examples
bool song_render(const std::string& text, std::vector<int16_t>* out, std::string* err);

// shared oscillator & text helpers used by sfx and song synthesis
float synth_noise();
bool synth_parse_wave(const std::string& s, Wave* w);
float synth_osc(Wave w, double& phase, float freq, float duty, float& hold);
std::vector<std::string> synth_split(const std::string& line);
std::string synth_strip_comment(const std::string& line);
bool synth_to_float(const std::string& s, float* out);
