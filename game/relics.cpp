#include "relics.h"
#include <cstring>

static const RelicDef DEFS[] = {
    { "coeur_cendre", "Cœur de Cendre", "Ralentit de 60% la perte de chaleur dans le froid", 1 },
    { "silex_vivace", "Silex Vivace",   "Élan recharge plus vite et autorise deux ruées", 1 },
    { "souffle_chaud", "Souffle Chaud", "Flèches à halo thermique qui fond la glace", 1 },
    { "plume_suie",   "Plume de Suie",  "Plané plus maniable et poussée de vapeur accrue", 1 },
    { "lame_suie",    "Lame de Suie",   "Rebond pogo à la lance bondit plus haut", 1 }
};
static const int DEF_COUNT = sizeof(DEFS) / sizeof(DEFS[0]);

static RelicState states[DEF_COUNT];

void relics_init()
{
    relics_reset();
}

void relics_reset()
{
    for (int i = 0; i < DEF_COUNT; i++) {
        states[i].unlocked = false;
        states[i].equipped = false;
    }
}

int relics_count()
{
    return DEF_COUNT;
}

const RelicDef& relic_def(int index)
{
    static const RelicDef fallback = { "", "", "", 1 };
    if (index < 0 || index >= DEF_COUNT)
        return fallback;
    return DEFS[index];
}

const RelicState& relic_state(int index)
{
    static const RelicState fallback = { false, false };
    if (index < 0 || index >= DEF_COUNT)
        return fallback;
    return states[index];
}

static int find_relic_index(const char* id)
{
    if (!id || !*id)
        return -1;
    for (int i = 0; i < DEF_COUNT; i++) {
        if (DEFS[i].id == id)
            return i;
    }
    return -1;
}

bool relic_is_unlocked(const char* id)
{
    int idx = find_relic_index(id);
    return idx >= 0 && states[idx].unlocked;
}

bool relic_is_equipped(const char* id)
{
    int idx = find_relic_index(id);
    return idx >= 0 && states[idx].equipped;
}

bool relic_unlock(const char* id)
{
    int idx = find_relic_index(id);
    if (idx < 0)
        return false;
    bool was_unlocked = states[idx].unlocked;
    states[idx].unlocked = true;
    if (!was_unlocked && relic_slots_used() + DEFS[idx].cost <= RELIC_MAX_SLOTS)
        states[idx].equipped = true;
    return true;
}

int relic_slots_used()
{
    int used = 0;
    for (int i = 0; i < DEF_COUNT; i++) {
        if (states[i].equipped)
            used += DEFS[i].cost;
    }
    return used;
}

int relic_slots_max()
{
    return RELIC_MAX_SLOTS;
}

bool relic_equip(const char* id, bool equip)
{
    int idx = find_relic_index(id);
    if (idx < 0 || !states[idx].unlocked)
        return false;

    if (!equip) {
        states[idx].equipped = false;
        return true;
    }

    if (relic_slots_used() + DEFS[idx].cost > RELIC_MAX_SLOTS)
        return false;

    states[idx].equipped = true;
    return true;
}

std::vector<std::string> relics_unlocked_list()
{
    std::vector<std::string> list;
    for (int i = 0; i < DEF_COUNT; i++) {
        if (states[i].unlocked)
            list.push_back(DEFS[i].id);
    }
    return list;
}

std::vector<std::string> relics_equipped_list()
{
    std::vector<std::string> list;
    for (int i = 0; i < DEF_COUNT; i++) {
        if (states[i].equipped)
            list.push_back(DEFS[i].id);
    }
    return list;
}

void relics_load_from_lists(const std::vector<std::string>& unlocked, const std::vector<std::string>& equipped)
{
    relics_reset();
    for (const std::string& id : unlocked) {
        int idx = find_relic_index(id.c_str());
        if (idx >= 0)
            states[idx].unlocked = true;
    }
    for (const std::string& id : equipped) {
        relic_equip(id.c_str(), true);
    }
}
