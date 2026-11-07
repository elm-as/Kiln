#pragma once
#include <cstdint>
#include <memory>
#include <vector>

struct SoundData {
    int rate = 22050;
    std::vector<int16_t> samples;   // mono
};

// shared so a sound that gets hot reloaded while playing doesn't pull the
// memory out from under the mixer
typedef std::shared_ptr<const SoundData> Sound;

Sound sound_from_pcm(const uint8_t* data, size_t size);    // cooked "PCM1" blob

bool audio_init();
void audio_shutdown();

// returns a voice id, 0 if it couldn't play
int audio_play(const Sound& s, float volume = 1, float pitch = 1, float pan = 0, bool loop = false);
void audio_stop(int voice, float fade = 0);
void audio_stop_all();
void audio_set_volume(int voice, float volume, float fade = 0);
bool audio_playing(int voice);
void audio_master(float volume);
void audio_groups(float music, float sfx);  // 0..1 each, on top of master

// one music track at a time, crossfades when you switch
void music_play(const Sound& s, float volume = 0.6f, float fade = 1);
void music_stop(float fade = 1);
