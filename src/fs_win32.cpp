#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include "fs.h"

bool read_file(const char* path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? n : 0);
    size_t got = out.empty() ? 0 : fread(out.data(), 1, out.size(), f);
    fclose(f);
    return got == out.size();
}

bool read_text(const char* path, std::string& out)
{
    std::vector<uint8_t> data;
    if (!read_file(path, data))
        return false;
    out.assign(data.begin(), data.end());
    return true;
}

bool write_file(const char* path, const void* data, size_t size)
{
    // write next to it and rename, so a crash halfway never eats the old file
    std::string tmp = std::string(path) + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp.c_str());
        return false;
    }
    return MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING) != 0;
}

bool file_exists(const char* path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dir_exists(const char* path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

void make_dir(const char* path)
{
    CreateDirectoryA(path, 0);
}

int64_t file_mtime(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d))
        return 0;
    return int64_t(d.ftLastWriteTime.dwHighDateTime) << 32 | d.ftLastWriteTime.dwLowDateTime;
}

std::vector<std::string> list_files(const char* dir, const char* ext)
{
    std::vector<std::string> out;
    std::string pattern = path_join(dir, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        std::string name = fd.cFileName;
        if (ext && path_ext(name) != ext)
            continue;
        out.push_back(name);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

std::string path_join(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    char last = a.back();
    if (last == '/' || last == '\\')
        return a + b;
    return a + "/" + b;
}

std::string path_stem(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string path_ext(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "";
    std::string e = path.substr(dot);
    for (char& c : e)
        c = (char)tolower((unsigned char)c);
    return e;
}
