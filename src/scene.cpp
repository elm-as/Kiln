#include "scene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include "assets.h"

static Json prefabs = Json::object();

void prefabs_load(const Json& j)
{
    prefabs = j;
}

const Json* prefab_get(const char* name)
{
    const Json& p = prefabs[name];
    return p.is_obj() ? &p : 0;
}

std::vector<std::string> prefab_names()
{
    std::vector<std::string> out;
    for (const auto& kv : prefabs.obj)
        if (kv.first[0] != '_')     // "_comment" style keys
            out.push_back(kv.first);
    return out;
}

static void apply(World& w, Entity e, const Json& comps)
{
    static std::set<std::string> warned;
    for (const auto& kv : comps.obj) {
        if (kv.first[0] == '_')
            continue;
        const ComponentType* t = component_type(kv.first.c_str());
        if (!t) {
            if (warned.insert(kv.first).second)
                fprintf(stderr, "no component type called '%s'\n", kv.first.c_str());
            continue;
        }
        t->read(w, e, kv.second);
    }
}

void draw_sprites(World& w, Rect view, float alpha)
{
    struct Item {
        int layer;
        Entity e;
    };
    static std::vector<Item> items;
    items.clear();
    w.each<Sprite, Transform>([&](Entity e, Sprite& spr, Transform&) {
        if (spr.visible)
            items.push_back({ spr.layer, e });
    });
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.layer < b.layer; });

    for (const Item& it : items) {
        Sprite& spr = *w.get<Sprite>(it.e);
        Transform& t = *w.get<Transform>(it.e);
        const SpriteSheet* sh = sprite_get(spr.sprite);
        if (!sh)
            continue;
        vec2 p = { lerp(t.prev.x, t.pos.x, alpha), lerp(t.prev.y, t.pos.y, alpha) };
        p += spr.offset;
        // whole pixels, or sprites shimmer against the tiles as the camera moves
        p = { floorf(p.x + 0.5f), floorf(p.y + 0.5f) };
        if (p.x + sh->w < view.x || p.x - sh->w > view.x + view.w || p.y + sh->h < view.y || p.y - sh->h > view.y + view.h)
            continue;
        SpriteOpts o;
        o.origin = sh->origin;
        o.scale = t.scale;
        o.angle = t.angle;
        o.flip_x = spr.flip_x;
        o.tint = spr.tint;
        o.flash = spr.flash;
        draw_spr_ex(spr.sprite, spr.frame, p, o);
    }
}

bool light_reaches(World& w, vec2 p, float reach)
{
    bool lit = false;
    w.each<Light, Transform>([&](Entity, Light& l, Transform& t) {
        if (lit || l.radius <= 0)
            return;
        vec2 d = p - (t.pos + l.offset);
        float r = l.radius * reach;
        if (d.x * d.x + d.y * d.y < r * r)
            lit = true;
    });
    return lit;
}

Entity prefab_spawn(World& w, const char* name, vec2 pos, const Json* overrides)
{
    Entity e = w.create();
    Info& info = w.add<Info>(e);
    info.prefab = name;
    Transform& t = w.add<Transform>(e);
    t.pos = t.prev = pos;

    const Json* p = prefab_get(name);
    if (p)
        apply(w, e, *p);
    else
        fprintf(stderr, "no prefab called '%s'\n", name);
    if (overrides)
        apply(w, e, *overrides);

    // the prefab might carry a Transform of its own, the position we were given wins
    Transform* tp = w.get<Transform>(e);
    tp->pos = tp->prev = pos;
    return e;
}

Json entity_overrides(World& w, Entity e)
{
    Info* info = w.get<Info>(e);
    static World scratch;
    scratch.clear();
    Entity base = prefab_spawn(scratch, info ? info->prefab.c_str() : "", { 0, 0 });

    Json out = Json::object();
    for (const ComponentType& t : component_types()) {
        if (!strcmp(t.name, "Transform") || !t.has(w, e))
            continue;
        Json mine = t.write(w, e);
        if (!t.has(scratch, base)) {
            out.set(t.name, mine);
            continue;
        }
        Json d = json_diff(t.write(scratch, base), mine);
        if (!d.is_null() && d.size())
            out.set(t.name, d);
    }
    return out;
}

// ---------------------------------------------------------------------------
// scenes

bool scene_load(Scene& s, const Json& j)
{
    s.world.clear();
    s.map.load(j["tiles"]);
    s.back.load(j["back"]);
    if (s.back.w != s.map.w || s.back.h != s.map.h)
        s.back.resize(s.map.w, s.map.h);
    s.props = j["props"].is_obj() ? j["props"] : Json::object();

    for (const Json& ej : j["entities"].arr) {
        const Json* over = ej["set"].is_obj() ? &ej["set"] : 0;
        Entity e = prefab_spawn(s.world, ej["prefab"].str(), json_vec2(ej["pos"], { 0, 0 }), over);
        if (ej["props"].is_obj() && s.world.has<Script>(e)) {
            Script* sc = s.world.get<Script>(e);
            for (const auto& kv : ej["props"].obj)
                sc->props.set(kv.first.c_str(), kv.second);
        }
        if (ej["name"].is_str())
            s.world.get<Info>(e)->name = ej["name"].s;
    }
    return true;
}

Json scene_save(Scene& s)
{
    Json j = Json::object();
    j.set("props", s.props);
    j.set("tiles", s.map.save());
    j.set("back", s.back.save());

    // save in the order they were created, so files don't reshuffle on every save
    std::vector<Entity> list;
    s.world.each<Info>([&](Entity e, Info& info) {
        if (!info.runtime)
            list.push_back(e);
    });
    std::sort(list.begin(), list.end(), [](Entity a, Entity b) { return entity_index(a) < entity_index(b); });

    Json ents = Json::array();
    for (Entity e : list) {
        Info* info = s.world.get<Info>(e);
        Transform* t = s.world.get<Transform>(e);
        Json ej = Json::object();
        ej.set("prefab", Json::string(info->prefab));
        vec2 p = t ? t->pos : vec2{ 0, 0 };
        ej.set("pos", json_from_vec2({ floorf(p.x * 10 + 0.5f) / 10, floorf(p.y * 10 + 0.5f) / 10 }));
        if (!info->name.empty())
            ej.set("name", Json::string(info->name));
        Json over = entity_overrides(s.world, e);
        if (over.size())
            ej.set("set", over);
        ents.push(ej);
    }
    j.set("entities", ents);
    return j;
}
