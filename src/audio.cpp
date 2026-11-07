#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "audio.h"

// waveOut with a handful of small buffers and a thread that refills whichever
// one comes back. Latency is around 70ms, fine for this kind of game.

enum { OUT_RATE = 44100, BUF_FRAMES = 512, NUM_BUFS = 6, MAX_VOICES = 32 };

struct Voice {
    int id;
    Sound sound;
    double pos;
    float step;         // source samples per output sample
    float volume;
    float target;       // volume ramps towards this
    float ramp;         // volume change per output sample
    float pan;
    bool loop;
    bool stop_at_zero;
    bool music;         // follows the music volume instead of the effects one
};

static HWAVEOUT dev;
static WAVEHDR headers[NUM_BUFS];
static int16_t buffers[NUM_BUFS][BUF_FRAMES * 2];
static HANDLE event;
static HANDLE thread;
static std::atomic<bool> running{ false };
static CRITICAL_SECTION lock;

static Voice voices[MAX_VOICES];
static int next_id = 1;
static std::atomic<float> master{ 1.0f };
static float music_gain = 1, sfx_gain = 1;
static int music_voice;

Sound sound_from_pcm(const uint8_t* d, size_t size)
{
    if (size < 12 || memcmp(d, "PCM1", 4) != 0)
        return Sound();
    auto s = std::make_shared<SoundData>();
    uint32_t rate, count;
    memcpy(&rate, d + 4, 4);
    memcpy(&count, d + 8, 4);
    if (12 + size_t(count) * 2 > size)
        return Sound();
    s->rate = (int)rate;
    s->samples.resize(count);
    if (count)
        memcpy(s->samples.data(), d + 12, size_t(count) * 2);
    return s;
}

static void mix(int16_t* out)
{
    static float acc[BUF_FRAMES * 2];
    memset(acc, 0, sizeof(acc));

    EnterCriticalSection(&lock);
    for (Voice& v : voices) {
        if (!v.id)
            continue;
        const std::vector<int16_t>& src = v.sound->samples;
        size_t n = src.size();
        if (n == 0) {
            v.id = 0;
            v.sound.reset();
            continue;
        }
        // constant power-ish pan
        float lp = sqrtf(0.5f * (1 - v.pan));
        float rp = sqrtf(0.5f * (1 + v.pan));

        for (int i = 0; i < BUF_FRAMES; i++) {
            size_t i0 = (size_t)v.pos;
            if (i0 >= n) {
                if (v.loop) {
                    v.pos = fmod(v.pos, (double)n);
                    i0 = (size_t)v.pos;
                } else {
                    v.id = 0;
                    break;
                }
            }
            size_t i1 = i0 + 1 < n ? i0 + 1 : (v.loop ? 0 : i0);
            float frac = float(v.pos - i0);
            float s = (src[i0] + (src[i1] - src[i0]) * frac) * (1.f / 32768.f);

            if (v.volume != v.target) {
                v.volume += v.ramp;
                if ((v.ramp > 0 && v.volume >= v.target) || (v.ramp < 0 && v.volume <= v.target))
                    v.volume = v.target;
            }
            if (v.stop_at_zero && v.volume <= 0) {
                v.id = 0;
                break;
            }

            s *= v.volume * (v.music ? music_gain : sfx_gain);
            acc[i * 2] += s * lp;
            acc[i * 2 + 1] += s * rp;
            v.pos += v.step;
        }
        if (!v.id)
            v.sound.reset();
    }
    float m = master;
    LeaveCriticalSection(&lock);

    for (int i = 0; i < BUF_FRAMES * 2; i++) {
        float x = acc[i] * m;
        // soft knee so a pile of voices squashes instead of wrapping around
        if (x > 0.8f) x = 0.8f + (x - 0.8f) / (1 + (x - 0.8f) * 5);
        else if (x < -0.8f) x = -0.8f + (x + 0.8f) / (1 - (x + 0.8f) * 5);
        if (x > 1) x = 1;
        if (x < -1) x = -1;
        out[i] = int16_t(x * 32767);
    }
}

static DWORD WINAPI audio_thread(void*)
{
    while (running) {
        WaitForSingleObject(event, 50);
        for (WAVEHDR& h : headers) {
            if (h.dwFlags & WHDR_DONE) {
                mix((int16_t*)h.lpData);
                h.dwFlags &= ~WHDR_DONE;
                waveOutWrite(dev, &h, sizeof(h));
            }
        }
    }
    return 0;
}

bool audio_init()
{
    InitializeCriticalSection(&lock);

    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2; fmt.nSamplesPerSec = OUT_RATE;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 4; fmt.nAvgBytesPerSec = OUT_RATE * 4;

    event = CreateEventA(0, FALSE, FALSE, 0);
    if (waveOutOpen(&dev, WAVE_MAPPER, &fmt, (DWORD_PTR)event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        fprintf(stderr, "audio: no output device, running silent\n");
        CloseHandle(event);
        DeleteCriticalSection(&lock);
        event = 0; dev = 0;
        return false;
    }

    for (int i = 0; i < NUM_BUFS; i++) {
        WAVEHDR& h = headers[i];
        memset(&h, 0, sizeof(h));
        h.lpData = (LPSTR)buffers[i];
        h.dwBufferLength = sizeof(buffers[i]);
        waveOutPrepareHeader(dev, &h, sizeof(h));
        memset(buffers[i], 0, sizeof(buffers[i]));
        waveOutWrite(dev, &h, sizeof(h));
    }

    running = true;
    thread = CreateThread(0, 0, audio_thread, 0, 0, 0);
    SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
    return true;
}

void audio_shutdown()
{
    if (!dev)
        return;
    running = false;
    WaitForSingleObject(thread, 1000);
    CloseHandle(thread);
    waveOutReset(dev);
    for (WAVEHDR& h : headers)
        waveOutUnprepareHeader(dev, &h, sizeof(h));
    waveOutClose(dev);
    CloseHandle(event);
    dev = 0;
    for (Voice& v : voices) {
        v.id = 0;
        v.sound.reset();
    }
    DeleteCriticalSection(&lock);
}

int audio_play(const Sound& s, float volume, float pitch, float pan, bool loop)
{
    if (!dev || !s || s->samples.empty())
        return 0;
    EnterCriticalSection(&lock);
    Voice* slot = 0;
    for (Voice& v : voices) {
        if (!v.id) {
            slot = &v;
            break;
        }
    }
    // everything busy: steal the one that's closest to done, unless it's music
    if (!slot) {
        double best = -1;
        for (Voice& v : voices) {
            if (v.id == music_voice || v.loop)
                continue;
            double left = 1 - v.pos / v.sound->samples.size();
            if (!slot || left < best) {
                best = left;
                slot = &v;
            }
        }
    }
    int id = 0;
    if (slot) {
        id = next_id++;
        slot->id = id;
        slot->sound = s;
        slot->pos = 0;
        slot->step = pitch * s->rate / (float)OUT_RATE;
        slot->volume = slot->target = volume;
        slot->ramp = 0;
        slot->pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
        slot->loop = loop;
        slot->stop_at_zero = false;
        slot->music = false;
    }
    LeaveCriticalSection(&lock);
    return id;
}

static Voice* find(int id)
{
    if (!id) return 0;
    for (Voice& v : voices)
        if (v.id == id) return &v;
    return 0;
}

static void fade_to(Voice* v, float target, float seconds)
{
    v->target = target;
    if (seconds <= 0) {
        v->volume = target;
        v->ramp = 0;
        return;
    }
    v->ramp = (target - v->volume) / (seconds * OUT_RATE);
    if (v->ramp == 0) v->volume = target;
}

void audio_stop(int id, float fade)
{
    if (!dev) return;
    EnterCriticalSection(&lock);
    if (Voice* v = find(id)) {
        if (fade <= 0) {
            v->id = 0;
            v->sound.reset();
        } else {
            fade_to(v, 0, fade);
            v->stop_at_zero = true;
        }
    }
    LeaveCriticalSection(&lock);
}

void audio_stop_all()
{
    if (!dev) return;
    EnterCriticalSection(&lock);
    for (Voice& v : voices) {
        v.id = 0;
        v.sound.reset();
    }
    music_voice = 0;
    LeaveCriticalSection(&lock);
}

void audio_set_volume(int id, float volume, float fade)
{
    if (!dev) return;
    EnterCriticalSection(&lock);
    if (Voice* v = find(id)) fade_to(v, volume, fade);
    LeaveCriticalSection(&lock);
}

bool audio_playing(int id)
{
    if (!dev) return false;
    EnterCriticalSection(&lock);
    bool p = find(id) != 0;
    LeaveCriticalSection(&lock);
    return p;
}

void audio_master(float volume) { master = volume; }

void audio_groups(float music, float sfx)
{
    if (!dev) return;
    EnterCriticalSection(&lock);
    music_gain = music;
    sfx_gain = sfx;
    LeaveCriticalSection(&lock);
}

void music_play(const Sound& s, float volume, float fade)
{
    if (!dev || !s)
        return;
    EnterCriticalSection(&lock);
    int cur_id = music_voice;
    Voice* cur = find(cur_id);
    bool same = cur && cur->sound == s && !cur->stop_at_zero;
    LeaveCriticalSection(&lock);
    if (same) {
        audio_set_volume(cur_id, volume, fade);
        return;
    }

    audio_stop(cur_id, fade);
    int id = audio_play(s, fade > 0 ? 0 : volume, 1, 0, true);
    EnterCriticalSection(&lock);
    if (Voice* v = find(id))
        v->music = true;
    music_voice = id;
    LeaveCriticalSection(&lock);
    if (id && fade > 0)
        audio_set_volume(id, volume, fade);
}

void music_stop(float fade)
{
    if (!dev)
        return;
    EnterCriticalSection(&lock);
    int cur_id = music_voice;
    music_voice = 0;
    LeaveCriticalSection(&lock);
    audio_stop(cur_id, fade);
}
