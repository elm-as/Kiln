#include "script_internal.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// values

Value Value::string(const std::string& s)
{
    Value r;
    r.type = V_STR;
    auto o = std::make_shared<Str>();
    o->s = s;
    r.obj = o;
    return r;
}

Value Value::table()
{
    Value r;
    r.type = V_TABLE;
    r.obj = std::make_shared<Table>();
    return r;
}

Value Value::list()
{
    return table();
}

const std::string& Value::str() const
{
    static const std::string empty;
    return type == V_STR ? static_cast<Str*>(obj.get())->s : empty;
}

Table* Value::tab() const
{
    return type == V_TABLE ? static_cast<Table*>(obj.get()) : 0;
}

Value Table::get(const std::string& key) const
{
    auto it = index.find(key);
    return it == index.end() ? Value() : fields[it->second].second;
}

void Table::set(const std::string& key, const Value& v)
{
    auto it = index.find(key);
    if (it != index.end()) {
        fields[it->second].second = v;
        return;
    }
    index[key] = fields.size();
    fields.emplace_back(key, v);
}

const char* value_type_name(const Value& v)
{
    switch (v.type) {
    case V_NIL: return "nil";
    case V_BOOL: return "bool";
    case V_NUM: return "number";
    case V_STR: return "string";
    case V_TABLE: return "table";
    case V_FUNC: return "function";
    case V_NATIVE: return "function";
    }
    return "?";
}

static void to_string(std::string& out, const Value& v, int depth)
{
    char buf[32];
    switch (v.type) {
    case V_NIL: out += "nil"; break;
    case V_BOOL: out += v.b ? "true" : "false"; break;
    case V_NUM:
        if (v.n == floor(v.n) && fabs(v.n) < 1e15)
            snprintf(buf, sizeof(buf), "%.0f", v.n);
        else
            snprintf(buf, sizeof(buf), "%g", v.n);
        out += buf;
        break;
    case V_STR:
        if (depth > 0) {
            out += '"';
            out += v.str();
            out += '"';
        } else {
            out += v.str();
        }
        break;
    case V_TABLE: {
        Table* t = v.tab();
        if (depth > 2) {
            out += "{...}";
            break;
        }
        bool list = t->fields.empty();
        out += list ? "[" : "{";
        bool first = true;
        for (const Value& e : t->arr) {
            if (!first) out += ", ";
            first = false;
            to_string(out, e, depth + 1);
        }
        for (const auto& kv : t->fields) {
            if (kv.second.is_nil())
                continue;
            if (!first) out += ", ";
            first = false;
            out += kv.first;
            out += ": ";
            to_string(out, kv.second, depth + 1);
        }
        out += list ? "]" : "}";
        break;
    }
    case V_FUNC: out += "<fn>"; break;
    case V_NATIVE: out += "<native fn>"; break;
    }
}

std::string value_to_string(const Value& v)
{
    std::string s;
    to_string(s, v, 0);
    return s;
}

bool values_equal(const Value& a, const Value& b)
{
    if (a.type != b.type)
        return false;
    switch (a.type) {
    case V_NIL: return true;
    case V_BOOL: return a.b == b.b;
    case V_NUM: return a.n == b.n;
    case V_STR: return a.str() == b.str();
    case V_NATIVE: return a.native == b.native;
    default: return a.obj == b.obj;
    }
}

// ---------------------------------------------------------------------------
// lexer

static const struct { const char* word; TokKind kind; } keywords[] = {
    { "let", TK_LET }, { "fn", TK_FN }, { "if", TK_IF }, { "elif", TK_ELIF },
    { "else", TK_ELSE }, { "while", TK_WHILE }, { "for", TK_FOR }, { "in", TK_IN },
    { "return", TK_RETURN }, { "break", TK_BREAK }, { "continue", TK_CONTINUE },
    { "true", TK_TRUE }, { "false", TK_FALSE }, { "nil", TK_NIL },
    { "and", TK_AND }, { "or", TK_OR }, { "not", TK_NOT },
};

void script_lex(const std::string& src, const std::string& name, std::vector<Token>& out)
{
    const char* p = src.c_str();
    int line = 1;
    auto fail = [&](const char* msg) {
        throw ScriptError{ name + ":" + std::to_string(line) + ": " + msg };
    };
    auto add = [&](TokKind k, int len) {
        out.push_back({ k, line, 0, std::string() });
        p += len;
    };

    for (;;) {
        char c = *p;
        if (c == 0)
            break;
        if (c == '\n') {
            line++;
            p++;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            p++;
            continue;
        }
        if (c == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;
            continue;
        }
        if (isdigit((unsigned char)c)) {
            const char* start = p;
            while (isdigit((unsigned char)*p))
                p++;
            // "0..10" is a range, not the number "0."
            if (*p == '.' && isdigit((unsigned char)p[1])) {
                p++;
                while (isdigit((unsigned char)*p))
                    p++;
            }
            out.push_back({ TK_NUM, line, strtod(start, 0), std::string() });
            continue;
        }
        if (isalpha((unsigned char)c) || c == '_') {
            const char* start = p;
            while (isalnum((unsigned char)*p) || *p == '_')
                p++;
            std::string word(start, p);
            TokKind k = TK_IDENT;
            for (const auto& kw : keywords)
                if (word == kw.word)
                    k = kw.kind;
            out.push_back({ k, line, 0, k == TK_IDENT ? word : std::string() });
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = c;
            p++;
            std::string s;
            while (*p != quote) {
                if (!*p || *p == '\n')
                    fail("unterminated string");
                if (*p == '\\') {
                    p++;
                    switch (*p) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case '\\': s += '\\'; break;
                    case '"': s += '"'; break;
                    case '\'': s += '\''; break;
                    default: fail("bad escape in string");
                    }
                    p++;
                } else {
                    s += *p++;
                }
            }
            p++;
            out.push_back({ TK_STR, line, 0, s });
            continue;
        }

        char n = p[1];
        switch (c) {
        case '(': add(TK_LPAREN, 1); break;
        case ')': add(TK_RPAREN, 1); break;
        case '{': add(TK_LBRACE, 1); break;
        case '}': add(TK_RBRACE, 1); break;
        case '[': add(TK_LBRACKET, 1); break;
        case ']': add(TK_RBRACKET, 1); break;
        case ',': add(TK_COMMA, 1); break;
        case ':': add(TK_COLON, 1); break;
        case ';': add(TK_SEMI, 1); break;
        case '%': add(TK_PERCENT, 1); break;
        case '.': if (n == '.') add(TK_DOTDOT, 2); else add(TK_DOT, 1); break;
        case '+': if (n == '=') add(TK_PLUS_EQ, 2); else add(TK_PLUS, 1); break;
        case '-': if (n == '=') add(TK_MINUS_EQ, 2); else add(TK_MINUS, 1); break;
        case '*': if (n == '=') add(TK_STAR_EQ, 2); else add(TK_STAR, 1); break;
        case '/': if (n == '=') add(TK_SLASH_EQ, 2); else add(TK_SLASH, 1); break;
        case '=': if (n == '=') add(TK_EQ, 2); else add(TK_ASSIGN, 1); break;
        case '!': if (n == '=') add(TK_NE, 2); else fail("'!' on its own, did you mean 'not'?"); break;
        case '<': if (n == '=') add(TK_LE, 2); else add(TK_LT, 1); break;
        case '>': if (n == '=') add(TK_GE, 2); else add(TK_GT, 1); break;
        default: {
            char msg[40];
            snprintf(msg, sizeof(msg), "unexpected character '%c'", c);
            fail(msg);
        }
        }
    }
    out.push_back({ TK_EOF, line, 0, std::string() });
}
