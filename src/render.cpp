#include "gl.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <vector>
#include "render.h"
#include "platform.h"
#include "image.h"

struct Vertex {
    float x, y;
    float u, v;
    Color c;
    uint8_t flash, pad[3];
};

enum { MAX_QUADS = 8192 };

static Vertex batch[MAX_QUADS * 4];
static int quad_count;
static GLuint batch_tex;

static GLuint vao, vbo, ibo, program;
static GLint u_proj;
Texture g_render_white, g_render_circle, g_render_light;

static Canvas canvas;
static int canvas_w, canvas_h;
static int win_w, win_h;
static Rect canvas_rect;
static bool screen_cleared;

enum Pass { PASS_NONE, PASS_CANVAS, PASS_SCREEN };
static Pass pass;

struct Target {
    GLuint fbo;
    int w, h;
    vec2 view_pos;
    float view_scale;
};
static Target targets[8];
static int target_depth;
static BlendMode blend = BLEND_ALPHA;

static const char* vs_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "layout(location = 3) in float a_flash;\n"
    "uniform mat4 u_proj;\n"
    "out vec2 v_uv; out vec4 v_color; out float v_flash;\n"
    "void main() { v_uv = a_uv; v_color = a_color; v_flash = a_flash; gl_Position = u_proj * vec4(a_pos, 0.0, 1.0); }\n";

static const char* fs_src =
    "#version 330 core\n"
    "in vec2 v_uv; in vec4 v_color; in float v_flash;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main() { vec4 t = texture(u_tex, v_uv); frag = vec4(mix(t.rgb * v_color.rgb, vec3(1.0), v_flash), t.a * v_color.a); }\n";

static GLuint compile(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), 0, log);
        fprintf(stderr, "shader error:\n%s\n", log);
    }
    return s;
}

static Texture make_round_texture(int size, bool soft)
{
    std::vector<uint8_t> px(size_t(size) * size * 4);
    float r = size / 2.f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r;
            float d = sqrtf(dx * dx + dy * dy) / r;
            float a = soft ? clampf(1 - d, 0, 1) : clampf((1 - d) * r, 0, 1);
            if (soft)
                a = a * a * (3 - 2 * a); // smoothstep, squared fell off too early and lights looked tiny
            uint8_t* p = &px[(size_t(y) * size + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = uint8_t(a * 255);
        }
    }
    return texture_create(size, size, px.data(), true);
}

void render_init(int w, int h)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), 0, log);
        fprintf(stderr, "link error:\n%s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    u_proj = glGetUniformLocation(program, "u_proj");
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "u_tex"), 0);

    // indices never change, every quad is 0 1 2 / 2 3 0
    static uint16_t indices[MAX_QUADS * 6];
    for (int i = 0; i < MAX_QUADS; i++) {
        uint16_t b = uint16_t(i * 4);
        uint16_t* p = indices + i * 6;
        p[0] = b; p[1] = b + 1; p[2] = b + 2;
        p[3] = b + 2; p[4] = b + 3; p[5] = b;
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(batch), 0, GL_STREAM_DRAW);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, c));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, flash));

    uint8_t px[4] = { 255, 255, 255, 255 };
    g_render_white = texture_create(1, 1, px);
    g_render_circle = make_round_texture(64, false);
    g_render_light = make_round_texture(128, true);

    canvas_w = w;
    canvas_h = h;
    canvas = canvas_create(w, h);
    glEnable(GL_BLEND);
    render_set_blend(BLEND_ALPHA);
}

void render_shutdown()
{
    canvas_free(&canvas);
    texture_free(&g_render_white); texture_free(&g_render_circle); texture_free(&g_render_light);
    glDeleteBuffers(1, &vbo); glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao); glDeleteProgram(program);
}

void render_flush()
{
    if (!quad_count || !target_depth) return;
    const Target& t = targets[target_depth - 1];
    float sx = 2 * t.view_scale / t.w, sy = -2 * t.view_scale / t.h;
    float proj[16] = { sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, 1, 0, -1 - t.view_pos.x * sx, 1 - t.view_pos.y * sy, 0, 1 };
    glUseProgram(program);
    glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, batch_tex);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(batch), 0, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, quad_count * 4 * sizeof(Vertex), batch);
    glDrawElements(GL_TRIANGLES, quad_count * 6, GL_UNSIGNED_SHORT, 0);
    quad_count = 0;
}

// corners go clockwise from top left
void render_push_quad(GLuint tex, const vec2 p[4], float u0, float v0, float u1, float v1, Color c, float flash)
{
    if (tex != batch_tex || quad_count == MAX_QUADS) {
        render_flush();
        batch_tex = tex;
    }
    uint8_t f = uint8_t(clampf(flash, 0, 1) * 255);
    Vertex* v = batch + quad_count * 4;
    v[0] = { p[0].x, p[0].y, u0, v0, c, f, {} };
    v[1] = { p[1].x, p[1].y, u1, v0, c, f, {} };
    v[2] = { p[2].x, p[2].y, u1, v1, c, f, {} };
    v[3] = { p[3].x, p[3].y, u0, v1, c, f, {} };
    quad_count++;
}

static void bind_target(const Target& t)
{
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glViewport(0, 0, t.w, t.h);
}

void render_frame_begin()
{
    platform_window_size(&win_w, &win_h);
    float scale = fminf(win_w / (float)canvas_w, win_h / (float)canvas_h);
    // whole numbers keep the pixels square when the window is big enough for it
    if (scale >= 1)
        scale = floorf(scale);
    float w = canvas_w * scale, h = canvas_h * scale;
    canvas_rect = { floorf((win_w - w) / 2), floorf((win_h - h) / 2), w, h };

    pass = PASS_NONE;
    screen_cleared = false;
    target_depth = 0;
}

void render_canvas_begin(Color clear)
{
    render_flush();
    pass = PASS_CANVAS;
    target_depth = 1;
    targets[0] = { canvas.fbo, canvas_w, canvas_h, { 0, 0 }, 1 };
    bind_target(targets[0]);
    glClearColor(clear.r / 255.f, clear.g / 255.f, clear.b / 255.f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    render_set_blend(BLEND_ALPHA);
}

void render_screen_begin()
{
    render_flush();
    pass = PASS_SCREEN;
    target_depth = 1;
    targets[0] = { 0, win_w, win_h, { 0, 0 }, 1 };
    bind_target(targets[0]);
    if (!screen_cleared) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        screen_cleared = true;
    }
    render_set_blend(BLEND_ALPHA);
}

void render_present_canvas()
{
    BlendMode old = blend;
    render_set_blend(BLEND_NONE);
    draw_canvas(canvas, canvas_rect);
    render_set_blend(old);
}

void render_frame_end()
{
    if (pass == PASS_CANVAS) {
        render_screen_begin();
        render_present_canvas();
    }
    render_flush();
}

void render_target_push(Canvas* c, Color clear)
{
    render_flush();
    if (target_depth >= 8) {
        fprintf(stderr, "render_target_push: too deep\n");
        return;
    }
    vec2 vp = target_depth > 0 ? targets[target_depth - 1].view_pos : vec2{ 0, 0 };
    float vs = target_depth > 0 ? targets[target_depth - 1].view_scale : 1.0f;
    Target& t = targets[target_depth++];
    t = { c->fbo, c->tex.w, c->tex.h, vp, vs };
    bind_target(t);
    glClearColor(clear.r / 255.f, clear.g / 255.f, clear.b / 255.f, clear.a / 255.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void render_target_pop()
{
    render_flush();
    if (target_depth > 1)
        target_depth--;
    bind_target(targets[target_depth - 1]);
}

void render_set_view(vec2 pos, float scale)
{
    render_flush();
    if (target_depth) {
        targets[target_depth - 1].view_pos = pos;
        targets[target_depth - 1].view_scale = scale;
    }
}

void render_set_blend(BlendMode m)
{
    render_flush();
    blend = m;
    switch (m) {
    case BLEND_ALPHA:
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case BLEND_ADD: glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
    case BLEND_MULTIPLY: glEnable(GL_BLEND); glBlendFunc(GL_DST_COLOR, GL_ZERO); break;
    case BLEND_NONE: glDisable(GL_BLEND); break;
    }
}

vec2 render_target_size()
{
    if (!target_depth) return { (float)win_w, (float)win_h };
    return { (float)targets[target_depth - 1].w, (float)targets[target_depth - 1].h };
}

vec2 render_canvas_size() { return { (float)canvas_w, (float)canvas_h }; }
Rect render_canvas_rect() { return canvas_rect; }

vec2 render_screen_to_canvas(float px, float py)
{
    if (canvas_rect.w <= 0 || canvas_rect.h <= 0) return { 0, 0 };
    return { (px - canvas_rect.x) * canvas_w / canvas_rect.w, (py - canvas_rect.y) * canvas_h / canvas_rect.h };
}

bool render_screenshot(const char* path)
{
    int w = win_w, h = win_h;
    if (w <= 0 || h <= 0) return false;
    render_flush();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::vector<uint8_t> px(size_t(w) * h * 4), flipped(size_t(w) * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    if (target_depth) bind_target(targets[target_depth - 1]);
    size_t row = size_t(w) * 4;
    for (int y = 0; y < h; y++)
        memcpy(&flipped[y * row], &px[(h - 1 - y) * row], row);
    for (size_t i = 3; i < flipped.size(); i += 4)
        flipped[i] = 255;
    return png_write(path, flipped.data(), w, h);
}

uint32_t render_current_fbo() { return target_depth ? targets[target_depth - 1].fbo : 0; }
uint32_t render_batch_tex() { return batch_tex; }
