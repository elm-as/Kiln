#pragma once
#include <cstdint>
#include <string>
#include <vector>

bool read_file(const char* path, std::vector<uint8_t>& out);
bool read_text(const char* path, std::string& out);
bool write_file(const char* path, const void* data, size_t size);
bool file_exists(const char* path);
bool dir_exists(const char* path);
void make_dir(const char* path);
int64_t file_mtime(const char* path);      // 0 if missing

// files directly inside dir (not recursive), names only, sorted
std::vector<std::string> list_files(const char* dir, const char* ext = 0);

std::string path_join(const std::string& a, const std::string& b);
std::string path_stem(const std::string& path);     // "a/b/hero.pix" -> "hero"
std::string path_ext(const std::string& path);      // ".pix", lowercase
