#include "scene.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "anim.h"
#include "assets.h"
#include "particles.h"
#include "scripting.h"
#include "ui.h"

static std::vector<ComponentType> types;

void component_register(const ComponentType& t)
{
    for (ComponentType& existing : types) {
        if (!strcmp(existing.name, t.name)) {
            existing = t;
            return;
        }
    }
    types.push_back(t);
}

const std::vector<ComponentType>& component_types() { return types; }

const ComponentType* component_type(const char* name)
{
    for (const ComponentType& t : types)
        if (!strcmp(t.name, name)) return &t;
    return 0;
}

vec2 json_vec2(const Json& j, vec2 def)
{
    return j.size() == 2 ? vec2{ j[0].f(), j[1].f() } : def;
}

Json json_from_vec2(vec2 v)
{
    Json a = Json::array();
    a.push(Json::number(v.x));
    a.push(Json::number(v.y));
    return a;
}

Color json_color(const Json& j, Color def)
{
    if (!j.is_str()) return def;
    const char* s = j.str();
    if (*s == '#') s++;
    size_t n = strlen(s);
    unsigned long v = strtoul(s, 0, 16);
    if (n == 6) return rgb((uint32_t)v);
    if (n == 8) return { uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v) };
    return def;
}

Json json_from_color(Color c)
{
    char buf[16];
    if (c.a == 255) snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
    else snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
    return Json::string(buf);
}

// read helpers: only assign when the key is there
static void rd(const Json& j, const char* k, float& v) { if (j[k].is_num()) v = j[k].f(); }
static void rd(const Json& j, const char* k, int& v) { if (j[k].is_num()) v = j[k].i(); }
static void rd(const Json& j, const char* k, bool& v) { if (j[k].type == Json::BOOL) v = j[k].b; }
static void rd(const Json& j, const char* k, vec2& v) { if (j.has(k)) v = json_vec2(j[k], v); }
static void rd(const Json& j, const char* k, Color& v) { if (j.has(k)) v = json_color(j[k], v); }

// ---------------------------------------------------------------------------
// engine components

static void transform_read(World& w, Entity e, const Json& j)
{
    Transform& t = w.has<Transform>(e) ? *w.get<Transform>(e) : w.add<Transform>(e);
    rd(j, "pos", t.pos); rd(j, "angle", t.angle); rd(j, "scale", t.scale);
    t.prev = t.pos;
}

static Json transform_write(World& w, Entity e)
{
    Transform& t = *w.get<Transform>(e);
    Json j = Json::object();
    j.set("pos", json_from_vec2(t.pos));
    if (t.angle != 0) j.set("angle", Json::number(t.angle));
    if (t.scale.x != 1 || t.scale.y != 1) j.set("scale", json_from_vec2(t.scale));
    return j;
}

static void transform_inspect(World& w, Entity e)
{
    Transform& t = *w.get<Transform>(e);
    if (ui_drag_vec2("pos", &t.pos, 0.5f)) t.prev = t.pos;
    ui_drag_float("angle", &t.angle, 0.02f);
}

static void sprite_read(World& w, Entity e, const Json& j)
{
    Sprite& s = w.has<Sprite>(e) ? *w.get<Sprite>(e) : w.add<Sprite>(e);
    if (j["sprite"].is_str()) s.sprite = sprite_id(j["sprite"].str());
    if (j["frame"].is_str()) {
        const SpriteSheet* sh = sprite_get(s.sprite);
        s.frame = sh ? std::max(0, sh->frame(j["frame"].str())) : 0;
    } else {
        rd(j, "frame", s.frame);
    }
    rd(j, "layer", s.layer); rd(j, "offset", s.offset);
    rd(j, "flip", s.flip_x); rd(j, "visible", s.visible); rd(j, "tint", s.tint);
}

static Json sprite_write(World& w, Entity e)
{
    Sprite& s = *w.get<Sprite>(e);
    Json j = Json::object();
    const SpriteSheet* sh = sprite_get(s.sprite);
    j.set("sprite", Json::string(sh ? sh->name : ""));
    j.set("frame", Json::number(s.frame));
    j.set("layer", Json::number(s.layer));
    if (s.offset.x || s.offset.y) j.set("offset", json_from_vec2(s.offset));
    j.set("flip", Json::boolean(s.flip_x));
    if (!s.visible) j.set("visible", Json::boolean(false));
    if (s.tint.r != 255 || s.tint.g != 255 || s.tint.b != 255 || s.tint.a != 255)
        j.set("tint", json_from_color(s.tint));
    return j;
}

static void sprite_inspect(World& w, Entity e)
{
    Sprite& s = *w.get<Sprite>(e);
    const SpriteSheet* sh = sprite_get(s.sprite);
    std::string name = sh ? sh->name : "";
    if (ui_input_text("sprite", &name)) s.sprite = sprite_id(name.c_str());
    ui_drag_int("frame", &s.frame); ui_drag_int("layer", &s.layer);
    ui_drag_vec2("offset", &s.offset); ui_checkbox("flip", &s.flip_x); ui_color("tint", &s.tint);
}

static void animator_read(World& w, Entity e, const Json& j)
{
    Animator& a = w.has<Animator>(e) ? *w.get<Animator>(e) : w.add<Animator>(e);
    rd(j, "speed", a.speed);
    if (j["play"].is_str()) { a.anim = -1; anim_play(w, e, j["play"].str(), true); }
}

static Json animator_write(World& w, Entity e)
{
    Animator& a = *w.get<Animator>(e);
    Json j = Json::object();
    Sprite* s = w.get<Sprite>(e);
    const SpriteSheet* sh = s ? sprite_get(s->sprite) : 0;
    if (sh && a.anim >= 0 && a.anim < (int)sh->anims.size())
        j.set("play", Json::string(sh->anims[a.anim].name));
    if (a.speed != 1) j.set("speed", Json::number(a.speed));
    return j;
}

static void animator_inspect(World& w, Entity e)
{
    Animator& a = *w.get<Animator>(e);
    Sprite* s = w.get<Sprite>(e);
    if (const SpriteSheet* sh = s ? sprite_get(s->sprite) : 0) {
        for (size_t i = 0; i < sh->anims.size(); i++)
            if (ui_selectable(sh->anims[i].name.c_str(), (int)i == a.anim))
                anim_play(w, e, sh->anims[i].name.c_str(), true);
    }
    ui_drag_float("speed", &a.speed, 0.01f);
}

static const char* body_types[] = { "static", "kinematic", "dynamic" };

static void body_read(World& w, Entity e, const Json& j)
{
    Body& b = w.has<Body>(e) ? *w.get<Body>(e) : w.add<Body>(e);
    if (j["type"].is_str())
        for (int i = 0; i < 3; i++)
            if (!strcmp(j["type"].str(), body_types[i])) b.type = (BodyType)i;
    rd(j, "vel", b.vel); rd(j, "gravity", b.gravity); rd(j, "mass", b.mass);
    rd(j, "restitution", b.restitution); rd(j, "friction", b.friction);
    rd(j, "max_fall", b.max_fall); rd(j, "tiles", b.tiles);
}

static Json body_write(World& w, Entity e)
{
    Body& b = *w.get<Body>(e);
    Json j = Json::object();
    j.set("type", Json::string(body_types[b.type]));
    if (b.vel.x || b.vel.y) j.set("vel", json_from_vec2(b.vel));
    j.set("gravity", Json::number(b.gravity)); j.set("mass", Json::number(b.mass));
    j.set("restitution", Json::number(b.restitution)); j.set("friction", Json::number(b.friction));
    j.set("max_fall", Json::number(b.max_fall)); j.set("tiles", Json::boolean(b.tiles));
    return j;
}

static void body_inspect(World& w, Entity e)
{
    Body& b = *w.get<Body>(e);
    for (int i = 0; i < 3; i++) {
        if (i) ui_same_line();
        if (ui_button(b.type == i ? (std::string("[") + body_types[i] + "]").c_str() : body_types[i]))
            b.type = (BodyType)i;
    }
    ui_drag_vec2("vel", &b.vel); ui_drag_float("gravity", &b.gravity, 0.01f);
    ui_drag_float("mass", &b.mass, 0.01f); ui_drag_float("restitution", &b.restitution, 0.01f);
    ui_drag_float("friction", &b.friction, 0.01f); ui_checkbox("tiles", &b.tiles);
    ui_label("ground %d  wall %d", b.on_ground, b.on_wall);
}

static void collider_read(World& w, Entity e, const Json& j)
{
    Collider& c = w.has<Collider>(e) ? *w.get<Collider>(e) : w.add<Collider>(e);
    if (j["shape"].is_str())
        c.shape = strcmp(j["shape"].str(), "circle") ? SHAPE_BOX : SHAPE_CIRCLE;
    rd(j, "half", c.half);
    if (j["size"].size() == 2)
        c.half = json_vec2(j["size"], c.half * 2) * 0.5f;
    rd(j, "radius", c.radius); rd(j, "offset", c.offset);
    if (j["layer"].is_num()) c.layer = (uint16_t)j["layer"].i();
    if (j["mask"].is_num()) c.mask = (uint16_t)j["mask"].i();
    rd(j, "trigger", c.trigger); rd(j, "oneway", c.oneway);
}

static Json collider_write(World& w, Entity e)
{
    Collider& c = *w.get<Collider>(e);
    Json j = Json::object();
    if (c.shape == SHAPE_CIRCLE) {
        j.set("shape", Json::string("circle"));
        j.set("radius", Json::number(c.radius));
    } else {
        j.set("size", json_from_vec2(c.half * 2));
    }
    if (c.offset.x || c.offset.y) j.set("offset", json_from_vec2(c.offset));
    j.set("layer", Json::number(c.layer)); j.set("mask", Json::number(c.mask));
    j.set("trigger", Json::boolean(c.trigger)); j.set("oneway", Json::boolean(c.oneway));
    return j;
}

static void collider_inspect(World& w, Entity e)
{
    Collider& c = *w.get<Collider>(e);
    bool circle = c.shape == SHAPE_CIRCLE;
    if (ui_checkbox("circle", &circle)) c.shape = circle ? SHAPE_CIRCLE : SHAPE_BOX;
    if (circle) {
        ui_drag_float("radius", &c.radius, 0.1f);
    } else {
        vec2 size = c.half * 2;
        if (ui_drag_vec2("size", &size, 0.1f)) c.half = size * 0.5f;
    }
    ui_drag_vec2("offset", &c.offset, 0.1f);
    int layer = c.layer, mask = c.mask;
    if (ui_drag_int("layer", &layer)) c.layer = (uint16_t)layer;
    if (ui_drag_int("mask", &mask)) c.mask = (uint16_t)mask;
    ui_checkbox("trigger", &c.trigger); ui_same_line(); ui_checkbox("oneway", &c.oneway);
}

static void emitter_read(World& w, Entity e, const Json& j)
{
    Emitter& em = w.has<Emitter>(e) ? *w.get<Emitter>(e) : w.add<Emitter>(e);
    if (j["fx"].is_str()) em.fx = particle_fx(j["fx"].str());
    rd(j, "on", em.on); rd(j, "rate", em.rate); rd(j, "offset", em.offset);
}

static Json emitter_write(World& w, Entity e)
{
    Emitter& em = *w.get<Emitter>(e);
    Json j = Json::object();
    j.set("fx", Json::string(particle_fx_name(em.fx)));
    j.set("on", Json::boolean(em.on)); j.set("rate", Json::number(em.rate));
    if (em.offset.x || em.offset.y) j.set("offset", json_from_vec2(em.offset));
    return j;
}

static void emitter_inspect(World& w, Entity e)
{
    Emitter& em = *w.get<Emitter>(e);
    std::string name = particle_fx_name(em.fx);
    if (ui_input_text("fx", &name)) em.fx = particle_fx(name.c_str());
    ui_checkbox("on", &em.on); ui_drag_float("rate", &em.rate, 0.01f); ui_drag_vec2("offset", &em.offset);
}

static void light_read(World& w, Entity e, const Json& j)
{
    Light& l = w.has<Light>(e) ? *w.get<Light>(e) : w.add<Light>(e);
    rd(j, "radius", l.radius); rd(j, "color", l.color); rd(j, "flicker", l.flicker); rd(j, "offset", l.offset);
}

static Json light_write(World& w, Entity e)
{
    Light& l = *w.get<Light>(e);
    Json j = Json::object();
    j.set("radius", Json::number(l.radius)); j.set("color", json_from_color(l.color));
    j.set("flicker", Json::number(l.flicker));
    if (l.offset.x || l.offset.y) j.set("offset", json_from_vec2(l.offset));
    return j;
}

static void light_inspect(World& w, Entity e)
{
    Light& l = *w.get<Light>(e);
    ui_drag_float("radius", &l.radius); ui_color("color", &l.color);
    ui_drag_float("flicker", &l.flicker, 0.01f); ui_drag_vec2("offset", &l.offset);
}

static void script_read(World& w, Entity e, const Json& j)
{
    Script& s = w.has<Script>(e) ? *w.get<Script>(e) : w.add<Script>(e);
    if (j["module"].is_str()) s.module = script_module(j["module"].str());
    if (j["props"].is_obj()) json_merge(s.props, j["props"]);
}

static Json script_write(World& w, Entity e)
{
    Script& s = *w.get<Script>(e);
    Json j = Json::object();
    j.set("module", Json::string(script_module_name(s.module)));
    j.set("props", s.props);
    return j;
}

static void script_inspect(World& w, Entity e)
{
    Script& s = *w.get<Script>(e);
    ui_label("module: %s", script_module_name(s.module));
    for (auto& kv : s.props.obj) {
        const char* k = kv.first.c_str();
        Json& v = kv.second;
        if (v.is_num()) {
            float f = (float)v.n;
            if (ui_drag_float(k, &f, 0.1f)) v.n = f;
        } else if (v.type == Json::BOOL) {
            ui_checkbox(k, &v.b);
        } else if (v.is_str()) {
            ui_input_text(k, &v.s);
        } else {
            ui_label("%s: %s", k, json_write(v, false).c_str());
        }
    }
}

void register_engine_components()
{
    component_register(component_type_for<Transform>("Transform", transform_read, transform_write, transform_inspect));
    component_register(component_type_for<Sprite>("Sprite", sprite_read, sprite_write, sprite_inspect));
    component_register(component_type_for<Animator>("Animator", animator_read, animator_write, animator_inspect));
    component_register(component_type_for<Body>("Body", body_read, body_write, body_inspect));
    component_register(component_type_for<Collider>("Collider", collider_read, collider_write, collider_inspect));
    component_register(component_type_for<Emitter>("Emitter", emitter_read, emitter_write, emitter_inspect));
    component_register(component_type_for<Light>("Light", light_read, light_write, light_inspect));
    component_register(component_type_for<Script>("Script", script_read, script_write, script_inspect));
}
