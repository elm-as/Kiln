// quick checks for the parts that are easy to get subtly wrong.
// build.bat test
#include <cstdio>
#include <cstring>
#include <direct.h>
#include <string>
#include "ecs.h"
#include "json.h"
#include "script.h"
#include "image.h"
#include "actions.h"
#include "camera.h"
#include "cook.h"
#include "fs.h"
#include "editor_paint.h"

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } } while (0)

static std::string run(const char* src)
{
    VM vm;
    static std::string out;
    out.clear();
    vm.on_print = [](const std::string& s) { out += s; out += "\n"; };
    vm.on_error = [](const std::string& s) { out += "ERROR " + s + "\n"; };
    Module m;
    vm.load(m, src, "test");
    return out;
}

static void test_script()
{
    CHECK(run("print(1 + 2 * 3)") == "7\n");
    CHECK(run("print(10 % 3, -1 % 3, 7 / 2)") == "1 2 3.5\n");
    CHECK(run("let s = \"a\" + 1 + true\nprint(s)") == "a1true\n");
    CHECK(run("let x = 0\nfor i in 0..5 { x += i }\nprint(x)") == "10\n");
    CHECK(run("let l = [1, 2, 3]\npush(l, 4)\nprint(len(l), l[3], l)") == "4 4 [1, 2, 3, 4]\n");
    CHECK(run("let t = {a: 1, b: 2}\nt.c = 3\nfor k, v in t { print(k, v) }") == "a 1\nb 2\nc 3\n");
    CHECK(run("fn fib(n) { if n < 2 { return n } return fib(n - 1) + fib(n - 2) }\nprint(fib(15))") == "610\n");
    // closures see the variable, not a copy
    CHECK(run("fn counter() { let n = 0\n return fn() { n += 1\n return n } }\n"
              "let c = counter()\nc()\nc()\nprint(c())") == "3\n");
    // top level names are hoisted
    CHECK(run("fn a() { return b() + total }\nfn b() { return 1 }\nlet total = 10\nprint(a())") == "11\n");
    CHECK(run("let i = 0\nwhile true { i += 1\n if i == 3 { continue }\n if i > 5 { break } }\nprint(i)") == "6\n");
    CHECK(run("if 0 { print(\"zero is truthy\") } else { print(\"no\") }") == "zero is truthy\n");
    CHECK(run("let x = nil\nprint(x == nil, not x, x or 5, 3 and 4)") == "true true 5 4\n");
    CHECK(run("let a = 1\nif a == 2 { print(2) } elif a == 1 { print(1) } else { print(0) }") == "1\n");
    CHECK(run("let t = {}\nprint(t.missing)") == "nil\n");

    // errors carry file and line
    CHECK(run("let a = 1\nlet b = a + nil") == "ERROR test:2: can't add number and nil\n");
    CHECK(run("undefined_thing()") == "ERROR test:1: 'undefined_thing' is not defined\n");
    CHECK(run("while true { }").find("infinite loop") != std::string::npos);
    CHECK(run("fn f() { return f() }\nf()").find("recursion") != std::string::npos);

    std::string err;
    CHECK(!script_check("let = 3", "x.ks", &err) && err == "x.ks:1: expected a name after let");
    CHECK(!script_check("fn f() {\n  y = 2\n}", "x.ks", &err) && err == "x.ks:2: 'y' isn't declared, use let");
    CHECK(!script_check("break", "x.ks", &err));
    CHECK(script_check("fn update(self, dt) { self.x += dt }", "x.ks", &err));

    // calling into a module from C++
    VM vm;
    Module m;
    CHECK(vm.load(m, "let hits = 0\nfn hit(n) { hits += n\n return hits }", "m"));
    Value r;
    vm.call(m.get("hit"), { Value::number(2) }, &r);
    vm.call(m.get("hit"), { Value::number(3) }, &r);
    CHECK(r.type == V_NUM && r.n == 5);
    CHECK(m.get("hits").n == 5);
}

static void test_json()
{
    Json j;
    std::string err;
    CHECK(json_parse("{ \"a\": [1, 2, {\"b\": \"x\\ny\"}], \"c\": true, // comment\n \"d\": null, }", &j, &err));
    CHECK(j["a"][2]["b"].s == "x\ny");
    CHECK(j["c"].b && j["d"].is_null() && j["nope"].is_null());
    CHECK(!json_parse("{\"a\": }", &j, &err) && err.find("line 1") == 0);

    Json a, b;
    json_parse("{\"x\": 1, \"s\": {\"p\": 1, \"q\": 2}}", &a, &err);
    json_parse("{\"x\": 1, \"s\": {\"p\": 1, \"q\": 3}, \"n\": 4}", &b, &err);
    Json d = json_diff(a, b);
    CHECK(json_write(d, false) == "{\"s\":{\"q\":3},\"n\":4}");
    json_merge(a, d);
    CHECK(a == b);

    Json back;
    CHECK(json_parse(json_write(b), &back, &err) && back == b);
}

struct Pos { float x, y; };
struct Vel { float x, y; };

static void test_ecs()
{
    World w;
    Entity a = w.create(), b = w.create(), c = w.create();
    w.add<Pos>(a, { 1, 1 });
    w.add<Pos>(b, { 2, 2 });
    w.add<Vel>(b, { 1, 0 });
    w.add<Pos>(c, { 3, 3 });
    int n = 0;
    w.each<Vel, Pos>([&](Entity, Vel& v, Pos& p) { p.x += v.x; n++; });
    CHECK(n == 1 && w.get<Pos>(b)->x == 3);

    w.destroy(a);
    CHECK(w.alive(a));
    w.flush();
    CHECK(!w.alive(a) && !w.get<Pos>(a) && w.get<Pos>(c)->x == 3);
    Entity d = w.create();      // reuses a's slot with a new generation
    CHECK(entity_index(d) == entity_index(a) && d != a && !w.alive(a));
    CHECK(w.live == 3);
}

static void test_png()
{
    uint8_t px[3 * 2 * 4];
    for (int i = 0; i < 24; i++)
        px[i] = uint8_t(i * 10);
    const char* path = "test_roundtrip.png";
    CHECK(png_write(path, px, 3, 2));
    int w = 0, h = 0;
    uint8_t* back = image_load(path, &w, &h);
    CHECK(back && w == 3 && h == 2 && memcmp(back, px, sizeof(px)) == 0);
    free(back);
    remove(path);
}

// a 2x1 grid sheet and an Aseprite sheet, through the cooker
static void test_sprite_import()
{
    const char* dir = "test_assets";
    make_dir(dir);
    make_dir("test_assets/sprites");

    uint8_t grid[8 * 4 * 4] = {};
    grid[0] = 255, grid[3] = 255;               // top left pixel of frame 0, red
    grid[(4 * 4) + 2] = 255, grid[(4 * 4) + 3] = 255;   // top left of frame 1, blue
    png_write("test_assets/sprites/grid.png", grid, 8, 4);
    const char* grid_json = R"({ "size": [4, 4], "origin": [2, 4], "names": ["a", "b"],
        "anims": { "blink": { "fps": 5, "frames": ["b", 0] } } })";
    write_file("test_assets/sprites/grid.json", grid_json, strlen(grid_json));

    // two 6x6 frames, the second trimmed to 2x2 and packed right after the first
    uint8_t ase[8 * 6 * 4] = {};
    ase[(0 * 8 + 6) * 4 + 1] = 255, ase[(0 * 8 + 6) * 4 + 3] = 255;     // green at (6,0)
    png_write("test_assets/sprites/hero.png", ase, 8, 6);
    const char* ase_json = R"({ "frames": [
        { "filename": "hero 0", "frame": { "x": 0, "y": 0, "w": 6, "h": 6 },
          "spriteSourceSize": { "x": 0, "y": 0, "w": 6, "h": 6 }, "sourceSize": { "w": 6, "h": 6 }, "duration": 125 },
        { "filename": "hero 1", "frame": { "x": 6, "y": 0, "w": 2, "h": 2 },
          "spriteSourceSize": { "x": 3, "y": 4, "w": 2, "h": 2 }, "sourceSize": { "w": 6, "h": 6 }, "duration": 125 },
        { "filename": "hero 2", "frame": { "x": 0, "y": 0, "w": 6, "h": 6 },
          "spriteSourceSize": { "x": 0, "y": 0, "w": 6, "h": 6 }, "sourceSize": { "w": 6, "h": 6 }, "duration": 125 } ],
      "meta": { "frameTags": [ { "name": "walk", "from": 0, "to": 2, "direction": "pingpong" } ],
                "slices": [ { "name": "pivot", "keys": [ { "bounds": { "x": 1, "y": 1, "w": 4, "h": 4 },
                                                            "pivot": { "x": 2, "y": 5 } } ] } ] } })";
    write_file("test_assets/sprites/hero.json", ase_json, strlen(ase_json));

    Pak pak;
    std::string log;
    cook_assets(dir, &pak, &log);
    CHECK(log.find("sprites/") == std::string::npos);
    std::vector<uint8_t>& blob = pak.files["atlas.json"];
    Json atlas;
    std::string err;
    CHECK(json_parse(std::string(blob.begin(), blob.end()), &atlas, &err));
    const Json& g = atlas["sprites"]["grid"];
    CHECK(g["size"][0].i() == 4 && g["frames"].size() == 2 && g["origin"][1].i() == 4);
    CHECK(g["anims"]["blink"]["frames"][0].i() == 1 && g["anims"]["blink"]["fps"].f() == 5);
    const Json& h = atlas["sprites"]["hero"];
    CHECK(h["size"][0].i() == 6 && h["frames"].size() == 3);
    CHECK(h["origin"][0].i() == 3 && h["origin"][1].i() == 6);  // slice bounds + pivot
    // pingpong: 0 1 2 1
    const Json& walk = h["anims"]["walk"]["frames"];
    CHECK(walk.size() == 4 && walk[3].i() == 1 && h["anims"]["walk"]["fps"].f() == 8);

    remove("test_assets/sprites/grid.png");
    remove("test_assets/sprites/grid.json");
    remove("test_assets/sprites/hero.png");
    remove("test_assets/sprites/hero.json");
    _rmdir("test_assets/sprites");
    _rmdir("test_assets");
}

static void test_actions()
{
    int jump = action_define("jump", "saut", { KEY_SPACE, KEY_K });
    int fire = action_define("fire", "tir", { KEY_X, KEY_J, KEY_K });
    Input in = {};
    in.down[KEY_K] = true;
    CHECK(action_down(in, jump) && action_down(in, fire));
    CHECK(action_text("{jump} pour sauter, {nope}") == "espace pour sauter, {nope}");

    // a key belongs to one action: binding K to fire takes it off jump
    action_bind(fire, KEY_K);
    CHECK(action_keys(fire)[0] == KEY_K && action_keys(fire)[1] == KEY_X && action_keys(fire)[2] == KEY_J);
    CHECK(action_keys(jump)[0] == KEY_SPACE && action_keys(jump)[1] == 0);
    // and the first key is what texts show
    action_bind(jump, KEY_N);
    CHECK(action_text("{jump}") == "n");

    Json saved = actions_save();
    actions_reset();
    CHECK(action_keys(jump)[1] == KEY_K);
    actions_load(saved);
    CHECK(action_keys(jump)[0] == KEY_N && action_keys(fire)[0] == KEY_K);
}

static void test_camera()
{
    Camera c;
    c.size = { 100, 50 };
    c.bounds = { 0, 0, 300, 200 };
    // stays inside the level at the edges
    camera_snap(c, { 10, 10 });
    CHECK(c.pos.x == 0 && c.pos.y == 0);
    camera_snap(c, { 290, 190 });
    CHECK(c.pos.x == 200 && c.pos.y == 150);
    // a level narrower than the view is centered
    c.bounds = { 0, 0, 60, 200 };
    camera_snap(c, { 30, 100 });
    CHECK(c.pos.x == -20);
    // easing: gets closer without jumping there
    c.bounds = { 0, 0, 1000, 1000 };
    camera_snap(c, { 100, 100 });
    camera_update(c, { 300, 100 }, 1.f / 60);
    CHECK(c.pos.x > 50 && c.pos.x < 250 && c.prev.x == 50);
}

uint32_t next_component_id()
{
    static uint32_t n;
    return n++;
}

static void test_editor_paint()
{
    Tilemap m;
    m.w = 5;
    m.h = 5;
    m.cells.assign(25, '.');
    CHECK(m.get(0, 0) == '.');

    tile_box_fill(m, 1, 1, 2, 2, '#');
    CHECK(m.get(1, 1) == '#');
    CHECK(m.get(2, 2) == '#');
    CHECK(m.get(0, 0) == '.');
    CHECK(m.get(3, 3) == '.');

    int filled = tile_flood_fill(m, 0, 0, '=');
    CHECK(filled == 21);
    CHECK(m.get(0, 0) == '=');
    CHECK(m.get(1, 1) == '#');

    int island = tile_flood_fill(m, 1, 1, '*');
    CHECK(island == 4);
    CHECK(m.get(2, 2) == '*');
}

int main()
{
    test_script();
    test_json();
    test_ecs();
    test_png();
    test_actions();
    test_camera();
    test_sprite_import();
    test_editor_paint();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
