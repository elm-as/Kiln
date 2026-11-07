#include "json.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const Json null_json;

const Json& Json::operator[](const char* key) const
{
    if (type == OBJ)
        for (const auto& kv : obj)
            if (kv.first == key)
                return kv.second;
    return null_json;
}

const Json& Json::operator[](size_t i) const
{
    if (type == ARR && i < arr.size())
        return arr[i];
    return null_json;
}

Json* Json::find(const char* key)
{
    if (type == OBJ)
        for (auto& kv : obj)
            if (kv.first == key)
                return &kv.second;
    return 0;
}

bool Json::has(const char* key) const
{
    if (type == OBJ)
        for (const auto& kv : obj)
            if (kv.first == key)
                return true;
    return false;
}

Json& Json::set(const char* key, Json v)
{
    if (type != OBJ) {
        *this = object();
    }
    if (Json* j = find(key)) {
        *j = std::move(v);
        return *j;
    }
    obj.emplace_back(key, std::move(v));
    return obj.back().second;
}

void Json::remove(const char* key)
{
    for (size_t i = 0; i < obj.size(); i++) {
        if (obj[i].first == key) {
            obj.erase(obj.begin() + i);
            return;
        }
    }
}

Json& Json::push(Json v)
{
    if (type != ARR)
        *this = array();
    arr.push_back(std::move(v));
    return arr.back();
}

bool operator==(const Json& a, const Json& b)
{
    if (a.type != b.type)
        return false;
    switch (a.type) {
    case Json::NUL: return true;
    case Json::BOOL: return a.b == b.b;
    case Json::NUM: return a.n == b.n;
    case Json::STR: return a.s == b.s;
    case Json::ARR:
        if (a.arr.size() != b.arr.size())
            return false;
        for (size_t i = 0; i < a.arr.size(); i++)
            if (a.arr[i] != b.arr[i])
                return false;
        return true;
    case Json::OBJ:
        // order doesn't matter for equality
        if (a.obj.size() != b.obj.size())
            return false;
        for (const auto& kv : a.obj) {
            if (!b.has(kv.first.c_str()) || b[kv.first.c_str()] != kv.second)
                return false;
        }
        return true;
    }
    return false;
}

void json_merge(Json& a, const Json& b)
{
    if (!b.is_obj() || !a.is_obj()) {
        a = b;
        return;
    }
    for (const auto& kv : b.obj) {
        Json* dst = a.find(kv.first.c_str());
        if (dst && dst->is_obj() && kv.second.is_obj())
            json_merge(*dst, kv.second);
        else
            a.set(kv.first.c_str(), kv.second);
    }
}

Json json_diff(const Json& a, const Json& b)
{
    if (!a.is_obj() || !b.is_obj())
        return a == b ? Json() : b;
    Json out;
    for (const auto& kv : b.obj) {
        const Json& old = a[kv.first.c_str()];
        if (!a.has(kv.first.c_str())) {
            out.set(kv.first.c_str(), kv.second);
        } else if (old.is_obj() && kv.second.is_obj()) {
            Json d = json_diff(old, kv.second);
            if (!d.is_null())
                out.set(kv.first.c_str(), d);
        } else if (old != kv.second) {
            out.set(kv.first.c_str(), kv.second);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// parser. Accepts // comments and trailing commas, both make hand editing nicer.

struct Parser {
    const char* p;
    const char* start;
    std::string err;

    int line() const
    {
        int n = 1;
        for (const char* c = start; c < p; c++)
            if (*c == '\n')
                n++;
        return n;
    }

    bool fail(const char* msg)
    {
        if (err.empty()) {
            char buf[128];
            snprintf(buf, sizeof(buf), "line %d: %s", line(), msg);
            err = buf;
        }
        return false;
    }

    void skip()
    {
        for (;;) {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                p++;
            if (p[0] == '/' && p[1] == '/') {
                while (*p && *p != '\n')
                    p++;
                continue;
            }
            break;
        }
    }

    bool string(std::string& out)
    {
        p++;    // opening quote
        while (*p != '"') {
            if (!*p || *p == '\n')
                return fail("unterminated string");
            if (*p == '\\') {
                p++;
                switch (*p) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '/': out += '/'; break;
                case '\\': out += '\\'; break;
                case '"': out += '"'; break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 1; i <= 4; i++) {
                        char c = p[i];
                        cp <<= 4;
                        if (c >= '0' && c <= '9') cp |= c - '0';
                        else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
                        else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
                        else return fail("bad \\u escape");
                    }
                    p += 4;
                    // utf-8 encode, surrogate pairs not handled
                    if (cp < 0x80) {
                        out += char(cp);
                    } else if (cp < 0x800) {
                        out += char(0xc0 | cp >> 6);
                        out += char(0x80 | (cp & 0x3f));
                    } else {
                        out += char(0xe0 | cp >> 12);
                        out += char(0x80 | ((cp >> 6) & 0x3f));
                        out += char(0x80 | (cp & 0x3f));
                    }
                    break;
                }
                default:
                    return fail("bad escape");
                }
                p++;
            } else {
                out += *p++;
            }
        }
        p++;
        return true;
    }

    bool value(Json& out, int depth)
    {
        if (depth > 64)
            return fail("nested too deep");
        skip();
        switch (*p) {
        case '{': {
            out = Json::object();
            p++;
            for (;;) {
                skip();
                if (*p == '}') {
                    p++;
                    return true;
                }
                if (*p != '"')
                    return fail("expected a key");
                std::string key;
                if (!string(key))
                    return false;
                skip();
                if (*p != ':')
                    return fail("expected ':'");
                p++;
                Json v;
                if (!value(v, depth + 1))
                    return false;
                out.obj.emplace_back(std::move(key), std::move(v));
                skip();
                if (*p == ',')
                    p++;
                else if (*p != '}')
                    return fail("expected ',' or '}'");
            }
        }
        case '[': {
            out = Json::array();
            p++;
            for (;;) {
                skip();
                if (*p == ']') {
                    p++;
                    return true;
                }
                Json v;
                if (!value(v, depth + 1))
                    return false;
                out.arr.push_back(std::move(v));
                skip();
                if (*p == ',')
                    p++;
                else if (*p != ']')
                    return fail("expected ',' or ']'");
            }
        }
        case '"':
            out = Json::string("");
            return string(out.s);
        case 't':
            if (strncmp(p, "true", 4)) return fail("unexpected character");
            p += 4;
            out = Json::boolean(true);
            return true;
        case 'f':
            if (strncmp(p, "false", 5)) return fail("unexpected character");
            p += 5;
            out = Json::boolean(false);
            return true;
        case 'n':
            if (strncmp(p, "null", 4)) return fail("unexpected character");
            p += 4;
            out = Json();
            return true;
        default: {
            if (*p != '-' && (*p < '0' || *p > '9'))
                return fail(*p ? "unexpected character" : "unexpected end of file");
            char* end;
            double d = strtod(p, &end);
            if (end == p)
                return fail("bad number");
            p = end;
            out = Json::number(d);
            return true;
        }
        }
    }
};

bool json_parse(const std::string& text, Json* out, std::string* err)
{
    Parser ps;
    ps.p = ps.start = text.c_str();
    Json j;
    bool ok = ps.value(j, 0);
    if (ok) {
        ps.skip();
        if (*ps.p)
            ok = ps.fail("junk after the end");
    }
    if (!ok) {
        if (err)
            *err = ps.err;
        return false;
    }
    *out = std::move(j);
    return true;
}
