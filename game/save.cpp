// Progress on disk: which level to come back to, and the run's totals so far.
// Written by the torch at the start of each world.
//
// Lives in %APPDATA%\Lueur\save.json. LUEUR_SAVE=path puts it somewhere else,
// the replays use that so they don't touch a real save.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game.h"
#include "relics.h"

static std::string save_path()
{
    if (const char* p = getenv("LUEUR_SAVE"))
        return p;
    if (const char* app = getenv("APPDATA")) {
        std::string dir = path_join(app, "Lueur");
        make_dir(dir.c_str());
        return path_join(dir, "save.json");
    }
    return "save.json";
}

std::string options_path()
{
    std::string p = save_path();
    size_t slash = p.find_last_of("/\\");
    return (slash == std::string::npos ? std::string() : p.substr(0, slash + 1)) + "options.json";
}

static std::string progress_path()
{
    std::string p = options_path();
    return p.substr(0, p.size() - strlen("options.json")) + "progress.json";
}

void load_progress()
{
    G.reached = -1;
    G.best.clear();
    std::string text, err;
    Json j;
    if (!read_text(progress_path().c_str(), text) || !json_parse(text, &j, &err))
        return;
    G.reached = j["reached"].i(-1);
    if (G.reached < -1)
        G.reached = -1;
    if (!G.levels.empty() && G.reached >= (int)G.levels.size())
        G.reached = (int)G.levels.size() - 1;
    for (const auto& kv : j["best"].obj)
        G.best[kv.first] = kv.second.i(0);
}

void save_progress()
{
    Json j = Json::object();
    j.set("reached", Json::number(G.reached));
    Json best = Json::object();
    for (const auto& kv : G.best)
        best.set(kv.first.c_str(), Json::number(kv.second));
    j.set("best", best);
    std::string text = json_write(j);
    write_file(progress_path().c_str(), text.data(), text.size());
}

bool save_game()
{
    Json j = Json::object();
    j.set("level", Json::string(G.levels[G.level]));
    j.set("time", Json::number(G.run_time));
    j.set("deaths", Json::number(G.run_deaths));
    j.set("embers", Json::number(G.run_embers));
    j.set("embers_total", Json::number(G.run_embers_total));
    j.set("spear", Json::boolean(G.has[WEAPON_SPEAR]));
    j.set("bow", Json::boolean(G.has[WEAPON_BOW]));
    j.set("weapon", Json::number(G.weapon));
    j.set("arrows", Json::number(G.quiver));
    Json powers = Json::array();
    for (const std::string& s : G.powers)
        powers.arr.push_back(Json::string(s));
    j.set("powers", powers);

    Json rel_un = Json::array();
    for (const std::string& s : relics_unlocked_list())
        rel_un.arr.push_back(Json::string(s));
    j.set("relics_unlocked", rel_un);

    Json rel_eq = Json::array();
    for (const std::string& s : relics_equipped_list())
        rel_eq.arr.push_back(Json::string(s));
    j.set("relics_equipped", rel_eq);

    std::string text = json_write(j);
    bool ok = write_file(save_path().c_str(), text.data(), text.size());
    if (!ok)
        fprintf(stderr, "couldn't write %s\n", save_path().c_str());
    return ok;
}

// the level index to continue from, -1 if there's no usable save
int saved_level()
{
    std::string text, err;
    Json j;
    if (!read_text(save_path().c_str(), text) || !json_parse(text, &j, &err))
        return -1;
    for (size_t i = 0; i < G.levels.size(); i++)
        if (G.levels[i] == j["level"].str())
            return (int)i;
    return -1;
}

bool save_summary(char* buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return false;
    std::string text, err;
    Json j;
    if (!read_text(save_path().c_str(), text) || !json_parse(text, &j, &err))
        return false;
    std::string lvl_id = j["level"].str();
    int embers = j["embers"].i(0);
    std::string lvl_title = lvl_id;
    for (size_t i = 0; i < G.levels.size(); i++) {
        if (G.levels[i] == lvl_id && i < G.level_titles.size() && !G.level_titles[i].empty()) {
            lvl_title = G.level_titles[i];
            break;
        }
    }
    snprintf(buf, buf_size, "%s - %d braises", lvl_title.c_str(), embers);
    return true;
}

bool load_saved_run()
{
    std::string text, err;
    Json j;
    if (!read_text(save_path().c_str(), text) || !json_parse(text, &j, &err))
        return false;
    G.run_time = j["time"].f(0);
    G.run_deaths = j["deaths"].i(0);
    G.run_embers = j["embers"].i(0);
    G.run_embers_total = j["embers_total"].i(0);
    G.has[WEAPON_SPEAR] = j["spear"].boolean_or(false);
    G.has[WEAPON_BOW] = j["bow"].boolean_or(false);
    G.weapon = j["weapon"].i(WEAPON_SPEAR) == WEAPON_BOW ? WEAPON_BOW : WEAPON_SPEAR;
    G.quiver = j["arrows"].i(0);
    G.powers.clear();
    for (const Json& s : j["powers"].arr)
        G.powers.push_back(s.str());

    std::vector<std::string> un, eq;
    for (const Json& s : j["relics_unlocked"].arr)
        un.push_back(s.str());
    for (const Json& s : j["relics_equipped"].arr)
        eq.push_back(s.str());
    relics_load_from_lists(un, eq);
    return true;
}
