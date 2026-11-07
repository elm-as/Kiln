#pragma once
#include "script.h"

enum TokKind {
    TK_EOF, TK_NUM, TK_STR, TK_IDENT,
    TK_LET, TK_FN, TK_IF, TK_ELIF, TK_ELSE, TK_WHILE, TK_FOR, TK_IN,
    TK_RETURN, TK_BREAK, TK_CONTINUE, TK_TRUE, TK_FALSE, TK_NIL,
    TK_AND, TK_OR, TK_NOT,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_DOT, TK_DOTDOT, TK_COLON, TK_SEMI,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_ASSIGN, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_PLUS_EQ, TK_MINUS_EQ, TK_STAR_EQ, TK_SLASH_EQ,
};

struct Token {
    TokKind kind;
    int line;
    double num;
    std::string text;
};

void script_lex(const std::string& src, const std::string& name, std::vector<Token>& out);

enum ExprKind {
    E_NUM, E_STR, E_BOOL, E_NIL, E_LOCAL, E_GLOBAL, E_NEG, E_NOT,
    E_BIN, E_AND, E_OR, E_CALL, E_INDEX, E_FIELD, E_LIST, E_TABLE, E_FUNC,
};

struct Expr {
    ExprKind kind;
    int line = 0;
    double num = 0;
    bool flag = false;
    std::string name;
    Value konst;
    int op = 0;
    int depth = 0, slot = 0;
    std::unique_ptr<Expr> a, b;
    std::vector<std::unique_ptr<Expr>> items;
    std::vector<std::string> keys;
    std::shared_ptr<FuncProto> fn;
};

enum StmtKind { S_EXPR, S_SET, S_IF, S_WHILE, S_FOR_RANGE, S_FOR_IN, S_RETURN, S_BREAK, S_CONTINUE, S_BLOCK };

struct Stmt {
    StmtKind kind;
    int line = 0;
    int op = 0;
    int slot_a = -1, slot_b = -1;
    std::unique_ptr<Expr> target, value, extra;
    std::vector<std::unique_ptr<Stmt>> body, else_body;
};

struct FuncProto {
    std::string name;
    std::string source;
    int line = 0;
    int nparams = 0;
    int nslots = 0;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct Frame {
    std::vector<Value> slots;
    std::shared_ptr<Frame> parent;
};

typedef std::unique_ptr<Expr> ExprP;
typedef std::unique_ptr<Stmt> StmtP;
typedef std::shared_ptr<Frame> FrameP;

struct Scope {
    std::vector<std::pair<std::string, int>> names;
};

struct FuncState {
    FuncProto* proto;
    std::vector<Scope> scopes;
    int loops;
};

struct Parser {
    VM* vm;
    std::string source;
    std::vector<Token> toks;
    size_t pos = 0;
    std::vector<FuncState> funcs;

    [[noreturn]] void fail(const std::string& msg, int line = -1);
    const Token& peek(int ahead = 0) const;
    bool check(TokKind k) const;
    const Token& next();
    bool match(TokKind k);
    const Token& expect(TokKind k, const char* what);
    ExprP make(ExprKind k, int line);
    int global(const std::string& name);
    int declare(const std::string& name);
    bool resolve(const std::string& name, int* depth, int* slot);
    ExprP variable(const std::string& name, int line);

    ExprP expression();
    ExprP or_expr();
    ExprP and_expr();
    ExprP not_expr();
    ExprP binary(ExprP l, int op, ExprP r, int line);
    ExprP comparison();
    ExprP additive();
    ExprP term();
    ExprP unary();
    ExprP postfix();
    ExprP primary();
    std::shared_ptr<FuncProto> function_body(const std::string& name, int line);

    StmtP make_stmt(StmtKind k, int line);
    void block(std::vector<StmtP>& out);
    ExprP local_target(int slot, int line);
    StmtP if_rest(int line);
    StmtP statement();
    StmtP statement_inner();
    std::shared_ptr<FuncProto> compile(const std::string& src, std::unordered_map<std::string, int>* exports);
};

enum Flow { FLOW_NORMAL, FLOW_RETURN, FLOW_BREAK, FLOW_CONTINUE };

Flow script_exec_block(VM& vm, const std::vector<StmtP>& body, const FrameP& f);

