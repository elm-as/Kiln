#pragma once
#include <initializer_list>
#include <string>
#include "json.h"
#include "platform.h"

// Named actions ("jump", "attack"...) with up to three keys each, so the
// player can change them. The game defines them once at startup, in the
// order it wants to index them by:
//
//   enum { JUMP, ATTACK };
//   action_define("jump", "saut", { KEY_SPACE, KEY_K });
//   action_define("attack", "arme", { KEY_X });
//   if (action_pressed(in, JUMP)) ...
//
// Texts can say {jump} and action_text() puts the key's name in.

const int ACTION_KEYS = 3;

int action_define(const char* id, const char* label, std::initializer_list<int> keys);
int action_count();
int action_find(const char* id);          // -1 if there's no such action
const char* action_id(int act);
const char* action_label(int act);
const int* action_keys(int act);          // ACTION_KEYS of them, 0 for none

bool action_down(const Input& in, int act);
bool action_pressed(const Input& in, int act);

// the key goes first for this action and is taken off every other one
void action_bind(int act, int key);
void actions_reset();                     // back to what action_define said

Json actions_save();                      // { "jump": [keys...], ... }
void actions_load(const Json& j);         // unknown ids are ignored, missing ones keep their keys

const char* key_name(int key);            // short, lowercase, french: "espace", "gauche", "x"
std::string action_text(const std::string& s);
