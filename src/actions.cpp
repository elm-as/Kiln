#include "actions.h"
#include <cstdio>
#include <vector>

struct Action {
    std::string id, label;
    int defaults[ACTION_KEYS];
    int keys[ACTION_KEYS];
};

static std::vector<Action> actions;

int action_define(const char* id, const char* label, std::initializer_list<int> keys)
{
    Action a;
    a.id = id;
    a.label = label;
    int i = 0;
    for (int k : keys)
        if (i < ACTION_KEYS)
            a.defaults[i++] = k;
    for (; i < ACTION_KEYS; i++)
        a.defaults[i] = 0;
    for (i = 0; i < ACTION_KEYS; i++)
        a.keys[i] = a.defaults[i];
    actions.push_back(a);
    return (int)actions.size() - 1;
}

int action_count()
{
    return (int)actions.size();
}

int action_find(const char* id)
{
    for (size_t i = 0; i < actions.size(); i++)
        if (actions[i].id == id)
            return (int)i;
    return -1;
}

const char* action_id(int act)
{
    return actions[act].id.c_str();
}

const char* action_label(int act)
{
    return actions[act].label.c_str();
}

const int* action_keys(int act)
{
    return actions[act].keys;
}

bool action_down(const Input& in, int act)
{
    for (int k : actions[act].keys)
        if (k && in.down[k])
            return true;
    return false;
}

bool action_pressed(const Input& in, int act)
{
    for (int k : actions[act].keys)
        if (k && in.pressed[k])
            return true;
    return false;
}

// drop the empty slots so the first key is always the one shown
static void pack(int* keys)
{
    int n = 0;
    for (int i = 0; i < ACTION_KEYS; i++)
        if (keys[i])
            keys[n++] = keys[i];
    for (; n < ACTION_KEYS; n++)
        keys[n] = 0;
}

void action_bind(int act, int key)
{
    if (act < 0 || act >= action_count() || key <= 0 || key >= KEY_COUNT)
        return;
    for (size_t a = 0; a < actions.size(); a++) {
        if ((int)a == act)
            continue;
        for (int& k : actions[a].keys)
            if (k == key)
                k = 0;
        pack(actions[a].keys);
    }
    int* keys = actions[act].keys;
    int kept[ACTION_KEYS] = { key };
    int n = 1;
    for (int i = 0; i < ACTION_KEYS && n < ACTION_KEYS; i++)
        if (keys[i] && keys[i] != key)
            kept[n++] = keys[i];
    for (int i = 0; i < ACTION_KEYS; i++)
        keys[i] = kept[i];
}

void actions_reset()
{
    for (Action& a : actions)
        for (int i = 0; i < ACTION_KEYS; i++)
            a.keys[i] = a.defaults[i];
}

Json actions_save()
{
    Json j = Json::object();
    for (const Action& a : actions) {
        Json list = Json::array();
        for (int k : a.keys)
            list.arr.push_back(Json::number(k));
        j.set(a.id.c_str(), list);
    }
    return j;
}

void actions_load(const Json& j)
{
    for (Action& a : actions) {
        const Json& list = j[a.id.c_str()];
        if (!list.is_arr())
            continue;
        for (int i = 0; i < ACTION_KEYS; i++) {
            int k = i < (int)list.size() ? list.arr[i].i(0) : 0;
            a.keys[i] = k > 0 && k < KEY_COUNT ? k : 0;
        }
        pack(a.keys);
    }
}

const char* key_name(int k)
{
    static char buf[8];
    if (k >= KEY_A && k <= KEY_Z) {
        snprintf(buf, sizeof(buf), "%c", 'a' + (k - KEY_A));
        return buf;
    }
    if (k >= KEY_0 && k <= KEY_9) {
        snprintf(buf, sizeof(buf), "%c", '0' + (k - KEY_0));
        return buf;
    }
    if (k >= KEY_F1 && k <= KEY_F12) {
        snprintf(buf, sizeof(buf), "f%d", 1 + (k - KEY_F1));
        return buf;
    }
    switch (k) {
    case KEY_LEFT: return "gauche";
    case KEY_RIGHT: return "droite";
    case KEY_UP: return "haut";
    case KEY_DOWN: return "bas";
    case KEY_SPACE: return "espace";
    case KEY_ENTER: return "entrée";
    case KEY_ESCAPE: return "échap";
    case KEY_TAB: return "tab";
    case KEY_BACKSPACE: return "retour";
    case KEY_DELETE: return "suppr";
    case KEY_HOME: return "début";
    case KEY_END: return "fin";
    case KEY_PAGEUP: return "pg haut";
    case KEY_PAGEDOWN: return "pg bas";
    case KEY_SHIFT: return "shift";
    case KEY_CTRL: return "ctrl";
    case KEY_ALT: return "alt";
    }
    return "?";
}

std::string action_text(const std::string& s)
{
    if (s.find('{') == std::string::npos)
        return s;
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        size_t open = s.find('{', i);
        size_t close = open == std::string::npos ? open : s.find('}', open);
        if (close == std::string::npos) {
            out += s.substr(i);
            break;
        }
        out += s.substr(i, open - i);
        int act = action_find(s.substr(open + 1, close - open - 1).c_str());
        if (act < 0)
            out += s.substr(open, close - open + 1);
        else
            out += key_name(actions[act].keys[0]);
        i = close + 1;
    }
    return out;
}
