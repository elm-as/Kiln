#pragma once
#include <string>
#include "ecs.h"
#include "json.h"
#include "render.h"
#include "script.h"

// The components the engine itself knows how to run. Games add their own on
// top, see scene.h for how they get saved and shown in the editor.

struct Transform {
    vec2 pos;
    vec2 prev;          // position at the start of the tick, for interpolation
    float angle = 0;
    vec2 scale = { 1, 1 };
};

struct Sprite {
    int sprite = -1;
    int frame = 0;
    int layer = 0;      // higher draws on top
    vec2 offset;
    bool flip_x = false;
    bool visible = true;
    Color tint = WHITE;
    float flash = 0;    // fades out by itself
};

struct Animator {
    int anim = -1;
    int step = 0;       // index into the anim's frame list
    float time = 0;
    float speed = 1;
    bool done = false;
};

enum BodyType { BODY_STATIC, BODY_KINEMATIC, BODY_DYNAMIC };

struct Body {
    BodyType type = BODY_DYNAMIC;
    vec2 vel;
    float gravity = 1;          // multiplier
    float mass = 1;
    float restitution = 0;
    float friction = 0.3f;
    float max_fall = 420;
    bool tiles = true;          // collide with the tilemap
    bool drop_through = false;  // ignore one-way platforms this tick

    // filled in by the physics step
    bool on_ground = false;
    bool on_ceiling = false;
    int on_wall = 0;            // -1 left, 1 right
    Entity ground = NO_ENTITY;  // what we're standing on, if it's an entity
    vec2 moved;                 // kinematic bodies: how far they went this tick
};

enum Shape { SHAPE_BOX, SHAPE_CIRCLE };

struct Collider {
    Shape shape = SHAPE_BOX;
    vec2 half = { 4, 4 };       // box half size
    float radius = 4;
    vec2 offset;
    uint16_t layer = 1;
    uint16_t mask = 0xffff;
    bool trigger = false;       // reports contacts, never pushes
    bool oneway = false;        // only solid from above (platforms)
};

struct Emitter {
    int fx = -1;
    bool on = true;
    float rate = 1;             // multiplier on the effect's own rate
    vec2 offset;
    float acc = 0;
};

struct Light {
    float radius = 48;
    Color color = { 255, 190, 120, 255 };
    float flicker = 0;          // 0..1
    vec2 offset;
};

struct Script {
    int module = -1;
    Json props = Json::object();    // copied into self when it starts
    Value self;
    bool started = false;
};

// bookkeeping the editor and scripts need
struct Info {
    std::string prefab;
    std::string name;           // optional, for find("door") from scripts
    bool runtime = false;       // spawned while playing, never saved
};
