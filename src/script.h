#pragma once
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// KScript: small dynamic language for gameplay code. Tree-walking interpreter,
// variables resolved to slots at compile time. Values are refcounted with
// shared_ptr, so a cycle (a table holding a closure that holds the table)
// leaks. Scripts get reloaded often enough that it hasn't mattered.
//
//   let speed = 40
//   fn update(self, dt) {
//       if on_wall(self.id) != 0 { self.dir = -self.dir }
//       set_vel(self.id, self.dir * speed, vel_y(self.id))
//   }
//
// Only nil and false are falsy. Numbers are doubles. + joins strings.
// [1, 2, 3] is a list (0 based), {x: 1, y: 2} a table, both are the same type.

struct VM;
struct Value;
struct Frame;
struct FuncProto;

typedef Value (*NativeFn)(VM& vm, const Value* args, int argc);

enum ValueType : unsigned char { V_NIL, V_BOOL, V_NUM, V_STR, V_TABLE, V_FUNC, V_NATIVE };

struct Object {
    virtual ~Object() {}
};

struct Value {
    ValueType type = V_NIL;
    bool b = false;
    double n = 0;
    NativeFn native = 0;
    std::shared_ptr<Object> obj;

    static Value boolean(bool v) { Value r; r.type = V_BOOL; r.b = v; return r; }
    static Value number(double v) { Value r; r.type = V_NUM; r.n = v; return r; }
    static Value string(const std::string& s);
    static Value table();
    static Value list();
    static Value fn(NativeFn f) { Value r; r.type = V_NATIVE; r.native = f; return r; }

    bool is_nil() const { return type == V_NIL; }
    bool truthy() const { return !(type == V_NIL || (type == V_BOOL && !b)); }
    const std::string& str() const;     // "" if not a string
    struct Table* tab() const;          // null if not a table
};

struct Str : Object {
    std::string s;
};

struct Table : Object {
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> fields;      // insertion order
    std::unordered_map<std::string, size_t> index;

    Value get(const std::string& key) const;
    void set(const std::string& key, const Value& v);
};

struct Closure : Object {
    std::shared_ptr<FuncProto> proto;
    std::shared_ptr<Frame> env;
};

struct Module {
    std::string name;
    std::shared_ptr<FuncProto> chunk;
    std::shared_ptr<Frame> frame;
    std::unordered_map<std::string, int> exports;   // top level names -> slots
    bool ok = false;

    ~Module();
    Value get(const char* name) const;
};

struct ScriptError {
    std::string msg;
};

struct VM {
    VM();
    ~VM();

    void def(const char* name, NativeFn fn);
    void set_global(const char* name, const Value& v);
    Value get_global(const char* name);

    // compiles and runs the top level. On failure the module keeps whatever it
    // had before, which is what you want when hot reloading a broken file
    bool load(Module& m, const std::string& source, const std::string& name);

    // protected call, errors go to last_error and the error hook
    bool call(const Value& fn, std::initializer_list<Value> args, Value* result = 0);
    bool call(const Value& fn, const Value* args, int argc, Value* result = 0);

    // for natives: throw a script error that points at the current line
    [[noreturn]] void error(const std::string& msg);
    double num(const Value* args, int argc, int i);
    const std::string& string_arg(const Value* args, int argc, int i);

    std::string last_error;
    void (*on_error)(const std::string& msg) = 0;
    void (*on_print)(const std::string& msg) = 0;
    void* user = 0;

    // internals, used by the interpreter
    std::vector<Value> globals;
    std::unordered_map<std::string, int> global_index;
    std::vector<std::string> global_names;
    int global_slot(const std::string& name);

    const std::string* cur_source = 0;
    int cur_line = 0;
    int depth = 0;
    long budget = 0;
    Value ret;

    Value invoke(const Value& fn, const Value* args, int argc);
};

std::string value_to_string(const Value& v);
const char* value_type_name(const Value& v);
bool values_equal(const Value& a, const Value& b);

// compile only, used by the asset cooker. err is "name:line: message"
bool script_check(const std::string& source, const std::string& name, std::string* err);
