#include "scripting.h"
#include <cstdio>
#include <map>
#include <memory>
#include <vector>
#include "assets.h"

struct Loaded {
    std::string name;
    Module mod;
    bool broken = false;    // hit a runtime error, skipped until the next reload
};

static std::unique_ptr<VM> vm;
static std::vector<Loaded> modules;
static std::map<std::string, int> module_ids;
static std::string error_text;
static int calling = -1;    // module currently running, to blame errors on

static void on_error(const std::string& msg)
{
    fprintf(stderr, "script: %s\n", msg.c_str());
    error_text = msg;
    if (calling >= 0)
        modules[calling].broken = true;
}

static void on_print(const std::string& msg)
{
    printf("%s\n", msg.c_str());
}

static std::map<std::string, bool> flags;

bool script_flag(const std::string& name)
{
    auto it = flags.find(name);
    return it != flags.end() && it->second;
}

void script_set_flag(const std::string& name, bool on)
{
    flags[name] = on;
}

void script_flags_clear()
{
    flags.clear();
}

static Value n_flag(VM& vm, const Value* a, int n)
{
    return Value::boolean(script_flag(vm.string_arg(a, n, 0)));
}

static Value n_set_flag(VM& vm, const Value* a, int n)
{
    script_set_flag(vm.string_arg(a, n, 0), n < 2 || a[1].truthy());
    return Value();
}

void scripting_init()
{
    vm.reset(new VM());
    vm->on_error = on_error;
    vm->on_print = on_print;
    vm->def("flag", n_flag);
    vm->def("set_flag", n_set_flag);
}

void scripting_shutdown()
{
    modules.clear();
    module_ids.clear();
    vm.reset();
}

VM& script_vm()
{
    return *vm;
}

static void compile(int id)
{
    Loaded& l = modules[id];
    std::string path = "scripts/" + l.name + ".ks";
    const std::vector<uint8_t>* src = asset_data(path);
    if (!src) {
        fprintf(stderr, "script: no %s\n", path.c_str());
        return;
    }
    int saved_calling = calling;
    calling = -1;
    l.broken = false;
    // on a compile error the old version keeps running
    vm->load(l.mod, std::string(src->begin(), src->end()), l.name + ".ks");
    calling = saved_calling;
}

int script_module(const char* name)
{
    if (!name || !*name)
        return -1;
    auto it = module_ids.find(name);
    if (it != module_ids.end())
        return it->second;
    if (!asset_data(std::string("scripts/") + name + ".ks")) {
        fprintf(stderr, "script: no module called '%s'\n", name);
        return -1;
    }
    int id = (int)modules.size();
    modules.emplace_back();
    modules.back().name = name;
    module_ids[name] = id;
    compile(id);
    return id;
}

const char* script_module_name(int id)
{
    if (id < 0 || id >= (int)modules.size())
        return "";
    return modules[id].name.c_str();
}

void scripting_reload()
{
    error_text.clear();
    for (int i = 0; i < (int)modules.size(); i++)
        compile(i);
}

static bool call(int module, const char* fn, const Value* args, int argc, Value* result)
{
    if (module < 0 || module >= (int)modules.size())
        return false;
    if (!modules[module].mod.ok || modules[module].broken)
        return false;
    Value f = modules[module].mod.get(fn);
    if (f.is_nil())
        return false;
    int saved = calling;
    calling = module;
    bool ok = vm->call(f, args, argc, result);
    calling = saved;
    return ok;
}

static Value make_self(Entity e, const Script& s)
{
    Value self = json_to_value(s.props);
    if (!self.tab())
        self = Value::table();
    self.tab()->set("id", entity_value(e));
    return self;
}

void scripts_update(World& w, float dt)
{
    // scripts spawn things, which can grow the Script pool under our feet,
    // so walk a copy of the entity list and look each one up again
    std::vector<Entity> list = w.pool<Script>().dense;
    for (Entity e : list) {
        Script* s = w.get<Script>(e);
        if (!s || s->module < 0 || !w.alive(e))
            continue;
        if (!s->started) {
            s->started = true;
            s->self = make_self(e, *s);
            Value self = s->self;
            call(s->module, "init", &self, 1, 0);
            s = w.get<Script>(e);
            if (!s)
                continue;
        }
        Value args[2] = { s->self, Value::number(dt) };
        call(s->module, "update", args, 2, 0);
    }
}

bool script_send(World& w, Entity e, const char* fn, std::initializer_list<Value> args, Value* result)
{
    Script* s = w.get<Script>(e);
    if (!s || s->module < 0)
        return false;
    if (!s->started) {
        s->started = true;
        s->self = make_self(e, *s);
    }
    Value all[16];
    int n = 0;
    all[n++] = s->self;
    for (const Value& v : args)
        if (n < 16)
            all[n++] = v;
    return call(s->module, fn, all, n, result);
}

Value json_to_value(const Json& j)
{
    switch (j.type) {
    case Json::NUL: return Value();
    case Json::BOOL: return Value::boolean(j.b);
    case Json::NUM: return Value::number(j.n);
    case Json::STR: return Value::string(j.s);
    case Json::ARR: {
        Value l = Value::list();
        for (const Json& e : j.arr)
            l.tab()->arr.push_back(json_to_value(e));
        return l;
    }
    case Json::OBJ: {
        Value t = Value::table();
        for (const auto& kv : j.obj)
            t.tab()->set(kv.first, json_to_value(kv.second));
        return t;
    }
    }
    return Value();
}

Json value_to_json(const Value& v)
{
    switch (v.type) {
    case V_BOOL: return Json::boolean(v.b);
    case V_NUM: return Json::number(v.n);
    case V_STR: return Json::string(v.str());
    case V_TABLE: {
        Table* t = v.tab();
        if (t->fields.empty()) {
            Json a = Json::array();
            for (const Value& e : t->arr)
                a.push(value_to_json(e));
            return a;
        }
        Json o = Json::object();
        for (const auto& kv : t->fields)
            if (!kv.second.is_nil())
                o.set(kv.first.c_str(), value_to_json(kv.second));
        return o;
    }
    default:
        return Json();
    }
}

const std::string& script_error()
{
    return error_text;
}

void script_clear_error()
{
    error_text.clear();
}
