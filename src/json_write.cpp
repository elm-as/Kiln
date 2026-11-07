// JSON serializer (compact and pretty-printed with inline short arrays).

#include "json.h"
#include <cmath>
#include <cstdio>

static void write_string(std::string& out, const std::string& s)
{
    out += '"';
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        case '\r': out += "\\r"; break;
        default:
            if ((unsigned char)c < 32) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

static void write_number(std::string& out, double n)
{
    char buf[32];
    if (std::isfinite(n) && n == floor(n) && fabs(n) < 1e15)
        snprintf(buf, sizeof(buf), "%.0f", n);
    else if (std::isfinite(n))
        snprintf(buf, sizeof(buf), "%.9g", n);
    else
        snprintf(buf, sizeof(buf), "0");
    out += buf;
}

static void write(std::string& out, const Json& j, bool pretty, int indent);

static void write_compact(std::string& out, const Json& j)
{
    write(out, j, false, 0);
}

// short arrays of plain values stay on one line, it keeps things like
// positions and colors readable
static bool fits_inline(const Json& j)
{
    if (j.type != Json::ARR && j.type != Json::OBJ)
        return true;
    if (j.type == Json::OBJ && !j.obj.empty())
        return false;
    for (const Json& e : j.arr)
        if (e.type == Json::ARR || e.type == Json::OBJ)
            return false;
    std::string tmp;
    write_compact(tmp, j);
    return tmp.size() <= 72;
}

static void newline(std::string& out, int indent)
{
    out += '\n';
    out.append(indent * 2, ' ');
}

static void write(std::string& out, const Json& j, bool pretty, int indent)
{
    switch (j.type) {
    case Json::NUL: out += "null"; break;
    case Json::BOOL: out += j.b ? "true" : "false"; break;
    case Json::NUM: write_number(out, j.n); break;
    case Json::STR: write_string(out, j.s); break;
    case Json::ARR:
        if (j.arr.empty()) {
            out += "[]";
        } else if (!pretty || fits_inline(j)) {
            out += '[';
            for (size_t i = 0; i < j.arr.size(); i++) {
                if (i) out += pretty ? ", " : ",";
                write(out, j.arr[i], false, 0);
            }
            out += ']';
        } else {
            out += '[';
            for (size_t i = 0; i < j.arr.size(); i++) {
                if (i) out += ',';
                newline(out, indent + 1);
                write(out, j.arr[i], true, indent + 1);
            }
            newline(out, indent);
            out += ']';
        }
        break;
    case Json::OBJ:
        if (j.obj.empty()) {
            out += "{}";
            break;
        }
        out += '{';
        for (size_t i = 0; i < j.obj.size(); i++) {
            if (i) out += ',';
            if (pretty)
                newline(out, indent + 1);
            write_string(out, j.obj[i].first);
            out += pretty ? ": " : ":";
            write(out, j.obj[i].second, pretty, indent + 1);
        }
        if (pretty)
            newline(out, indent);
        out += '}';
        break;
    }
}

std::string json_write(const Json& j, bool pretty)
{
    std::string out;
    write(out, j, pretty, 0);
    if (pretty)
        out += '\n';
    return out;
}

