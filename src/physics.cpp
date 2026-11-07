#include "physics.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

Rect collider_rect(const Transform& t, const Collider& c)
{
    vec2 p = t.pos + c.offset;
    if (c.shape == SHAPE_CIRCLE)
        return { p.x - c.radius, p.y - c.radius, c.radius * 2, c.radius * 2 };
    return { p.x - c.half.x, p.y - c.half.y, c.half.x * 2, c.half.y * 2 };
}

static uint8_t flags_at(const Tilemap& m, const Tileset& ts, int x, int y)
{
    if (x < 0 || x >= m.w) return TILE_SOLID;
    if (y < 0 || y >= m.h) return 0;
    return tile_flags(m, ts, x, y);
}

static void tile_range(const Tileset& ts, Rect r, int* x0, int* y0, int* x1, int* y1)
{
    float s = (float)ts.size;
    *x0 = (int)floorf(r.x / s);
    *y0 = (int)floorf(r.y / s);
    *x1 = (int)floorf((r.x + r.w - 0.001f) / s);
    *y1 = (int)floorf((r.y + r.h - 0.001f) / s);
}

bool solid_at(const Tilemap& m, const Tileset& ts, vec2 p)
{
    return flags_at(m, ts, (int)floorf(p.x / ts.size), (int)floorf(p.y / ts.size)) & TILE_SOLID;
}

bool rect_hits_solid(const Tilemap& m, const Tileset& ts, Rect r)
{
    int x0, y0, x1, y1;
    tile_range(ts, r, &x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (flags_at(m, ts, x, y) & TILE_SOLID)
                return true;
    return false;
}

bool rect_hits_hazard(const Tilemap& m, const Tileset& ts, Rect r)
{
    int x0, y0, x1, y1;
    tile_range(ts, r, &x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const TileDef* d = ts.get(m.get(x, y));
            if (!d || !(d->flags & TILE_HAZARD))
                continue;
            Rect box = { x * (float)ts.size + d->box.x, y * (float)ts.size + d->box.y, d->box.w, d->box.h };
            if (overlaps(r, box))
                return true;
        }
    }
    return false;
}

uint8_t rect_tile_flags(const Tilemap& m, const Tileset& ts, Rect r)
{
    int x0, y0, x1, y1;
    tile_range(ts, r, &x0, &y0, &x1, &y1);
    uint8_t out = 0;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const TileDef* d = ts.get(m.get(x, y));
            if (!d || !d->flags)
                continue;
            Rect box = { x * (float)ts.size + d->box.x, y * (float)ts.size + d->box.y, d->box.w, d->box.h };
            if (overlaps(r, box))
                out |= d->flags;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// bodies against the tilemap, one axis at a time

static void move_x(Transform& t, Body& b, const Collider& c, float dx, const Tilemap& m, const Tileset& ts)
{
    if (dx == 0)
        return;
    t.pos.x += dx;
    Rect r = collider_rect(t, c);
    int x0, y0, x1, y1;
    tile_range(ts, r, &x0, &y0, &x1, &y1);
    float s = (float)ts.size;

    // only tiles in front of where we were, so a body that somehow started
    // inside a wall doesn't get flung across it
    if (dx > 0) {
        float old_right = r.x + r.w - dx;
        float best = FLT_MAX;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if ((flags_at(m, ts, x, y) & TILE_SOLID) && x * s >= old_right - 0.01f)
                    best = std::min(best, x * s);
        if (best < FLT_MAX) {
            t.pos.x -= r.x + r.w - best;
            b.vel.x = b.restitution > 0 ? -b.vel.x * b.restitution : 0;
            b.on_wall = 1;
        }
    } else {
        float old_left = r.x - dx;
        float best = -FLT_MAX;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if ((flags_at(m, ts, x, y) & TILE_SOLID) && (x + 1) * s <= old_left + 0.01f)
                    best = std::max(best, (x + 1) * s);
        if (best > -FLT_MAX) {
            t.pos.x += best - r.x;
            b.vel.x = b.restitution > 0 ? -b.vel.x * b.restitution : 0;
            b.on_wall = -1;
        }
    }
}

static void move_y(Transform& t, Body& b, const Collider& c, float dy, const Tilemap& m, const Tileset& ts)
{
    if (dy == 0)
        return;
    t.pos.y += dy;
    Rect r = collider_rect(t, c);
    int x0, y0, x1, y1;
    tile_range(ts, r, &x0, &y0, &x1, &y1);
    float s = (float)ts.size;

    if (dy > 0) {
        float old_bottom = r.y + r.h - dy;
        float best = FLT_MAX;
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                uint8_t f = flags_at(m, ts, x, y);
                bool blocks = (f & TILE_SOLID) || ((f & TILE_ONEWAY) && !b.drop_through);
                if (blocks && y * s >= old_bottom - 0.01f)
                    best = std::min(best, y * s);
            }
        }
        if (best < FLT_MAX) {
            t.pos.y -= r.y + r.h - best;
            // small bounces just jitter, kill them
            b.vel.y = (b.restitution > 0 && b.vel.y > 80) ? -b.vel.y * b.restitution : 0;
            b.on_ground = true;
        }
    } else {
        float old_top = r.y - dy;
        float best = -FLT_MAX;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if ((flags_at(m, ts, x, y) & TILE_SOLID) && (y + 1) * s <= old_top + 0.01f)
                    best = std::max(best, (y + 1) * s);
        if (best > -FLT_MAX) {
            t.pos.y += best - r.y;
            b.vel.y = b.restitution > 0 ? -b.vel.y * b.restitution : 0;
            b.on_ceiling = true;
        }
    }
}

void physics_move_body(Transform& t, Body& b, const Collider& c, vec2 d, const Tilemap& m, const Tileset& ts)
{
    // never move more than half a tile per sub step
    float big = std::max(fabsf(d.x), fabsf(d.y));
    int steps = std::max(1, (int)ceilf(big / (ts.size * 0.5f)));
    vec2 part = d * (1.f / steps);
    for (int i = 0; i < steps; i++) {
        move_x(t, b, c, part.x, m, ts);
        move_y(t, b, c, part.y, m, ts);
    }
}

// push a body back out of any solid tile it overlaps, along the shallow axis
void physics_settle(Transform& t, Body& b, const Collider& c, const Tilemap& m, const Tileset& ts)
{
    float s = (float)ts.size;
    for (int iter = 0; iter < 4; iter++) {
        Rect r = collider_rect(t, c);
        int x0, y0, x1, y1;
        tile_range(ts, r, &x0, &y0, &x1, &y1);
        bool moved = false;
        for (int y = y0; y <= y1 && !moved; y++) {
            for (int x = x0; x <= x1 && !moved; x++) {
                if (!(flags_at(m, ts, x, y) & TILE_SOLID))
                    continue;
                Rect tile = { x * s, y * s, s, s };
                float push_l = r.x + r.w - tile.x;
                float push_r = tile.x + tile.w - r.x;
                float push_u = r.y + r.h - tile.y;
                float push_d = tile.y + tile.h - r.y;
                float mx = push_l < push_r ? -push_l : push_r;
                float my = push_u < push_d ? -push_u : push_d;
                if (fabsf(mx) < fabsf(my)) {
                    t.pos.x += mx;
                    if (mx * b.vel.x < 0)
                        b.vel.x = 0;
                } else {
                    t.pos.y += my;
                    if (my < 0) {
                        b.on_ground = true;
                        if (b.vel.y > 0) b.vel.y = 0;
                    } else if (b.vel.y < 0) {
                        b.vel.y = 0;
                    }
                }
                moved = true;
            }
        }
        if (!moved)
            return;
    }
}

// ---------------------------------------------------------------------------
// queries

RayHit raycast_tiles(const Tilemap& m, const Tileset& ts, vec2 from, vec2 dir, float max_dist)
{
    RayHit hit;
    float s = (float)ts.size;
    int x = (int)floorf(from.x / s), y = (int)floorf(from.y / s);
    if (flags_at(m, ts, x, y) & TILE_SOLID) {
        hit.hit = true;
        hit.point = from;
        return hit;
    }

    int sx = dir.x > 0 ? 1 : -1, sy = dir.y > 0 ? 1 : -1;
    float tdx = dir.x != 0 ? fabsf(s / dir.x) : FLT_MAX;
    float tdy = dir.y != 0 ? fabsf(s / dir.y) : FLT_MAX;
    float tx = dir.x != 0 ? ((sx > 0 ? (x + 1) * s : x * s) - from.x) / dir.x : FLT_MAX;
    float ty = dir.y != 0 ? ((sy > 0 ? (y + 1) * s : y * s) - from.y) / dir.y : FLT_MAX;

    for (;;) {
        float t;
        vec2 n;
        if (tx < ty) {
            t = tx; x += sx; tx += tdx; n = { (float)-sx, 0 };
        } else {
            t = ty; y += sy; ty += tdy; n = { 0, (float)-sy };
        }
        if (t > max_dist)
            return hit;
        // the side walls go up forever, but past the bottom there's nothing to hit
        if (y >= m.h && sy > 0)
            return hit;
        if (flags_at(m, ts, x, y) & TILE_SOLID) {
            hit.hit = true;
            hit.dist = t;
            hit.point = from + dir * t;
            hit.normal = n;
            return hit;
        }
    }
}

static bool ray_box(vec2 o, vec2 d, Rect r, float* t, vec2* n)
{
    float tmin = -FLT_MAX, tmax = FLT_MAX;
    vec2 nmin = {};
    for (int axis = 0; axis < 2; axis++) {
        float oa = axis ? o.y : o.x, da = axis ? d.y : d.x;
        float lo = axis ? r.y : r.x, hi = lo + (axis ? r.h : r.w);
        if (fabsf(da) < 1e-8f) {
            if (oa < lo || oa > hi)
                return false;
            continue;
        }
        float t1 = (lo - oa) / da, t2 = (hi - oa) / da;
        float sgn = -1;
        if (t1 > t2) {
            std::swap(t1, t2);
            sgn = 1;
        }
        if (t1 > tmin) {
            tmin = t1;
            nmin = axis ? vec2{ 0, sgn } : vec2{ sgn, 0 };
        }
        tmax = std::min(tmax, t2);
        if (tmin > tmax)
            return false;
    }
    if (tmax < 0)
        return false;
    *t = tmin < 0 ? 0 : tmin;
    *n = nmin;
    return true;
}

static bool ray_circle(vec2 o, vec2 d, vec2 c, float r, float* t, vec2* n)
{
    vec2 m = o - c;
    float b = dot(m, d);
    float cc = dot(m, m) - r * r;
    if (cc > 0 && b > 0)
        return false;
    float disc = b * b - cc;
    if (disc < 0)
        return false;
    *t = std::max(0.f, -b - sqrtf(disc));
    *n = normalize(o + d * *t - c);
    return true;
}

RayHit raycast(World& w, const Tilemap& m, const Tileset& ts, vec2 from, vec2 dir, float max_dist,
               uint16_t mask, Entity ignore)
{
    RayHit best = raycast_tiles(m, ts, from, dir, max_dist);
    float limit = best.hit ? best.dist : max_dist;

    w.each<Collider, Transform>([&](Entity e, Collider& c, Transform& t) {
        if (e == ignore || c.trigger || !(c.layer & mask))
            return;
        float d;
        vec2 n;
        bool hit = c.shape == SHAPE_CIRCLE
            ? ray_circle(from, dir, t.pos + c.offset, c.radius, &d, &n)
            : ray_box(from, dir, collider_rect(t, c), &d, &n);
        if (hit && d <= limit) {
            limit = d;
            best.hit = true;
            best.dist = d;
            best.point = from + dir * d;
            best.normal = n;
            best.entity = e;
        }
    });
    return best;
}

void query_rect(World& w, Rect r, uint16_t mask, std::vector<Entity>& out)
{
    w.each<Collider, Transform>([&](Entity e, Collider& c, Transform& t) {
        if ((c.layer & mask) && overlaps(r, collider_rect(t, c)))
            out.push_back(e);
    });
}
