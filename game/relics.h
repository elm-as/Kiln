#pragma once
#include <string>
#include <vector>

struct RelicDef {
    std::string id;
    std::string name;
    std::string desc;
    int cost = 1;
};

struct RelicState {
    bool unlocked = false;
    bool equipped = false;
};

const int RELIC_MAX_SLOTS = 3;

void relics_init();
void relics_reset();

int relics_count();
const RelicDef& relic_def(int index);
const RelicState& relic_state(int index);

/**
 * Checks whether the relic identified by `id` has been unlocked in the current run/save.
 * @param id Unique relic identifier (e.g. "coeur_braise", "plume_suie"). Null returns false.
 * @return true if the relic exists and is unlocked, false otherwise.
 */
bool relic_is_unlocked(const char* id);

/**
 * Checks whether the relic identified by `id` is currently equipped and active.
 * @param id Unique relic identifier. Null returns false.
 * @return true if the relic exists, is unlocked, and is equipped.
 */
bool relic_is_equipped(const char* id);

/**
 * Unlocks a relic by `id` and auto-equips it if enough free slots remain (up to RELIC_MAX_SLOTS).
 * @param id Unique relic identifier.
 * @return true if newly unlocked (false if unknown or already unlocked).
 */
bool relic_unlock(const char* id);

/**
 * Equips or unequips an unlocked relic, enforcing the RELIC_MAX_SLOTS capacity limit.
 * @param id Unique relic identifier.
 * @param equip true to equip, false to unequip.
 * @return true if the state was updated, false if locked, unknown, or slot capacity exceeded.
 */
bool relic_equip(const char* id, bool equip);

int relic_slots_used();
int relic_slots_max();

std::vector<std::string> relics_unlocked_list();
std::vector<std::string> relics_equipped_list();
void relics_load_from_lists(const std::vector<std::string>& unlocked, const std::vector<std::string>& equipped);
