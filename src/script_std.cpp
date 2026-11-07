#include "script_internal.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>

void VM::error(const std::string& msg)
{
    std::string where = cur_source ? *cur_source : std::string("?");
    throw ScriptError{ where + ":" + std::to_string(cur_line) + ": " + msg };
}

double VM::num(const Value* args, int argc, int i)
{
    if (i >= argc || args[i].type != V_NUM)
        error("argument " + std::to_string(i + 1) + " should be a number, got " +
              (i < argc ? value_type_name(args[i]) : "nothing"));
    return args[i].n;
}

const std::string& VM::string_arg(const Value* args, int argc, int i)
{
    if (i >= argc || args[i].type != V_STR)
        error("argument " + std::to_string(i + 1) + " should be a string, got " +
              (i < argc ? value_type_name(args[i]) : "nothing"));
    return args[i].str();
}

int VM::global_slot(const std::string& name)
{
    auto it = global_index.find(name);
    if (it != global_index.end())
        return it->second;
    int slot = (int)globals.size();
    globals.push_back(Value());
    global_names.push_back(name);
    global_index[name] = slot;
    return slot;
}

void VM::def(const char* name, NativeFn fn)
{
    globals[global_slot(name)] = Value::fn(fn);
}

void VM::set_global(const char* name, const Value& v)
{
    globals[global_slot(name)] = v;
}

Value VM::get_global(const char* name)
{
    auto it = global_index.find(name);
    return it == global_index.end() ? Value() : globals[it->second];
}

static void report(VM& vm, const std::string& msg)
{
    vm.last_error = msg;
    if (vm.on_error)
        vm.on_error(msg);
    else
        fprintf(stderr, "script error: %s\n", msg.c_str());
}

bool VM::load(Module& m, const std::string& src, const std::string& name)
{
    try {
        Parser ps;
        ps.vm = this;
        ps.source = name;
        std::unordered_map<std::string, int> exports;
        std::shared_ptr<FuncProto> chunk = ps.compile(src, &exports);

        auto frame = std::make_shared<Frame>();
        frame->slots.resize(chunk->nslots);
        const std::string* saved = cur_source;
        cur_source = &chunk->source;
        int saved_budget = budget;
        budget = 2000000;
        int saved_depth = depth;
        try {
            script_exec_block(*this, chunk->body, frame);
        } catch (...) {
            frame->slots.clear();
            cur_source = saved;
            depth = saved_depth;
            if (depth > 0)
                budget = saved_budget;
            throw;
        }
        cur_source = saved;
        if (depth > 0)
            budget = saved_budget;

        if (m.frame)
            m.frame->slots.clear();
        m.name = name;
        m.chunk = chunk;
        m.frame = frame;
        m.exports = exports;
        m.ok = true;
        return true;
    } catch (ScriptError& e) {
        report(*this, e.msg);
        return false;
    }
}

bool VM::call(const Value& fn, const Value* args, int argc, Value* result)
{
    if (depth == 0)
        budget = 2000000;
    int saved_depth = depth;
    const std::string* saved_source = cur_source;
    try {
        Value r = invoke(fn, args, argc);
        if (result)
            *result = r;
        return true;
    } catch (ScriptError& e) {
        depth = saved_depth;
        cur_source = saved_source;
        report(*this, e.msg);
        return false;
    }
}

bool VM::call(const Value& fn, std::initializer_list<Value> args, Value* result)
{
    return call(fn, args.begin(), (int)args.size(), result);
}

Module::~Module()
{
    if (frame)
        frame->slots.clear();
}

Value Module::get(const char* n) const
{
    auto it = exports.find(n);
    if (it == exports.end() || !frame)
        return Value();
    return frame->slots[it->second];
}

// ---------------------------------------------------------------------------
// builtins

static uint32_t rng = (uint32_t)time(0) | 1;

static double rand01()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng >> 8) / 16777216.0;
}

static Value b_print(VM& vm, const Value* a, int n)
{
    std::string s;
    for (int i = 0; i < n; i++) {
        if (i) s += " ";
        s += value_to_string(a[i]);
    }
    if (vm.on_print)
        vm.on_print(s);
    else
        printf("%s\n", s.c_str());
    return Value();
}

static Value b_len(VM& vm, const Value* a, int n)
{
    if (n > 0 && a[0].type == V_STR) return Value::number((double)a[0].str().size());
    if (n > 0 && a[0].tab()) {
        Table* t = a[0].tab();
        return Value::number(double(t->arr.size()));
    }
    vm.error("len() wants a list or a string");
}

static Value b_push(VM& vm, const Value* a, int n)
{
    if (n < 2 || !a[0].tab())
        vm.error("push(list, value)");
    a[0].tab()->arr.push_back(a[1]);
    return a[1];
}

static Value b_pop(VM& vm, const Value* a, int n)
{
    if (n < 1 || !a[0].tab())
        vm.error("pop(list)");
    std::vector<Value>& arr = a[0].tab()->arr;
    if (arr.empty())
        return Value();
    Value v = arr.back();
    arr.pop_back();
    return v;
}

static Value b_remove(VM& vm, const Value* a, int n)
{
    if (n < 2 || !a[0].tab())
        vm.error("remove(list, index) or remove(table, key)");
    Table* t = a[0].tab();
    if (a[1].type == V_NUM) {
        size_t i = (size_t)a[1].n;
        if (i >= t->arr.size())
            return Value();
        Value v = t->arr[i];
        t->arr.erase(t->arr.begin() + i);
        return v;
    }
    Value v = t->get(a[1].str());
    t->set(a[1].str(), Value());
    return v;
}

static Value b_keys(VM& vm, const Value* a, int n)
{
    if (n < 1 || !a[0].tab())
        vm.error("keys(table)");
    Value out = Value::list();
    for (auto& kv : a[0].tab()->fields)
        if (!kv.second.is_nil())
            out.tab()->arr.push_back(Value::string(kv.first));
    return out;
}

static Value b_has(VM& vm, const Value* a, int n)
{
    if (n < 2 || !a[0].tab())
        vm.error("has(table, key)");
    Table* t = a[0].tab();
    if (a[1].type == V_NUM)
        return Value::boolean(a[1].n >= 0 && a[1].n < t->arr.size());
    return Value::boolean(!t->get(a[1].str()).is_nil());
}

static Value b_str(VM&, const Value* a, int n) { return Value::string(n ? value_to_string(a[0]) : ""); }

static Value b_num(VM&, const Value* a, int n)
{
    if (n && a[0].type == V_NUM) return a[0];
    if (n && a[0].type == V_STR) {
        char* end;
        double d = strtod(a[0].str().c_str(), &end);
        if (end != a[0].str().c_str())
            return Value::number(d);
    }
    return Value();
}

static Value b_type(VM&, const Value* a, int n) { return Value::string(n ? value_type_name(a[0]) : "nil"); }
static Value b_floor(VM& vm, const Value* a, int n) { return Value::number(floor(vm.num(a, n, 0))); }
static Value b_ceil(VM& vm, const Value* a, int n) { return Value::number(ceil(vm.num(a, n, 0))); }
static Value b_round(VM& vm, const Value* a, int n) { return Value::number(floor(vm.num(a, n, 0) + 0.5)); }
static Value b_abs(VM& vm, const Value* a, int n) { return Value::number(fabs(vm.num(a, n, 0))); }
static Value b_sqrt(VM& vm, const Value* a, int n) { return Value::number(sqrt(vm.num(a, n, 0))); }
static Value b_sin(VM& vm, const Value* a, int n) { return Value::number(sin(vm.num(a, n, 0))); }
static Value b_cos(VM& vm, const Value* a, int n) { return Value::number(cos(vm.num(a, n, 0))); }
static Value b_atan2(VM& vm, const Value* a, int n) { return Value::number(atan2(vm.num(a, n, 0), vm.num(a, n, 1))); }

static Value b_sign(VM& vm, const Value* a, int n)
{
    double x = vm.num(a, n, 0);
    return Value::number(x > 0 ? 1 : x < 0 ? -1 : 0);
}

static Value b_min(VM& vm, const Value* a, int n) { return Value::number(fmin(vm.num(a, n, 0), vm.num(a, n, 1))); }
static Value b_max(VM& vm, const Value* a, int n) { return Value::number(fmax(vm.num(a, n, 0), vm.num(a, n, 1))); }

static Value b_clamp(VM& vm, const Value* a, int n)
{
    double v = vm.num(a, n, 0), lo = vm.num(a, n, 1), hi = vm.num(a, n, 2);
    return Value::number(v < lo ? lo : v > hi ? hi : v);
}

static Value b_lerp(VM& vm, const Value* a, int n)
{
    double x = vm.num(a, n, 0), y = vm.num(a, n, 1), t = vm.num(a, n, 2);
    return Value::number(x + (y - x) * t);
}

// rand() is 0..1, rand(a, b) is a..b
static Value b_rand(VM& vm, const Value* a, int n)
{
    if (n == 0)
        return Value::number(rand01());
    double lo = vm.num(a, n, 0), hi = vm.num(a, n, 1);
    return Value::number(lo + (hi - lo) * rand01());
}

// inclusive on both ends
static Value b_randi(VM& vm, const Value* a, int n)
{
    double lo = vm.num(a, n, 0), hi = vm.num(a, n, 1);
    return Value::number(floor(lo + (hi - lo + 1) * rand01()));
}

VM::VM()
{
    def("print", b_print);
    def("len", b_len);
    def("push", b_push);
    def("pop", b_pop);
    def("remove", b_remove);
    def("keys", b_keys);
    def("has", b_has);
    def("str", b_str);
    def("num", b_num);
    def("type", b_type);
    def("floor", b_floor);
    def("ceil", b_ceil);
    def("round", b_round);
    def("abs", b_abs);
    def("sqrt", b_sqrt);
    def("sin", b_sin);
    def("cos", b_cos);
    def("atan2", b_atan2);
    def("sign", b_sign);
    def("min", b_min);
    def("max", b_max);
    def("clamp", b_clamp);
    def("lerp", b_lerp);
    def("rand", b_rand);
    def("randi", b_randi);
    set_global("pi", Value::number(3.14159265358979));
}

VM::~VM() {}

