#pragma once
#include <map>
#include <string>
#include <vector>
#include "kiln.h"

const int VIEW_W = 320;
const int VIEW_H = 180;

// collider layers
enum {
    L_WORLD = 1,    // crates, platforms
    L_PLAYER = 2,
    L_ENEMY = 4,
    L_PICKUP = 8,
};

struct Player {
    int facing = 1;
    float coyote = 0;
    float buffer = 0;       // jump pressed a moment before we could jump
    int wall_dir = 0;
    float wall_grace = 0;   // lets a wall jump through a frame or two after leaving the wall
    float lock = 0;         // less air control right after a wall jump
    bool jumping = false;
    bool was_ground = false;
    float fall_speed = 0;
    float drop = 0;
    float step = 0;
    float squash = 0;       // > 0 squashed flat, < 0 stretched tall

    // the spear
    float attack = 0;       // time left on the current thrust
    float attack_cool = 0;
    bool attack_down = false;
    bool attack_up = false;
    std::vector<Entity> struck;     // so one thrust hits each thing once
    float bow_cool = 0;
    vec2 wind;              // pushed by scripts during a frame: x a speed, y an acceleration
    float dash = 0;         // time left on a burst of fire, the forge's gift
    float dash_cool = 0;
    bool dash_ready = true; // one per jump, back on landing
    int air_dashes = 0;
    std::vector<Entity> dashed;
    bool gliding = false;
    bool aiming = false;    // attack held with the bow out, lets go to shoot
    float aim = 0;          // radians from straight ahead, negative is up
};

const float SPEAR_TIME = 0.2f;
const float DASH_TIME = 0.18f;
const float DASH_SPEED = 300;
const float ECLAT_RADIUS = 130;
const float ECLAT_COOL = 3;

// Arrows aren't entities, nothing else needs to see them
struct Arrow {
    vec2 pos, prev, vel;
    float angle = 0;
    float life = 0;
    bool stuck = false;     // in a wall, just fading out
};

const int QUIVER_MAX = 12;     // before the cave's bonus
const float ARROW_SPEED = 300;
const float ARROW_GRAVITY = 420;

// Braise hands these out as the story goes, spear first
enum Weapon { WEAPON_SPEAR, WEAPON_BOW, WEAPON_COUNT };

struct Tuning {
    float run = 105, accel = 900, air_accel = 620, friction = 1200, air_friction = 300;
    float jump = 290, jump_cut = 0.45f, fall_gravity = 1.5f, apex_gravity = 0.55f;
    float wall_slide = 55, wall_jump_x = 140, wall_jump_y = 270;
    float coyote = 0.1f, buffer = 0.12f;
};

// A world is a run of levels sharing a look and a mood, and what it does to
// Lueur: the volcano feeds her, the sea saps her.
struct WorldInfo {
    std::string id, name;
    std::string intro;          // cutscene played on the way in
    std::vector<std::string> levels;
    float run = 1, jump = 1, light = 1;
    Color aura = { 0, 0, 0, 0 };    // her light color there, if not the usual
    float cold = 0;             // heat lost per second away from a fire, 0 = not cold at all
};

enum Mode { MODE_TITLE, MODE_STORY, MODE_PLAY, MODE_PAUSE, MODE_CLEAR, MODE_END, MODE_EDIT, MODE_OPTIONS, MODE_SELECT };

// what the keys are for, defined in this order by game_actions()
enum Action {
    ACT_LEFT, ACT_RIGHT, ACT_UP, ACT_DOWN, ACT_JUMP, ACT_ATTACK, ACT_SWITCH, ACT_DASH, ACT_ECLAT, ACT_PAUSE,
    ACT_COUNT
};

struct Options {
    int music = 8, sfx = 8;     // 0..10
    bool fullscreen = false;
};
extern Options O;

struct Game {
    Mode mode = MODE_TITLE;
    Scene scene;
    Tileset tileset;
    Json config;
    Tuning tune;
    PhysicsConfig physics;
    std::vector<WorldInfo> worlds;
    std::vector<std::string> levels;    // every world's levels, back to back
    std::vector<int> level_world;       // which world each of those is in
    int level = 0;
    int next_level = -1;                // waiting on a cutscene to load this
    bool world_done = false;            // the level ended through a portal
    std::string next_spawn_tag;         // metroidvania target spawn tag

    Entity player = NO_ENTITY;
    vec2 spawn;
    Camera camera;
    std::vector<Contact> contacts;
    std::vector<Arrow> arrows;
    int quiver = 0;
    bool has[WEAPON_COUNT] = {};
    std::vector<std::string> powers;    // what the worlds gave her, "planer" and on

    // progress kept across runs, in progress.json
    int reached = -1;                   // furthest level ever started
    std::map<std::string, int> best;    // most embers taken in each level
    std::vector<int> level_embers;      // how many each level has
    std::vector<std::string> level_titles;
    bool replay = false;                // playing a level again from the world list
    std::string bonus_msg;              // shown on the clear screen when one unlocks
    float heat = 1;         // only goes down in a cold world, 0 and she's out
    float light_base = 0;   // her light radius before the cold eats into it
    float shiver = 0;
    float arrow_regen = 0;
    std::string boss_name;  // a boss bar at the top while this is set
    float boss_hp = 0;      // 0..1
    float eclat_glow = 0;   // what's left of the last burst of light, 1 right after
    float eclat_cool = 0;
    vec2 eclat_pos;
    int weapon = WEAPON_SPEAR;

    float level_time = 0;
    int deaths = 0;
    int embers = 0, embers_total = 0;
    float run_time = 0;
    int run_deaths = 0, run_embers = 0, run_embers_total = 0;

    float respawn_timer = 0;
    float finish_timer = 0;
    float fade = 1;         // black overlay, 1 = fully black
    std::string message;
    float message_time = 0;
    std::string banner;     // level name slide-in
    float banner_x = -200;

    bool clear_after_talk = false;  // the level is done, waiting for the outro to finish
    bool playtest = false;  // running a level straight from the editor
    std::string playtest_snapshot;

    int menu = 0;
    float title_time = 0;
    Font font, big;
    Canvas light;
    int assets_gen = -1;
    std::vector<ParallaxLayer> parallax;
};

extern Game G;

// world.cpp & world_step.cpp
void game_load_data();
void load_parallax(const Json& props);
bool level_load(int index);
void level_begin();
void world_update(const Input& in, float dt);
void kill_player(const char* how = "");
void spawn_player();
vec2 cam_focus();
const WorldInfo& current_world();
Tuning world_tuning();
void finish_level();
void bounce_player(float vy, bool can_cut = true);
void set_checkpoint(vec2 p);
void enter_clear();
void say(const std::string& text, float seconds);
void add_shake(float amount);
bool transition_to(const std::string& level_name, const std::string& spawn_tag = "");

// player.cpp & player_combat.cpp
void register_game_components();
void player_update(const Input& in, float dt);
void player_combat_update(const Input& in, float dt, bool ground);
bool player_alive();
Rect spear_rect(const Transform& t, const Player& p);
void arrows_update(float dt);
void add_arrows(int n);
bool give_weapon(const std::string& name);
bool has_power(const char* name);
void weapons_for_level(int level);

// draw.cpp & menus.cpp
void draw_background(Rect view);
void draw_level(Scene& s, Rect view, float alpha);
void draw_game(float alpha);
void pause_frame(const Input& in);
void draw_pause();
void draw_clear();
void draw_end();
void open_editor();
void start_playtest(bool from_view);
void stop_playtest();
void text_shadow(const Font& f, const char* s, float x, float y, Color c);
void text_center(const Font& f, const char* s, float y, Color c);
void text_center_at(const char* s, float y, Color c);
void editor_draw_world(Scene& s, Rect view);

// hud.cpp
void draw_hud();

// title.cpp
void title_build_menu();
int title_update(const Input& in, float dt);
void draw_title();

// dialogue.cpp & cutscene.cpp
void story_load();
const Json& story_data();
std::vector<std::string> wrap_text(const Font& f, const std::string& text, float width);
bool dialogue_start(const std::string& id);
bool dialogue_active();
void dialogue_frame(const Input& in, float dt);
void dialogue_draw();
void cutscene_start(const std::string& id, void (*done)());
void cutscene_frame(const Input& in, float dt);
void cutscene_draw();

// options.cpp
void game_actions();
void options_defaults();
void options_load();
void options_save();
void options_apply();
void options_open(Mode from);
Mode options_from();
void options_frame(const Input& in);
void options_draw();

// select.cpp
void select_open();
void select_frame(const Input& in);
void select_draw();

// what a world gives for all its embers
bool has_bonus(const char* world);
const char* bonus_text(const char* world);
void world_embers(int world, int* got, int* total);
int best_embers(int level);
int quiver_max();

// save.cpp
std::string options_path();
void load_progress();
void save_progress();
bool save_game();
int saved_level();
bool save_summary(char* buf, size_t buf_size);
bool load_saved_run();

// natives.cpp
void register_natives();
void natives_reset();

// input helpers, keyboard or pad
bool input_left(const Input& in);
bool input_right(const Input& in);
bool input_down(const Input& in);
bool input_up(const Input& in);
bool input_jump_pressed(const Input& in);
bool input_jump_held(const Input& in);
bool input_attack(const Input& in);
bool input_attack_held(const Input& in);
bool input_switch(const Input& in);
bool input_dash(const Input& in);
bool input_eclat(const Input& in);
bool lit_at(vec2 p);
bool input_skip(const Input& in);
bool input_confirm(const Input& in);
bool input_back(const Input& in);
