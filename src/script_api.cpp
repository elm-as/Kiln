#include "script_api.h"
#include <cmath>
#include <map>
#include "anim.h"
#include "assets.h"
#include "audio.h"
#include "engine.h"
#include "particles.h"
#include "physics.h"
#include "scripting.h"

static Scene* scene;
static const Tileset* tiles;

World& script_world()
{
    return scene->world;
}

Entity script_arg_entity(const Value* a, int n, int i)
{
    if (i >= n)
        return NO_ENTITY;
    Entity e = value_entity(a[i]);
    return scene->world.alive(e) ? e : NO_ENTITY;
}

float script_argf(VM& vm, const Value* a, int n, int i, float def)
{
    if (i >= n || a[i].is_nil())
        return def;
    return (float)vm.num(a, n, i);
}

bool script_argb(const Value* a, int n, int i, bool def)
{
    return i < n ? a[i].truthy() : def;
}

template <class T>
static T* comp(const Value* a, int n, int i = 0)
{
    Entity e = script_arg_entity(a, n, i);
    return e ? scene->world.get<T>(e) : 0;
}

static Value num(double d) { return Value::number(d); }
static Value nil() { return Value(); }
#define argf script_argf
#define argb script_argb

static Value n_x(VM&, const Value* a, int n)
{
    Transform* t = comp<Transform>(a, n);
    return num(t ? t->pos.x : 0);
}

static Value n_y(VM&, const Value* a, int n)
{
    Transform* t = comp<Transform>(a, n);
    return num(t ? t->pos.y : 0);
}

static Value n_set_pos(VM& vm, const Value* a, int n)
{
    if (Transform* t = comp<Transform>(a, n))
        t->pos = { argf(vm, a, n, 1, t->pos.x), argf(vm, a, n, 2, t->pos.y) };
    return nil();
}

static Value n_vel_x(VM&, const Value* a, int n)
{
    Body* b = comp<Body>(a, n);
    return num(b ? b->vel.x : 0);
}

static Value n_vel_y(VM&, const Value* a, int n)
{
    Body* b = comp<Body>(a, n);
    return num(b ? b->vel.y : 0);
}

static Value n_set_vel(VM& vm, const Value* a, int n)
{
    if (Body* b = comp<Body>(a, n))
        b->vel = { argf(vm, a, n, 1, 0), argf(vm, a, n, 2, 0) };
    return nil();
}

static Value n_set_gravity(VM& vm, const Value* a, int n)
{
    if (Body* b = comp<Body>(a, n))
        b->gravity = argf(vm, a, n, 1, 1);
    return nil();
}

static Value n_on_ground(VM&, const Value* a, int n)
{
    Body* b = comp<Body>(a, n);
    return Value::boolean(b && b->on_ground);
}

static Value n_on_wall(VM&, const Value* a, int n)
{
    Body* b = comp<Body>(a, n);
    return num(b ? b->on_wall : 0);
}

static Value n_ground_of(VM&, const Value* a, int n)
{
    Body* b = comp<Body>(a, n);
    if (!b || !b->on_ground || !scene->world.alive(b->ground))
        return nil();
    return entity_value(b->ground);
}

static Value n_anim(VM& vm, const Value* a, int n)
{
    Entity e = script_arg_entity(a, n, 0);
    if (e)
        anim_play(scene->world, e, vm.string_arg(a, n, 1).c_str(), argb(a, n, 2, false));
    return nil();
}

static Value n_anim_done(VM&, const Value* a, int n)
{
    Animator* an = comp<Animator>(a, n);
    return Value::boolean(an && an->done);
}

static Value n_flip(VM&, const Value* a, int n)
{
    if (Sprite* s = comp<Sprite>(a, n))
        s->flip_x = argb(a, n, 1, true);
    return nil();
}

static Value n_flash(VM&, const Value* a, int n)
{
    if (Sprite* s = comp<Sprite>(a, n))
        s->flash = 1;
    return nil();
}

static Value n_visible(VM&, const Value* a, int n)
{
    if (Sprite* s = comp<Sprite>(a, n))
        s->visible = argb(a, n, 1, true);
    return nil();
}

static Value n_light(VM& vm, const Value* a, int n)
{
    Entity e = script_arg_entity(a, n, 0);
    if (!e)
        return nil();
    Light* l = scene->world.get<Light>(e);
    if (!l)
        l = &scene->world.add<Light>(e);
    l->radius = argf(vm, a, n, 1, l->radius);
    return nil();
}

static Value n_emit(VM&, const Value* a, int n)
{
    if (Emitter* em = comp<Emitter>(a, n))
        em->on = argb(a, n, 1, true);
    return nil();
}

// turning a collider off keeps its layer and mask somewhere so it can come back
static std::map<Entity, std::pair<uint16_t, uint16_t>> parked;

static Value n_collide(VM&, const Value* a, int n)
{
    Entity e = script_arg_entity(a, n, 0);
    Collider* c = e ? scene->world.get<Collider>(e) : 0;
    if (!c)
        return nil();
    bool on = argb(a, n, 1, true);
    auto it = parked.find(e);
    if (!on && it == parked.end()) {
        parked[e] = { c->layer, c->mask };
        c->layer = c->mask = 0;
    } else if (on && it != parked.end()) {
        c->layer = it->second.first;
        c->mask = it->second.second;
        parked.erase(it);
    }
    return nil();
}

static Value n_alive(VM&, const Value* a, int n)
{
    return Value::boolean(script_arg_entity(a, n, 0) != NO_ENTITY);
}

static Value n_solid(VM& vm, const Value* a, int n)
{
    vec2 p = { argf(vm, a, n, 0, 0), argf(vm, a, n, 1, 0) };
    return Value::boolean(solid_at(scene->map, *tiles, p));
}

// solid or a ledge you can stand on
static Value n_floor_at(VM& vm, const Value* a, int n)
{
    vec2 p = { argf(vm, a, n, 0, 0), argf(vm, a, n, 1, 0) };
    int tx = (int)floorf(p.x / tiles->size), ty = (int)floorf(p.y / tiles->size);
    uint8_t f = tile_flags(scene->map, *tiles, tx, ty);
    return Value::boolean((f & (TILE_SOLID | TILE_ONEWAY)) != 0 || solid_at(scene->map, *tiles, p));
}

static Value n_sfx(VM& vm, const Value* a, int n)
{
    audio_play(sfx(vm.string_arg(a, n, 0).c_str()), argf(vm, a, n, 1, 1), argf(vm, a, n, 2, 1));
    return nil();
}

static Value n_burst(VM& vm, const Value* a, int n)
{
    int fx = particle_fx(vm.string_arg(a, n, 0).c_str());
    int count = n > 3 ? (int)vm.num(a, n, 3) : -1;
    particles_burst(fx, { argf(vm, a, n, 1, 0), argf(vm, a, n, 2, 0) }, count);
    return nil();
}

static Value n_hitstop(VM& vm, const Value* a, int n)
{
    engine_hitstop(argf(vm, a, n, 0, 0.05f));
    return nil();
}

static Value n_destroy(VM&, const Value* a, int n)
{
    Entity e = script_arg_entity(a, n, 0);
    if (e)
        scene->world.destroy(e);
    return nil();
}

static Value n_spawn(VM& vm, const Value* a, int n)
{
    const std::string& name = vm.string_arg(a, n, 0);
    if (!prefab_get(name.c_str()))
        vm.error("no prefab called '" + name + "'");
    Entity e = prefab_spawn(scene->world, name.c_str(), { argf(vm, a, n, 1, 0), argf(vm, a, n, 2, 0) });
    scene->world.get<Info>(e)->runtime = true;
    return entity_value(e);
}

static Value n_find(VM& vm, const Value* a, int n)
{
    const std::string& name = vm.string_arg(a, n, 0);
    Entity found = NO_ENTITY;
    scene->world.each<Info>([&](Entity e, Info& info) {
        if (info.name == name)
            found = e;
    });
    return found ? entity_value(found) : nil();
}

static Value n_dist(VM&, const Value* a, int n)
{
    Transform* p = comp<Transform>(a, n, 0);
    Transform* q = comp<Transform>(a, n, 1);
    if (!p || !q)
        return num(1e9);
    return num(length(p->pos - q->pos));
}

void script_api_reset()
{
    // entity handles get reused once a level is reloaded
    parked.clear();
    script_flags_clear();
}

void script_api_init(Scene* s, const Tileset* ts)
{
    scene = s;
    tiles = ts;
    VM& vm = script_vm();
    vm.def("x", n_x);
    vm.def("y", n_y);
    vm.def("set_pos", n_set_pos);
    vm.def("vel_x", n_vel_x);
    vm.def("vel_y", n_vel_y);
    vm.def("set_vel", n_set_vel);
    vm.def("set_gravity", n_set_gravity);
    vm.def("on_ground", n_on_ground);
    vm.def("on_wall", n_on_wall);
    vm.def("ground_of", n_ground_of);
    vm.def("anim", n_anim);
    vm.def("anim_done", n_anim_done);
    vm.def("flip", n_flip);
    vm.def("flash", n_flash);
    vm.def("visible", n_visible);
    vm.def("light", n_light);
    vm.def("emit", n_emit);
    vm.def("collide", n_collide);
    vm.def("alive", n_alive);
    vm.def("solid", n_solid);
    vm.def("floor_at", n_floor_at);
    vm.def("sfx", n_sfx);
    vm.def("burst", n_burst);
    vm.def("hitstop", n_hitstop);
    vm.def("destroy", n_destroy);
    vm.def("spawn", n_spawn);
    vm.def("find", n_find);
    vm.def("dist", n_dist);
}
