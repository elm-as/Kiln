#include "cook.h"
#include <cstring>
#include "fs.h"
#include "json.h"
#include "script.h"
#include "synth.h"

uint32_t cook_get32(const uint8_t* p);

static bool decode_wav(const std::vector<uint8_t>& d, int* rate, std::vector<int16_t>& out, std::string& err)
{
    if (d.size() < 12 || memcmp(&d[0], "RIFF", 4) || memcmp(&d[8], "WAVE", 4)) {
        err = "not a wav file";
        return false;
    }
    int channels = 0, bits = 0, format = 0;
    size_t pos = 12;
    while (pos + 8 <= d.size()) {
        uint32_t len = cook_get32(&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        if (pos + 8 + len > d.size())
            len = uint32_t(d.size() - pos - 8);
        if (!memcmp(&d[pos], "fmt ", 4) && len >= 16) {
            format = body[0] | body[1] << 8;
            channels = body[2] | body[3] << 8;
            *rate = (int)cook_get32(body + 4);
            bits = body[14] | body[15] << 8;
        } else if (!memcmp(&d[pos], "data", 4)) {
            if (format != 1 || (bits != 8 && bits != 16) || channels < 1 || channels > 2) {
                err = "only 8/16 bit PCM, mono or stereo";
                return false;
            }
            size_t frame = size_t(channels) * bits / 8;
            size_t n = len / frame;
            out.resize(n);
            for (size_t i = 0; i < n; i++) {
                int sum = 0;
                for (int c = 0; c < channels; c++) {
                    const uint8_t* s = body + i * frame + c * (bits / 8);
                    sum += bits == 16 ? int16_t(s[0] | s[1] << 8) : (int(s[0]) - 128) << 8;
                }
                out[i] = int16_t(sum / channels);
            }
            return true;
        }
        pos += 8 + len + (len & 1);
    }
    err = "no data chunk";
    return false;
}

bool cook_sounds(const std::string& dir, Pak* out, std::string* log)
{
    bool ok = true;
    for (const std::string& f : list_files(dir.c_str())) {
        std::string ext = path_ext(f);
        std::string path = path_join(dir, f);
        std::string err;
        std::vector<int16_t> pcm;
        int rate = SYNTH_RATE;
        if (ext == ".sfx") {
            std::string text;
            read_text(path.c_str(), text);
            SfxParams p;
            if (!sfx_parse(text, &p, &err)) {
                *log += "sounds/" + f + ": " + err + "\n";
                ok = false;
                continue;
            }
            pcm = sfx_render(p);
        } else if (ext == ".wav") {
            std::vector<uint8_t> d;
            read_file(path.c_str(), d);
            if (!decode_wav(d, &rate, pcm, err)) {
                *log += "sounds/" + f + ": " + err + "\n";
                ok = false;
                continue;
            }
        } else {
            continue;
        }
        out->files["sfx/" + path_stem(f) + ".pcm"] = pcm_pack(rate, pcm);
    }
    return ok;
}

bool cook_music(const std::string& dir, Pak* out, std::string* log)
{
    bool ok = true;
    for (const std::string& f : list_files(dir.c_str(), ".song")) {
        std::string text, err;
        read_text(path_join(dir, f).c_str(), text);
        std::vector<int16_t> pcm;
        if (!song_render(text, &pcm, &err)) {
            *log += "music/" + f + ": " + err + "\n";
            ok = false;
            continue;
        }
        out->files["music/" + path_stem(f) + ".pcm"] = pcm_pack(SYNTH_RATE, pcm);
    }
    return ok;
}

bool cook_scripts(const std::string& dir, Pak* out, std::string* log)
{
    bool ok = true;
    for (const std::string& f : list_files(dir.c_str(), ".ks")) {
        std::string text, err;
        read_text(path_join(dir, f).c_str(), text);
        if (!script_check(text, f, &err)) {
            *log += "scripts/" + err + "\n";
            ok = false;
        }
        // still packed, the game reports the error again and keeps running
        out->files["scripts/" + f].assign(text.begin(), text.end());
    }
    return ok;
}

bool cook_json(const std::string& dir, const std::string& prefix, Pak* out, std::string* log)
{
    bool ok = true;
    for (const std::string& f : list_files(dir.c_str(), ".json")) {
        std::string text, err;
        read_text(path_join(dir, f).c_str(), text);
        Json j;
        if (!json_parse(text, &j, &err)) {
            *log += prefix + f + ": " + err + "\n";
            ok = false;
            continue;
        }
        out->files[prefix + f].assign(text.begin(), text.end());
    }
    return ok;
}

