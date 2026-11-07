#pragma once
#include <string>
#include <utility>
#include <vector>

// Small DOM. Objects keep their keys in file order so a scene saved by the
// editor diffs cleanly against the one you wrote by hand.
struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ };

    Type type = NUL;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    static Json boolean(bool v) { Json j; j.type = BOOL; j.b = v; return j; }
    static Json number(double v) { Json j; j.type = NUM; j.n = v; return j; }
    static Json string(const std::string& v) { Json j; j.type = STR; j.s = v; return j; }
    static Json array() { Json j; j.type = ARR; return j; }
    static Json object() { Json j; j.type = OBJ; return j; }

    bool is_null() const { return type == NUL; }
    bool is_obj() const { return type == OBJ; }
    bool is_arr() const { return type == ARR; }
    bool is_num() const { return type == NUM; }
    bool is_str() const { return type == STR; }

    // missing keys and wrong types give back a null / the default, never throw
    const Json& operator[](const char* key) const;
    const Json& operator[](size_t i) const;
    // without this, j[0] is ambiguous: 0 also converts to a null const char*
    const Json& operator[](int i) const { return (*this)[(size_t)i]; }
    Json* find(const char* key);
    bool has(const char* key) const;
    size_t size() const { return type == ARR ? arr.size() : type == OBJ ? obj.size() : 0; }

    double num(double def = 0) const { return type == NUM ? n : def; }
    float f(float def = 0) const { return type == NUM ? (float)n : def; }
    int i(int def = 0) const { return type == NUM ? (int)n : def; }
    bool boolean_or(bool def) const { return type == BOOL ? b : def; }
    const char* str(const char* def = "") const { return type == STR ? s.c_str() : def; }

    Json& set(const char* key, Json v);     // replaces or appends
    void remove(const char* key);
    Json& push(Json v);
};

bool operator==(const Json& a, const Json& b);
inline bool operator!=(const Json& a, const Json& b) { return !(a == b); }

bool json_parse(const std::string& text, Json* out, std::string* err);
std::string json_write(const Json& j, bool pretty = true);

// a's fields overwritten by b's, recursively for nested objects
void json_merge(Json& a, const Json& b);
// fields of b that are missing from a or different, recursively. null if none.
Json json_diff(const Json& a, const Json& b);
