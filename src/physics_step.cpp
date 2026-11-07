#include "physics.h"
#include <algorithm>
#include <cmath>

void physics_move_body(Transform& t, Body& b, const Collider& c, vec2 d, const Tilemap& m, const Tileset& ts);
void physics_settle(Transform& t, Body& b, const Collider& c, const Tilemap& m, const Tileset& ts);

struct Item {
    Entity e;
    Transform* t;
    Collider* c;
    Body* b;
    Rect r;
};

static float inv_mass(const Item& it)
{
    if (!it.b || it.b->type != BODY_DYNAMIC || it.b->mass <= 0)
        return 0;
    return 1 / it.b->mass;
}

static bool narrow(const Item& A, const Item& B, vec2* n, float* depth)
{
    const Collider& a = *A.c;
    const Collider& b = *B.c;
    vec2 pa = A.t->pos + a.offset;
    vec2 pb = B.t->pos + b.offset;

    if (a.shape == SHAPE_BOX && b.shape == SHAPE_BOX) {
        vec2 d = pb - pa;
        float ox = a.half.x + b.half.x - fabsf(d.x);
        float oy = a.half.y + b.half.y - fabsf(d.y);
        if (ox <= 0 || oy <= 0)
            return false;
        // If they were apart vertically last tick, this is a landing or a bonk,
        // whatever the overlap says. Stops boxes catching on each other's corners.
        vec2 pd = (B.t->prev + b.offset) - (A.t->prev + a.offset);
        float pox = a.half.x + b.half.x - fabsf(pd.x);
        float poy = a.half.y + b.half.y - fabsf(pd.y);
        bool use_y;
        if (poy <= 0.01f) use_y = true;
        else if (pox <= 0.01f) use_y = false;
        else use_y = oy < ox;
        if (use_y) {
            *n = { 0, d.y < 0 ? -1.f : 1.f };
            *depth = oy;
        } else {
            *n = { d.x < 0 ? -1.f : 1.f, 0 };
            *depth = ox;
        }
        return true;
    }

    if (a.shape == SHAPE_CIRCLE && b.shape == SHAPE_CIRCLE) {
        vec2 d = pb - pa;
        float r = a.radius + b.radius;
        float dist2 = dot(d, d);
        if (dist2 >= r * r)
            return false;
        float dist = sqrtf(dist2);
        *n = dist > 0 ? d * (1 / dist) : vec2{ 0, 1 };
        *depth = r - dist;
        return true;
    }

    // box against circle, worked out with the box first
    bool flip = a.shape == SHAPE_CIRCLE;
    const Collider& box = flip ? b : a;
    const Collider& cir = flip ? a : b;
    vec2 bc = flip ? pb : pa;
    vec2 cc = flip ? pa : pb;
    vec2 d = cc - bc;
    vec2 closest = { clampf(d.x, -box.half.x, box.half.x), clampf(d.y, -box.half.y, box.half.y) };
    if (closest.x == d.x && closest.y == d.y) {
        float ox = box.half.x - fabsf(d.x);
        float oy = box.half.y - fabsf(d.y);
        if (ox < oy) {
            *n = { d.x < 0 ? -1.f : 1.f, 0 };
            *depth = ox + cir.radius;
        } else {
            *n = { 0, d.y < 0 ? -1.f : 1.f };
            *depth = oy + cir.radius;
        }
    } else {
        vec2 diff = d - closest;
        float dist = length(diff);
        if (dist >= cir.radius)
            return false;
        *n = diff * (1 / dist);
        *depth = cir.radius - dist;
    }
    if (flip)
        *n = *n * -1;
    return true;
}

// one-way colliders only stop things coming down onto them from above
static bool oneway_blocks(const Item& plat, const Item& other, vec2 n_plat_to_other)
{
    if (n_plat_to_other.y > -0.5f)
        return false;
    if (other.b && other.b->drop_through)
        return false;
    Transform prev_other = *other.t;
    prev_other.pos = other.t->prev;
    Transform prev_plat = *plat.t;
    prev_plat.pos = plat.t->prev;
    Rect was_o = collider_rect(prev_other, *other.c);
    Rect was_p = collider_rect(prev_plat, *plat.c);
    return was_o.y + was_o.h <= was_p.y + 1.5f;
}

static void resolve(Item& A, Item& B, vec2 n, float depth)
{
    float ia = inv_mass(A), ib = inv_mass(B);
    if (ia + ib <= 0)
        return;

    vec2 corr = n * (depth / (ia + ib));
    A.t->pos -= corr * ia;
    B.t->pos += corr * ib;

    vec2 va = A.b ? A.b->vel : vec2{ 0, 0 };
    vec2 vb = B.b ? B.b->vel : vec2{ 0, 0 };
    vec2 rv = vb - va;
    float vn = dot(rv, n);
    if (vn < 0) {
        float e = std::min(A.b ? A.b->restitution : 0.f, B.b ? B.b->restitution : 0.f);
        float j = -(1 + e) * vn / (ia + ib);
        vec2 imp = n * j;
        if (ia > 0) A.b->vel -= imp * ia;
        if (ib > 0) B.b->vel += imp * ib;

        vec2 tan = rv - n * vn;
        float tl = length(tan);
        if (tl > 0.0001f) {
            tan = tan * (1 / tl);
            float mu = sqrtf((A.b ? A.b->friction : 0.5f) * (B.b ? B.b->friction : 0.5f));
            float jt = clampf(-dot(rv, tan) / (ia + ib), -j * mu, j * mu);
            if (ia > 0) A.b->vel -= tan * (jt * ia);
            if (ib > 0) B.b->vel += tan * (jt * ib);
        }
    }

    // normal points from A to B, so pointing down means A sits on top of B
    if (n.y > 0.7f && A.b) {
        A.b->on_ground = true;
        A.b->ground = B.e;
    } else if (n.y < -0.7f && B.b) {
        B.b->on_ground = true;
        B.b->ground = A.e;
    }
}

void physics_step(World& w, const Tilemap& map, const Tileset& ts, const PhysicsConfig& cfg,
                  float dt, std::vector<Contact>& contacts)
{
    contacts.clear();

    w.each<Transform>([&](Entity, Transform& t) { t.prev = t.pos; });

    w.each<Body, Transform>([&](Entity, Body& b, Transform& t) {
        if (b.type == BODY_KINEMATIC) {
            b.moved = b.vel * dt;
            t.pos += b.moved;
        } else {
            b.moved = { 0, 0 };
        }
    });

    w.each<Body, Transform>([&](Entity e, Body& b, Transform& t) {
        if (b.type != BODY_DYNAMIC)
            return;

        // ride whatever kinematic thing we were standing on
        if (b.ground && w.alive(b.ground)) {
            Body* g = w.get<Body>(b.ground);
            if (g && g->type == BODY_KINEMATIC)
                t.pos += g->moved;
        }

        b.vel.y += cfg.gravity * b.gravity * dt;
        if (b.vel.y > b.max_fall)
            b.vel.y = b.max_fall;

        b.on_ground = b.on_ceiling = false;
        b.on_wall = 0;
        b.ground = NO_ENTITY;

        Collider* c = w.get<Collider>(e);
        if (c && b.tiles && !c->trigger)
            physics_move_body(t, b, *c, b.vel * dt, map, ts);
        else
            t.pos += b.vel * dt;
    });

    // sort and sweep on x
    std::vector<Item> items;
    items.reserve(w.count<Collider>());
    w.each<Collider, Transform>([&](Entity e, Collider& c, Transform& t) {
        items.push_back({ e, &t, &c, w.get<Body>(e), collider_rect(t, c) });
    });
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.r.x < b.r.x; });

    for (size_t i = 0; i < items.size(); i++) {
        Item& A = items[i];
        for (size_t j = i + 1; j < items.size(); j++) {
            Item& B = items[j];
            if (B.r.x > A.r.x + A.r.w)
                break;
            if (B.r.y > A.r.y + A.r.h || A.r.y > B.r.y + B.r.h)
                continue;
            bool ab = (A.c->layer & B.c->mask) != 0;
            bool ba = (B.c->layer & A.c->mask) != 0;
            if (!ab && !ba)
                continue;

            vec2 n;
            float depth;
            if (!narrow(A, B, &n, &depth))
                continue;

            // if only one side wants to hit the other, it's reported but nobody gets pushed
            bool trigger = A.c->trigger || B.c->trigger || !(ab && ba);
            if (!trigger) {
                if (A.c->oneway && !oneway_blocks(A, B, n))
                    continue;
                if (B.c->oneway && !oneway_blocks(B, A, n * -1))
                    continue;
            }

            contacts.push_back({ A.e, B.e, n, depth, trigger });
            if (!trigger)
                resolve(A, B, n, depth);
        }
    }

    // the body pass can shove things into walls, the map has the final say
    w.each<Body, Transform>([&](Entity e, Body& b, Transform& t) {
        if (b.type != BODY_DYNAMIC || !b.tiles)
            return;
        if (Collider* c = w.get<Collider>(e))
            if (!c->trigger)
                physics_settle(t, b, *c, map, ts);
    });
}

