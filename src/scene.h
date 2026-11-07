#pragma once
#include <string>
#include <vector>
#include "components.h"
#include "tilemap.h"

// Every component that should be saved, loaded from prefabs, or shown in the
// editor gets registered here with a name and a few functions. read() adds
// the component if it's missing and only touches the fields present in the
// json, so prefab data and scene overrides layer on top of each other.
struct ComponentType {
    const char* name;
    bool (*has)(World& w, Entity e);
    void (*read)(World& w, Entity e, const Json& j);
    Json (*write)(World& w, Entity e);
    void (*remove)(World& w, Entity e);
    void (*inspect)(World& w, Entity e);    // editor widgets, can be null
};

void component_register(const ComponentType& t);
const std::vector<ComponentType>& component_types();
const ComponentType* component_type(const char* name);
void register_engine_components();

// shorthand for the common case
template <class T>
ComponentType component_type_for(const char* name, void (*read)(World&, Entity, const Json&),
                                 Json (*write)(World&, Entity), void (*inspect)(World&, Entity))
{
    ComponentType t;
    t.name = name;
    t.has = [](World& w, Entity e) { return w.has<T>(e); };
    t.read = read;
    t.write = write;
    t.remove = [](World& w, Entity e) { w.remove<T>(e); };
    t.inspect = inspect;
    return t;
}

// data/prefabs.json: { "slime": { "Sprite": {...}, "Body": {...} }, ... }
void prefabs_load(const Json& j);
const Json* prefab_get(const char* name);
std::vector<std::string> prefab_names();
Entity prefab_spawn(World& w, const char* name, vec2 pos, const Json* overrides = 0);

struct Scene {
    std::string name;       // file name without .json
    World world;
    Tilemap map;            // collision layer
    Tilemap back;           // decoration behind it
    Json props = Json::object();    // everything else: music, lighting, next level...
};

bool scene_load(Scene& s, const Json& j);
Json scene_save(Scene& s);
// what makes this entity different from a fresh copy of its prefab
Json entity_overrides(World& w, Entity e);

// every visible Sprite, lowest layer first, between last tick and this one
// (alpha), on whole pixels. Whatever's outside view is skipped.
void draw_sprites(World& w, Rect view, float alpha);

// true if p sits inside the inner part (reach, 0..1) of any Light's radius
bool light_reaches(World& w, vec2 p, float reach = 0.75f);

// json helpers the component readers share
vec2 json_vec2(const Json& j, vec2 def);
Json json_from_vec2(vec2 v);
Color json_color(const Json& j, Color def);
Json json_from_color(Color c);
