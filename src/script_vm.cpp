#include "script_internal.h"
#include <cmath>

static Frame* up(Frame* f, int depth)
{
    while (depth--) f = f->parent.get();
    return f;
}

static const char* op_name(int op)
{
    switch (op) {
    case TK_PLUS: return "add";
    case TK_MINUS: return "subtract";
    case TK_STAR: return "multiply";
    case TK_SLASH: return "divide";
    case TK_PERCENT: return "take the modulo of";
    default: return "compare";
    }
}

static Value arith(VM& vm, int op, const Value& a, const Value& b)
{
    if (op == TK_EQ) return Value::boolean(values_equal(a, b));
    if (op == TK_NE) return Value::boolean(!values_equal(a, b));
    if (op == TK_PLUS && (a.type == V_STR || b.type == V_STR))
        return Value::string(value_to_string(a) + value_to_string(b));
    if (a.type == V_STR && b.type == V_STR) {
        int c = a.str().compare(b.str());
        if (op == TK_LT) return Value::boolean(c < 0);
        if (op == TK_LE) return Value::boolean(c <= 0);
        if (op == TK_GT) return Value::boolean(c > 0);
        if (op == TK_GE) return Value::boolean(c >= 0);
    }
    if (a.type != V_NUM || b.type != V_NUM)
        vm.error(std::string("can't ") + op_name(op) + " " + value_type_name(a) + " and " + value_type_name(b));
    double x = a.n, y = b.n;
    switch (op) {
    case TK_PLUS: return Value::number(x + y);
    case TK_MINUS: return Value::number(x - y);
    case TK_STAR: return Value::number(x * y);
    case TK_SLASH: return Value::number(x / y);
    case TK_PERCENT: return Value::number(y == 0 ? 0 : x - floor(x / y) * y);
    case TK_LT: return Value::boolean(x < y);
    case TK_LE: return Value::boolean(x <= y);
    case TK_GT: return Value::boolean(x > y);
    case TK_GE: return Value::boolean(x >= y);
    }
    vm.error("bad operator");
}

static Value index_get(VM& vm, const Value& obj, const Value& key)
{
    if (Table* t = obj.tab()) {
        if (key.type == V_NUM) {
            double i = key.n;
            if (i >= 0 && i < t->arr.size() && i == floor(i))
                return t->arr[(size_t)i];
            return Value();
        }
        if (key.type == V_STR)
            return t->get(key.str());
        vm.error(std::string("can't index a table with a ") + value_type_name(key));
    }
    if (obj.type == V_STR && key.type == V_NUM) {
        const std::string& s = obj.str();
        if (key.n >= 0 && key.n < s.size())
            return Value::string(std::string(1, s[(size_t)key.n]));
        return Value();
    }
    vm.error(std::string("can't index a ") + value_type_name(obj));
}

static void index_set(VM& vm, const Value& obj, const Value& key, const Value& v)
{
    Table* t = obj.tab();
    if (!t)
        vm.error(std::string("can't set an index on a ") + value_type_name(obj));
    if (key.type == V_NUM) {
        double i = key.n;
        if (i != floor(i) || i < 0 || i > t->arr.size())
            vm.error("list index " + value_to_string(key) + " out of range (size " + std::to_string(t->arr.size()) + ")");
        if (i == t->arr.size())
            t->arr.push_back(v);
        else
            t->arr[(size_t)i] = v;
        return;
    }
    if (key.type != V_STR)
        vm.error(std::string("can't use a ") + value_type_name(key) + " as a key");
    t->set(key.str(), v);
}

static Value eval(VM& vm, const Expr* e, const FrameP& f)
{
    switch (e->kind) {
    case E_NUM: return Value::number(e->num);
    case E_STR: return e->konst;
    case E_BOOL: return Value::boolean(e->flag);
    case E_NIL: return Value();
    case E_LOCAL: return up(f.get(), e->depth)->slots[e->slot];
    case E_GLOBAL: {
        const Value& v = vm.globals[e->slot];
        if (v.is_nil()) {
            vm.cur_line = e->line;
            vm.error("'" + e->name + "' is not defined");
        }
        return v;
    }
    case E_NEG: {
        Value v = eval(vm, e->a.get(), f);
        if (v.type != V_NUM) {
            vm.cur_line = e->line;
            vm.error(std::string("can't negate a ") + value_type_name(v));
        }
        return Value::number(-v.n);
    }
    case E_NOT: return Value::boolean(!eval(vm, e->a.get(), f).truthy());
    case E_AND: {
        Value l = eval(vm, e->a.get(), f);
        return l.truthy() ? eval(vm, e->b.get(), f) : l;
    }
    case E_OR: {
        Value l = eval(vm, e->a.get(), f);
        return l.truthy() ? l : eval(vm, e->b.get(), f);
    }
    case E_BIN: {
        Value a = eval(vm, e->a.get(), f);
        Value b = eval(vm, e->b.get(), f);
        vm.cur_line = e->line;
        return arith(vm, e->op, a, b);
    }
    case E_CALL: {
        Value fn = eval(vm, e->a.get(), f);
        Value args[16];
        int n = (int)e->items.size();
        if (n > 16) {
            vm.cur_line = e->line;
            vm.error("more than 16 arguments");
        }
        for (int i = 0; i < n; i++)
            args[i] = eval(vm, e->items[i].get(), f);
        vm.cur_line = e->line;
        return vm.invoke(fn, args, n);
    }
    case E_INDEX: {
        Value obj = eval(vm, e->a.get(), f);
        Value key = eval(vm, e->b.get(), f);
        vm.cur_line = e->line;
        return index_get(vm, obj, key);
    }
    case E_FIELD: {
        Value obj = eval(vm, e->a.get(), f);
        if (Table* t = obj.tab())
            return t->get(e->name);
        vm.cur_line = e->line;
        vm.error("can't read ." + e->name + " of a " + value_type_name(obj));
    }
    case E_LIST: {
        Value l = Value::list();
        Table* t = l.tab();
        t->arr.reserve(e->items.size());
        for (const ExprP& it : e->items)
            t->arr.push_back(eval(vm, it.get(), f));
        return l;
    }
    case E_TABLE: {
        Value v = Value::table();
        Table* t = v.tab();
        for (size_t i = 0; i < e->items.size(); i++)
            t->set(e->keys[i], eval(vm, e->items[i].get(), f));
        return v;
    }
    case E_FUNC: {
        Value v;
        v.type = V_FUNC;
        auto c = std::make_shared<Closure>();
        c->proto = e->fn;
        c->env = f;
        v.obj = c;
        return v;
    }
    }
    return Value();
}

static void assign(VM& vm, const Stmt* s, const FrameP& f)
{
    const Expr* t = s->target.get();
    Value v = eval(vm, s->value.get(), f);
    if (s->op) {
        Value old = eval(vm, t, f);
        vm.cur_line = s->line;
        v = arith(vm, s->op, old, v);
    }
    switch (t->kind) {
    case E_LOCAL:
        up(f.get(), t->depth)->slots[t->slot] = std::move(v);
        break;
    case E_INDEX: {
        Value obj = eval(vm, t->a.get(), f);
        Value key = eval(vm, t->b.get(), f);
        vm.cur_line = s->line;
        index_set(vm, obj, key, v);
        break;
    }
    case E_FIELD: {
        Value obj = eval(vm, t->a.get(), f);
        Table* tab = obj.tab();
        if (!tab) {
            vm.cur_line = s->line;
            vm.error("can't set ." + t->name + " on a " + value_type_name(obj));
        }
        tab->set(t->name, v);
        break;
    }
    default:
        break;
    }
}

static void spend(VM& vm)
{
    if (--vm.budget < 0)
        vm.error("script ran too long, infinite loop?");
}

static Flow exec(VM& vm, const Stmt* s, const FrameP& f)
{
    vm.cur_line = s->line;
    spend(vm);

    switch (s->kind) {
    case S_EXPR: eval(vm, s->value.get(), f); return FLOW_NORMAL;
    case S_SET: assign(vm, s, f); return FLOW_NORMAL;
    case S_IF:
        return eval(vm, s->value.get(), f).truthy() ? script_exec_block(vm, s->body, f) : script_exec_block(vm, s->else_body, f);
    case S_WHILE:
        while (eval(vm, s->value.get(), f).truthy()) {
            spend(vm);  // an empty body would never hit the check in exec
            Flow fl = script_exec_block(vm, s->body, f);
            if (fl == FLOW_RETURN) return fl;
            if (fl == FLOW_BREAK) break;
        }
        return FLOW_NORMAL;
    case S_FOR_RANGE: {
        Value a = eval(vm, s->value.get(), f);
        Value b = eval(vm, s->extra.get(), f);
        if (a.type != V_NUM || b.type != V_NUM) {
            vm.cur_line = s->line;
            vm.error("a range needs numbers");
        }
        for (double i = a.n; i < b.n; i++) {
            spend(vm);
            f->slots[s->slot_a] = Value::number(i);
            Flow fl = script_exec_block(vm, s->body, f);
            if (fl == FLOW_RETURN) return fl;
            if (fl == FLOW_BREAK) break;
        }
        return FLOW_NORMAL;
    }
    case S_FOR_IN: {
        Value coll = eval(vm, s->value.get(), f);
        Table* t = coll.tab();
        if (!t) {
            vm.cur_line = s->line;
            vm.error(std::string("can't loop over a ") + value_type_name(coll));
        }
        bool pairs = s->slot_b >= 0;
        // the list can change while we walk it, so check the size every time
        for (size_t i = 0; i < t->arr.size(); i++) {
            if (pairs) {
                f->slots[s->slot_a] = Value::number((double)i);
                f->slots[s->slot_b] = t->arr[i];
            } else {
                f->slots[s->slot_a] = t->arr[i];
            }
            Flow fl = script_exec_block(vm, s->body, f);
            if (fl == FLOW_RETURN) return fl;
            if (fl == FLOW_BREAK) return FLOW_NORMAL;
        }
        for (size_t i = 0; i < t->fields.size(); i++) {
            if (t->fields[i].second.is_nil())
                continue;
            if (pairs) {
                f->slots[s->slot_a] = Value::string(t->fields[i].first);
                f->slots[s->slot_b] = t->fields[i].second;
            } else {
                f->slots[s->slot_a] = t->fields[i].second;
            }
            Flow fl = script_exec_block(vm, s->body, f);
            if (fl == FLOW_RETURN) return fl;
            if (fl == FLOW_BREAK) break;
        }
        return FLOW_NORMAL;
    }
    case S_RETURN:
        vm.ret = s->value ? eval(vm, s->value.get(), f) : Value();
        return FLOW_RETURN;
    case S_BREAK: return FLOW_BREAK;
    case S_CONTINUE: return FLOW_CONTINUE;
    case S_BLOCK: return script_exec_block(vm, s->body, f);
    }
    return FLOW_NORMAL;
}

Flow script_exec_block(VM& vm, const std::vector<StmtP>& body, const FrameP& f)
{
    for (const StmtP& s : body) {
        Flow fl = exec(vm, s.get(), f);
        if (fl != FLOW_NORMAL)
            return fl;
    }
    return FLOW_NORMAL;
}

Value VM::invoke(const Value& fn, const Value* args, int argc)
{
    if (fn.type == V_NATIVE)
        return fn.native(*this, args, argc);
    if (fn.type != V_FUNC)
        error(std::string("trying to call a ") + value_type_name(fn));
    if (depth >= 200)
        error("too much recursion");

    Closure* c = static_cast<Closure*>(fn.obj.get());
    FuncProto* p = c->proto.get();
    auto frame = std::make_shared<Frame>();
    frame->slots.resize(p->nslots);
    frame->parent = c->env;
    for (int i = 0; i < p->nparams && i < argc; i++)
        frame->slots[i] = args[i];

    const std::string* saved_source = cur_source;
    int saved_line = cur_line;
    cur_source = &p->source;
    depth++;
    Flow fl = script_exec_block(*this, p->body, frame);
    depth--;
    cur_source = saved_source;
    cur_line = saved_line;

    Value r;
    if (fl == FLOW_RETURN) {
        r = std::move(ret);
        ret = Value();
    }
    return r;
}
