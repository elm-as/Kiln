#pragma once
#include <cmath>

// zeroed by default: a SpriteOpts with a garbage origin once drew sprites
// wherever the stack said, and only sometimes
struct vec2 {
    float x = 0, y = 0;
};

inline vec2 operator+(vec2 a, vec2 b) { return { a.x + b.x, a.y + b.y }; }
inline vec2 operator-(vec2 a, vec2 b) { return { a.x - b.x, a.y - b.y }; }
inline vec2 operator*(vec2 a, float s) { return { a.x * s, a.y * s }; }
inline vec2 operator*(float s, vec2 a) { return { a.x * s, a.y * s }; }
inline vec2& operator+=(vec2& a, vec2 b) { a.x += b.x; a.y += b.y; return a; }
inline vec2& operator-=(vec2& a, vec2 b) { a.x -= b.x; a.y -= b.y; return a; }

inline float dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(vec2 a) { return sqrtf(dot(a, a)); }

inline vec2 normalize(vec2 a)
{
    float l = length(a);
    return l > 0 ? a * (1 / l) : a;
}

inline vec2 rotate(vec2 a, float r)
{
    float c = cosf(r), s = sinf(r);
    return { a.x * c - a.y * s, a.x * s + a.y * c };
}

struct vec3 {
    float x = 0, y = 0, z = 0;
};

inline vec3 operator+(vec3 a, vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline vec3 operator-(vec3 a, vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline vec3 operator*(vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline vec3 operator*(float s, vec3 a) { return { a.x * s, a.y * s, a.z * s }; }
inline vec3& operator+=(vec3& a, vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline vec3& operator-=(vec3& a, vec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(vec3 a) { return sqrtf(dot(a, a)); }
inline vec3 normalize(vec3 a) {
    float l = length(a);
    return l > 0 ? a * (1.f / l) : a;
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

inline bool overlaps(Rect a, Rect b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

inline bool contains(Rect r, vec2 p)
{
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}
