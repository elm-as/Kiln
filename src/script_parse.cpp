#include "script_internal.h"

[[noreturn]] void Parser::fail(const std::string& msg, int line)
{
    if (line < 0)
        line = toks[pos].line;
    throw ScriptError{ source + ":" + std::to_string(line) + ": " + msg };
}

const Token& Parser::peek(int ahead) const
{
    size_t i = pos + ahead;
    return toks[i < toks.size() ? i : toks.size() - 1];
}

bool Parser::check(TokKind k) const
{
    return peek().kind == k;
}

const Token& Parser::next()
{
    const Token& t = toks[pos];
    if (pos + 1 < toks.size())
        pos++;
    return t;
}

bool Parser::match(TokKind k)
{
    if (!check(k))
        return false;
    next();
    return true;
}

const Token& Parser::expect(TokKind k, const char* what)
{
    if (!check(k))
        fail(std::string("expected ") + what);
    return next();
}

ExprP Parser::make(ExprKind k, int line)
{
    ExprP e(new Expr());
    e->kind = k;
    e->line = line;
    return e;
}

int Parser::global(const std::string& name)
{
    return vm ? vm->global_slot(name) : 0;
}

int Parser::declare(const std::string& name)
{
    FuncState& f = funcs.back();
    Scope& s = f.scopes.back();
    for (auto& p : s.names)
        if (p.first == name)
            return p.second;
    int slot = f.proto->nslots++;
    s.names.push_back({ name, slot });
    return slot;
}

bool Parser::resolve(const std::string& name, int* depth, int* slot)
{
    for (int d = (int)funcs.size() - 1; d >= 0; d--) {
        FuncState& f = funcs[d];
        for (int s = (int)f.scopes.size() - 1; s >= 0; s--) {
            for (auto& p : f.scopes[s].names) {
                if (p.first == name) {
                    *depth = (int)funcs.size() - 1 - d;
                    *slot = p.second;
                    return true;
                }
            }
        }
    }
    return false;
}

ExprP Parser::variable(const std::string& name, int line)
{
    int depth, slot;
    if (resolve(name, &depth, &slot)) {
        ExprP e = make(E_LOCAL, line);
        e->depth = depth;
        e->slot = slot;
        e->name = name;
        return e;
    }
    ExprP e = make(E_GLOBAL, line);
    e->slot = global(name);
    e->name = name;
    return e;
}

ExprP Parser::expression()
{
    return or_expr();
}

ExprP Parser::or_expr()
{
    ExprP l = and_expr();
    while (check(TK_OR)) {
        int line = next().line;
        ExprP e = make(E_OR, line);
        e->a = std::move(l);
        e->b = and_expr();
        l = std::move(e);
    }
    return l;
}

ExprP Parser::and_expr()
{
    ExprP l = not_expr();
    while (check(TK_AND)) {
        int line = next().line;
        ExprP e = make(E_AND, line);
        e->a = std::move(l);
        e->b = not_expr();
        l = std::move(e);
    }
    return l;
}

ExprP Parser::not_expr()
{
    if (check(TK_NOT)) {
        int line = next().line;
        ExprP e = make(E_NOT, line);
        e->a = not_expr();
        return e;
    }
    return comparison();
}

ExprP Parser::binary(ExprP l, int op, ExprP r, int line)
{
    ExprP e = make(E_BIN, line);
    e->op = op;
    e->a = std::move(l);
    e->b = std::move(r);
    return e;
}

ExprP Parser::comparison()
{
    ExprP l = additive();
    for (;;) {
        TokKind k = peek().kind;
        if (k != TK_EQ && k != TK_NE && k != TK_LT && k != TK_LE && k != TK_GT && k != TK_GE)
            return l;
        int line = next().line;
        l = binary(std::move(l), k, additive(), line);
    }
}

ExprP Parser::additive()
{
    ExprP l = term();
    while (check(TK_PLUS) || check(TK_MINUS)) {
        const Token& t = next();
        l = binary(std::move(l), t.kind, term(), t.line);
    }
    return l;
}

ExprP Parser::term()
{
    ExprP l = unary();
    while (check(TK_STAR) || check(TK_SLASH) || check(TK_PERCENT)) {
        const Token& t = next();
        l = binary(std::move(l), t.kind, unary(), t.line);
    }
    return l;
}

ExprP Parser::unary()
{
    if (check(TK_MINUS)) {
        int line = next().line;
        ExprP inner = unary();
        if (inner->kind == E_NUM) {
            inner->num = -inner->num;
            return inner;
        }
        ExprP e = make(E_NEG, line);
        e->a = std::move(inner);
        return e;
    }
    return postfix();
}

ExprP Parser::postfix()
{
    ExprP e = primary();
    for (;;) {
        if (check(TK_LPAREN)) {
            int line = next().line;
            ExprP c = make(E_CALL, line);
            c->a = std::move(e);
            if (!check(TK_RPAREN)) {
                do {
                    if (check(TK_RPAREN))
                        break;
                    c->items.push_back(expression());
                } while (match(TK_COMMA));
            }
            expect(TK_RPAREN, "')' after the arguments");
            e = std::move(c);
        } else if (check(TK_LBRACKET)) {
            int line = next().line;
            ExprP ix = make(E_INDEX, line);
            ix->a = std::move(e);
            ix->b = expression();
            expect(TK_RBRACKET, "']'");
            e = std::move(ix);
        } else if (check(TK_DOT)) {
            int line = next().line;
            ExprP f = make(E_FIELD, line);
            f->a = std::move(e);
            f->name = expect(TK_IDENT, "a field name after '.'").text;
            e = std::move(f);
        } else {
            return e;
        }
    }
}

ExprP Parser::primary()
{
    const Token& t = peek();
    int line = t.line;
    switch (t.kind) {
    case TK_NUM: {
        ExprP e = make(E_NUM, line);
        e->num = next().num;
        return e;
    }
    case TK_STR: {
        ExprP e = make(E_STR, line);
        e->konst = Value::string(next().text);
        return e;
    }
    case TK_TRUE:
    case TK_FALSE: {
        ExprP e = make(E_BOOL, line);
        e->flag = next().kind == TK_TRUE;
        return e;
    }
    case TK_NIL:
        next();
        return make(E_NIL, line);
    case TK_IDENT:
        return variable(next().text, line);
    case TK_LPAREN: {
        next();
        ExprP e = expression();
        expect(TK_RPAREN, "')'");
        return e;
    }
    case TK_LBRACKET: {
        next();
        ExprP e = make(E_LIST, line);
        while (!check(TK_RBRACKET)) {
            e->items.push_back(expression());
            if (!match(TK_COMMA))
                break;
        }
        expect(TK_RBRACKET, "']' to close the list");
        return e;
    }
    case TK_LBRACE: {
        next();
        ExprP e = make(E_TABLE, line);
        while (!check(TK_RBRACE)) {
            const Token& k = next();
            if (k.kind != TK_IDENT && k.kind != TK_STR)
                fail("table keys are names or strings");
            e->keys.push_back(k.text);
            expect(TK_COLON, "':' after the key");
            e->items.push_back(expression());
            if (!match(TK_COMMA))
                break;
        }
        expect(TK_RBRACE, "'}' to close the table");
        return e;
    }
    case TK_FN: {
        next();
        ExprP e = make(E_FUNC, line);
        e->fn = function_body("<anonymous>", line);
        return e;
    }
    default:
        fail("expected an expression");
    }
}

std::shared_ptr<FuncProto> Parser::function_body(const std::string& name, int line)
{
    auto proto = std::make_shared<FuncProto>();
    proto->name = name;
    proto->source = source;
    proto->line = line;
    funcs.push_back({ proto.get(), { Scope() }, 0 });

    expect(TK_LPAREN, "'(' after fn");
    if (!check(TK_RPAREN)) {
        do {
            declare(expect(TK_IDENT, "a parameter name").text);
            proto->nparams++;
        } while (match(TK_COMMA));
    }
    expect(TK_RPAREN, "')' after the parameters");
    expect(TK_LBRACE, "'{' to start the function body");
    while (!check(TK_RBRACE)) {
        if (check(TK_EOF))
            fail("missing '}' at the end of fn " + name, line);
        proto->body.push_back(statement());
    }
    next();

    funcs.pop_back();
    return proto;
}

