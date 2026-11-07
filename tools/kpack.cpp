// kpack: cooks the assets folder into a single .pak for shipping.
//
//   kpack [assets dir] [out.pak] [--atlas preview.png]
//
// Exits non-zero if anything failed to cook, so a build script can stop.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include "cook.h"
#include "image.h"
#include "json.h"

int main(int argc, char** argv)
{
    const char* src = "assets";
    const char* out = "build/game.pak";
    const char* preview = 0;
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--atlas") && i + 1 < argc) {
            preview = argv[++i];
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("usage: kpack [assets dir] [out.pak] [--atlas preview.png]\n");
            return 0;
        } else if (pos == 0) {
            src = argv[i];
            pos++;
        } else {
            out = argv[i];
        }
    }

    auto t0 = std::chrono::steady_clock::now();
    Pak pak;
    std::string log;
    bool ok = cook_assets(src, &pak, &log);
    fputs(log.c_str(), stderr);

    size_t bytes = 0;
    int sprites = 0, sounds = 0, songs = 0, scripts = 0, scenes = 0, data = 0;
    for (const auto& f : pak.files) {
        bytes += f.second.size();
        const std::string& n = f.first;
        if (!n.compare(0, 4, "sfx/")) sounds++;
        else if (!n.compare(0, 6, "music/")) songs++;
        else if (!n.compare(0, 8, "scripts/")) scripts++;
        else if (!n.compare(0, 7, "scenes/")) scenes++;
        else if (!n.compare(0, 5, "data/")) data++;
    }
    if (pak.files.count("atlas.json")) {
        const std::vector<uint8_t>& m = pak.files["atlas.json"];
        Json meta;
        std::string err;
        if (json_parse(std::string(m.begin(), m.end()), &meta, &err))
            sprites = (int)meta["sprites"].size();
    }

    if (!pak_write(pak, out)) {
        fprintf(stderr, "kpack: can't write %s\n", out);
        return 1;
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("%s: %d sprites, %d sounds, %d songs, %d scripts, %d scenes, %d data files, %.0f KB in %.0f ms\n",
           out, sprites, sounds, songs, scripts, scenes, data, bytes / 1024.0, ms);

    if (preview && pak.files.count("atlas.rgba")) {
        const std::vector<uint8_t>& a = pak.files["atlas.rgba"];
        uint32_t w, h;
        memcpy(&w, a.data(), 4);
        memcpy(&h, a.data() + 4, 4);
        png_write(preview, a.data() + 8, (int)w, (int)h);
        printf("atlas %ux%u -> %s\n", w, h, preview);
    }
    return ok ? 0 : 1;
}
