#pragma once
#include <cstdint>
#include <memory>
#include <vector>

// Sparse set ECS. An entity is 20 bits of index and 12 bits of generation, so
// a stale handle to a recycled slot is caught instead of silently pointing at
// whatever lives there now. 0 is never a valid entity.
//
// Careful with references: adding a T can move every other T in memory.
// Don't hold a T& across an add<T>().

typedef uint32_t Entity;

const Entity NO_ENTITY = 0;

inline uint32_t entity_index(Entity e) { return e & 0xfffff; }
inline uint32_t entity_gen(Entity e) { return e >> 20; }

struct PoolBase {
    virtual ~PoolBase() {}
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
    virtual void clear() = 0;
    virtual size_t count() const = 0;
};

template <class T>
struct Pool : PoolBase {
    std::vector<uint32_t> sparse;   // entity index -> slot in dense, or ~0
    std::vector<Entity> dense;
    std::vector<T> data;

    bool has(Entity e) const override
    {
        uint32_t i = entity_index(e);
        return i < sparse.size() && sparse[i] != ~0u && dense[sparse[i]] == e;
    }

    T* get(Entity e)
    {
        uint32_t i = entity_index(e);
        if (i < sparse.size() && sparse[i] != ~0u && dense[sparse[i]] == e)
            return &data[sparse[i]];
        return 0;
    }

    T& add(Entity e, T v)
    {
        if (T* existing = get(e)) {
            *existing = std::move(v);
            return *existing;
        }
        uint32_t i = entity_index(e);
        if (i >= sparse.size())
            sparse.resize(i + 1, ~0u);
        sparse[i] = (uint32_t)dense.size();
        dense.push_back(e);
        data.push_back(std::move(v));
        return data.back();
    }

    // swap with the last one and pop, so dense stays packed
    void remove(Entity e) override
    {
        if (!has(e))
            return;
        uint32_t i = entity_index(e);
        uint32_t slot = sparse[i];
        uint32_t last = (uint32_t)dense.size() - 1;
        if (slot != last) {
            dense[slot] = dense[last];
            data[slot] = std::move(data[last]);
            sparse[entity_index(dense[slot])] = slot;
        }
        dense.pop_back();
        data.pop_back();
        sparse[i] = ~0u;
    }

    void clear() override
    {
        sparse.clear();
        dense.clear();
        data.clear();
    }

    size_t count() const override { return dense.size(); }
};

uint32_t next_component_id();

template <class T>
uint32_t component_id()
{
    static uint32_t id = next_component_id();
    return id;
}

struct World {
    std::vector<uint32_t> gens;         // current generation per index
    std::vector<uint32_t> free_slots;
    std::vector<std::unique_ptr<PoolBase>> pools;
    std::vector<Entity> doomed;
    uint32_t live = 0;

    Entity create()
    {
        uint32_t i;
        if (!free_slots.empty()) {
            i = free_slots.back();
            free_slots.pop_back();
        } else {
            i = (uint32_t)gens.size();
            // index 0 is burned so that entity 0 can mean "nothing"
            if (i == 0) {
                gens.push_back(0);
                i = 1;
            }
            gens.push_back(1);
        }
        live++;
        return gens[i] << 20 | i;
    }

    bool alive(Entity e) const
    {
        uint32_t i = entity_index(e);
        return e != NO_ENTITY && i < gens.size() && gens[i] == entity_gen(e);
    }

    // deferred, so systems can kill things while iterating. flush() does the work
    void destroy(Entity e)
    {
        if (alive(e))
            doomed.push_back(e);
    }

    void flush()
    {
        // destroying can queue more (children etc), so go by index
        for (size_t k = 0; k < doomed.size(); k++) {
            Entity e = doomed[k];
            if (!alive(e))
                continue;
            for (auto& p : pools)
                if (p)
                    p->remove(e);
            uint32_t i = entity_index(e);
            gens[i] = (gens[i] + 1) & 0xfff;
            if (gens[i] == 0)
                gens[i] = 1;
            free_slots.push_back(i);
            live--;
        }
        doomed.clear();
    }

    void clear()
    {
        for (auto& p : pools)
            if (p)
                p->clear();
        gens.clear();
        free_slots.clear();
        doomed.clear();
        live = 0;
    }

    template <class T>
    Pool<T>& pool()
    {
        uint32_t id = component_id<T>();
        if (id >= pools.size())
            pools.resize(id + 1);
        if (!pools[id])
            pools[id].reset(new Pool<T>());
        return *static_cast<Pool<T>*>(pools[id].get());
    }

    template <class T> T& add(Entity e, T v = T()) { return pool<T>().add(e, std::move(v)); }
    template <class T> T* get(Entity e) { return pool<T>().get(e); }
    template <class T> bool has(Entity e) { return pool<T>().has(e); }
    template <class T> void remove(Entity e) { pool<T>().remove(e); }

    // f(Entity, A&, B&...) for every entity that has all of them. Walks A's pool,
    // so put the rarest component first. Snapshots dense entities so adding or
    // removing components/entities inside f never invalidates references or visits twice.
    template <class A, class... Rest, class F>
    void each(F&& f)
    {
        Pool<A>& pa = pool<A>();
        std::vector<Entity> snap = pa.dense;
        for (size_t k = snap.size(); k-- > 0;) {
            Entity e = snap[k];
            if (!alive(e) || !pa.has(e) || !all_present<Rest...>(e))
                continue;
            f(e, *pa.get(e), *pool<Rest>().get(e)...);
        }
    }

    template <class T>
    size_t count() { return pool<T>().count(); }

private:
    template <class... Ts>
    bool all_present(Entity e)
    {
        (void)e;    // unused when there's only one component
        bool ok = true;
        ((ok = ok && pool<Ts>().has(e)), ...);
        return ok;
    }
};
