#include "script_internal.h"

StmtP Parser::make_stmt(StmtKind k, int line)
{
    StmtP s(new Stmt());
    s->kind = k;
    s->line = line;
    return s;
}

void Parser::block(std::vector<StmtP>& out)
{
    int line = expect(TK_LBRACE, "'{'").line;
    funcs.back().scopes.push_back(Scope());
    while (!check(TK_RBRACE)) {
        if (check(TK_EOF))
            fail("missing '}' for the block opened here", line);
        out.push_back(statement());
    }
    next();
    funcs.back().scopes.pop_back();
}

ExprP Parser::local_target(int slot, int line)
{
    ExprP e = make(E_LOCAL, line);
    e->slot = slot;
    return e;
}

StmtP Parser::if_rest(int line)
{
    StmtP s = make_stmt(S_IF, line);
    s->value = expression();
    block(s->body);
    if (check(TK_ELIF)) {
        int l = next().line;
        s->else_body.push_back(if_rest(l));
    } else if (match(TK_ELSE)) {
        if (check(TK_IF)) {
            int l = next().line;
            s->else_body.push_back(if_rest(l));
        } else {
            block(s->else_body);
        }
    }
    return s;
}

StmtP Parser::statement()
{
    StmtP s = statement_inner();
    match(TK_SEMI);
    return s;
}

StmtP Parser::statement_inner()
{
    const Token& t = peek();
    int line = t.line;

    switch (t.kind) {
    case TK_LET: {
        next();
        std::string name = expect(TK_IDENT, "a name after let").text;
        ExprP value;
        if (match(TK_ASSIGN))
            value = expression();
        else
            value = make(E_NIL, line);
        // declared after the value, so "let x = x + 1" reads the outer x
        int slot = declare(name);
        StmtP s = make_stmt(S_SET, line);
        s->target = local_target(slot, line);
        s->value = std::move(value);
        return s;
    }
    case TK_FN:
        if (peek(1).kind == TK_IDENT) {
            next();
            std::string name = next().text;
            int slot = declare(name);   // before the body, so it can call itself
            StmtP s = make_stmt(S_SET, line);
            s->target = local_target(slot, line);
            s->value = make(E_FUNC, line);
            s->value->fn = function_body(name, line);
            return s;
        }
        break;
    case TK_IF:
        next();
        return if_rest(line);
    case TK_WHILE: {
        next();
        StmtP s = make_stmt(S_WHILE, line);
        s->value = expression();
        funcs.back().loops++;
        block(s->body);
        funcs.back().loops--;
        return s;
    }
    case TK_FOR: {
        next();
        funcs.back().scopes.push_back(Scope());
        std::string a = expect(TK_IDENT, "a loop variable").text;
        std::string b;
        if (match(TK_COMMA))
            b = expect(TK_IDENT, "a second loop variable").text;
        expect(TK_IN, "'in'");
        ExprP from = expression();
        StmtP s;
        if (match(TK_DOTDOT)) {
            if (!b.empty())
                fail("a range only gives one value");
            s = make_stmt(S_FOR_RANGE, line);
            s->value = std::move(from);
            s->extra = expression();
        } else {
            s = make_stmt(S_FOR_IN, line);
            s->value = std::move(from);
        }
        s->slot_a = declare(a);
        if (!b.empty())
            s->slot_b = declare(b);
        funcs.back().loops++;
        block(s->body);
        funcs.back().loops--;
        funcs.back().scopes.pop_back();
        return s;
    }
    case TK_RETURN: {
        next();
        StmtP s = make_stmt(S_RETURN, line);
        if (!check(TK_RBRACE) && !check(TK_SEMI) && !check(TK_EOF))
            s->value = expression();
        return s;
    }
    case TK_BREAK:
    case TK_CONTINUE: {
        next();
        if (funcs.back().loops == 0)
            fail(t.kind == TK_BREAK ? "break outside a loop" : "continue outside a loop");
        return make_stmt(t.kind == TK_BREAK ? S_BREAK : S_CONTINUE, line);
    }
    case TK_LBRACE: {
        StmtP s = make_stmt(S_BLOCK, line);
        block(s->body);
        return s;
    }
    default:
        break;
    }

    ExprP e = expression();
    TokKind k = peek().kind;
    if (k == TK_ASSIGN || k == TK_PLUS_EQ || k == TK_MINUS_EQ || k == TK_STAR_EQ || k == TK_SLASH_EQ) {
        next();
        if (e->kind == E_GLOBAL)
            fail("'" + e->name + "' isn't declared, use let", line);
        if (e->kind != E_LOCAL && e->kind != E_INDEX && e->kind != E_FIELD)
            fail("can't assign to that", line);
        StmtP s = make_stmt(S_SET, line);
        static const TokKind ops[] = { TK_EOF, TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH };
        s->op = k == TK_ASSIGN ? 0 : ops[k - TK_PLUS_EQ + 1];
        s->target = std::move(e);
        s->value = expression();
        return s;
    }
    StmtP s = make_stmt(S_EXPR, line);
    s->value = std::move(e);
    return s;
}

std::shared_ptr<FuncProto> Parser::compile(const std::string& src, std::unordered_map<std::string, int>* exports)
{
    script_lex(src, source, toks);

    auto chunk = std::make_shared<FuncProto>();
    chunk->name = "<top>";
    chunk->source = source;
    funcs.push_back({ chunk.get(), { Scope() }, 0 });

    // hoist top level fn and let names, so functions can use each other
    // and module variables no matter the order they're written in
    int braces = 0;
    for (size_t i = 0; i + 1 < toks.size(); i++) {
        TokKind k = toks[i].kind;
        if (k == TK_LBRACE) braces++;
        else if (k == TK_RBRACE) braces--;
        else if (braces == 0 && (k == TK_FN || k == TK_LET) && toks[i + 1].kind == TK_IDENT)
            declare(toks[i + 1].text);
    }

    while (!check(TK_EOF))
        chunk->body.push_back(statement());

    if (exports)
        for (auto& p : funcs[0].scopes[0].names)
            (*exports)[p.first] = p.second;
    funcs.pop_back();
    return chunk;
}

bool script_check(const std::string& src, const std::string& name, std::string* err)
{
    try {
        Parser ps;
        ps.vm = 0;
        ps.source = name;
        ps.compile(src, 0);
        return true;
    } catch (ScriptError& e) {
        *err = e.msg;
        return false;
    }
}

