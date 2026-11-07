#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// All of these return malloc'd RGBA8, top row first, or NULL on failure.
uint8_t* image_load(const char* path, int* w, int* h);
uint8_t* image_decode(const uint8_t* data, size_t size, int* w, int* h, const char* name = "image");
uint8_t* tga_decode(const uint8_t* data, size_t size, int* w, int* h, const char* name);
uint8_t* png_decode(const uint8_t* data, size_t size, int* w, int* h, const char* name);

// zlib stream (2 byte header + deflate). Used by the png loader.
bool zlib_inflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out);

bool png_write(const char* path, const uint8_t* rgba, int w, int h);
