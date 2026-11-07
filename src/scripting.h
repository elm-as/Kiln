#pragma once
#include <initializer_list>
#include <string>
#include "components.h"

// Glue between KScript and the ECS. A Script component points at a module
// (scripts/<name>.ks). The module can define:
//
//   fn init(self)            once, the first tick the entity exists
//   fn update(self, dt)      every tick
//   fn touch(self, other)    when its collider touches another entity
//
// self is a table holding the component's props plus self.id, the entity.
// Modules are shared: top level variables are shared by every entity running
// that script.

void scripting_init();
void scripting_shutdown();
VM& script_vm();

int script_module(const char* name);        // loads on first use, -1 if missing
const char* script_module_name(int id);
void scripting_reload();                    // recompile all modules from the current assets

void scripts_update(World& w, float dt);
bool script_send(World& w, Entity e, const char* fn, std::initializer_list<Value> args, Value* result = 0);

Value json_to_value(const Json& j);
Json value_to_json(const Value& v);
inline Value entity_value(Entity e) { return Value::number((double)e); }
inline Entity value_entity(const Value& v) { return v.type == V_NUM ? (Entity)v.n : NO_ENTITY; }

// Switches shared by every script, by name: a lever sets one, a door reads
// it. The scripts see flag(name) and set_flag(name, on). They belong to the
// level, clear them when a new one starts.
bool script_flag(const std::string& name);
void script_set_flag(const std::string& name, bool on);
void script_flags_clear();

// last error per session, shown on screen in dev builds until cleared
const std::string& script_error();
void script_clear_error();
