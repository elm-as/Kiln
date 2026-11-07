#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// A pak is just named blobs. The cooker turns the source assets folder into
// one, kpack writes it to disk, and in dev builds the game cooks straight into
// memory so it can hot reload.
//
// What goes in:
//   atlas.rgba            u32 w, u32 h, pixels
//   atlas.json            sprite rects, origins, animations
//   sfx/<name>.pcm        "PCM1", u32 rate, u32 count, int16 mono
//   music/<name>.pcm      same
//   scripts/<name>.ks     source, checked for syntax errors
//   scenes/<name>.json
//   data/<name>.json
struct Pak {
    std::map<std::string, std::vector<uint8_t>> files;
};

bool pak_write(const Pak& pak, const char* path);
bool pak_read(const char* path, Pak* pak);

// returns false if anything failed. log gets one line per problem
bool cook_assets(const std::string& src_dir, Pak* out, std::string* log);

// newest mtime of anything under the source folders, for hot reload polling
int64_t assets_newest_mtime(const std::string& src_dir);

std::vector<uint8_t> pcm_pack(int rate, const std::vector<int16_t>& samples);
