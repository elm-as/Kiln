#include "gl.h"
#include <cstdio>

#define X(type, name) type name;
GL_FUNCS(X)
#undef X

static void* get_proc(HMODULE lib, const char* name)
{
    void* p = (void*)wglGetProcAddress(name);
    // 1.1 functions live in opengl32.dll itself, and some drivers hand back
    // 1, 2, 3 or -1 for them instead of NULL
    if (p == 0 || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1)
        p = (void*)GetProcAddress(lib, name);
    return p;
}

bool gl_load()
{
    HMODULE lib = LoadLibraryA("opengl32.dll");
    bool ok = true;

#define X(type, name) \
    name = (type)get_proc(lib, #name); \
    if (!name) { fprintf(stderr, "gl: missing %s\n", #name); ok = false; }
    GL_FUNCS(X)
#undef X

    return ok;
}
