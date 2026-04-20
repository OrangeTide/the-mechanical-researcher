/* parse.c - Recursive-descent parser for TinC.
 *
 * Grammar (informal):
 *   program    = { top-decl } .
 *   top-decl   = [ type ] ident ( '(' ... ')' func-tail | array? '=' init ';' | array? ';' )
 *   func-tail  = '(' [ ident-list ] ')' { param-decl } compound
 *   param-decl = type [ '*' ] ident ';'
 *   compound   = '{' { local-decl } { stmt } '}'
 *   local-decl = type [ '*' ] ident [ '[' num ']' ] { ',' ... } ';'
 *   stmt       = compound | if | while | 'break' ';' | 'continue' ';'
 *              | 'return' [ expr ] ';' | [ expr ] ';'
 *   expr       = assign ; precedence climb for binary operators.
 *
 * "int" is the default type when none is written; this covers K&R
 * implicit-int function headers ("fib(n) int n; {...}").
 */

#include <stdlib.h>
#include <string.h>

#include "tinc.h"

/* ---------- helpers ---------- */

static struct token cur;

static void
advance(void)
{
    cur = lex_next();
}

static int
accept(int k)
{
    if (cur.kind == k) {
        advance();
        return 1;
    }
    return 0;
}

static void
expect(int k, const char *what)
{
    if (cur.kind != k)
        die("parse:%d: expected %s", cur.line, what);
    advance();
}

static struct node *
new_node(int kind, int line)
{
    struct node *n;

    n = xcalloc(1, sizeof(*n));
    n->kind = kind;
    n->line = line;
    n->arr_size = 0;
    n->base_type = T_INT;
    return n;
}

/* ---------- expressions (precedence climb) ---------- */

static struct node *parse_expr(void);
static struct node *parse_assign(void);

static struct node *
parse_primary(void)
{
    struct node *n;
    struct token t;

    t = cur;
    if (t.kind == T_NUMBER) {
        advance();
        n = new_node(N_NUM, t.line);
        n->ival = t.nval;
        return n;
    }
    if (t.kind == T_CHARLIT) {
        advance();
        n = new_node(N_CHARLIT, t.line);
        n->ival = t.nval;
        return n;
    }
    if (t.kind == T_STRING) {
        advance();
        n = new_node(N_STR, t.line);
        n->sval = t.sval;
        n->slen = t.slen;
        return n;
    }
    if (t.kind == T_IDENT) {
        advance();
        n = new_node(N_NAME, t.line);
        n->name = t.sval;
        return n;
    }
    if (t.kind == T_LPAREN) {
        advance();
        n = parse_expr();
        expect(T_RPAREN, ")");
        return n;
    }
    die("parse:%d: unexpected token in expression", t.line);
    return NULL;
}

static struct node *
parse_postfix(void)
{
    struct node *n, *idx, *args, **tail, *arg;
    int line;

    n = parse_primary();
    for (;;) {
        line = cur.line;
        if (cur.kind == T_LBRACK) {
            advance();
            idx = parse_expr();
            expect(T_RBRACK, "]");
            {
                struct node *p = new_node(N_INDEX, line);
                p->a = n;
                p->b = idx;
                n = p;
            }
        } else if (cur.kind == T_LPAREN) {
            advance();
            args = NULL;
            tail = &args;
            if (cur.kind != T_RPAREN) {
                for (;;) {
                    arg = parse_assign();
                    *tail = arg;
                    tail = &arg->next;
                    if (!accept(T_COMMA))
                        break;
                }
            }
            expect(T_RPAREN, ")");
            {
                struct node *p = new_node(N_CALL, line);
                p->a = n;
                p->b = args;
                n = p;
            }
        } else {
            break;
        }
    }
    return n;
}

static struct node *
parse_unary(void)
{
    struct node *n, *u;
    int op, line;

    if (cur.kind == T_MINUS || cur.kind == T_BANG ||
        cur.kind == T_TILDE || cur.kind == T_STAR ||
        cur.kind == T_AMP   || cur.kind == T_PLUS) {
        op = cur.kind;
        line = cur.line;
        advance();
        n = parse_unary();
        if (op == T_PLUS)
            return n;
        u = new_node(N_UNOP, line);
        u->op = op;
        u->a = n;
        return u;
    }
    return parse_postfix();
}

/* Precedence table for binary operators. Higher = tighter. */
static int
binop_prec(int k)
{
    switch (k) {
    case T_STAR: case T_SLASH: case T_PERCENT:           return 11;
    case T_PLUS: case T_MINUS:                           return 10;
    case T_SHL:  case T_SHR:                             return  9;
    case T_LT: case T_LE: case T_GT: case T_GE:          return  8;
    case T_EQ: case T_NE:                                return  7;
    case T_AMP:                                          return  6;
    case T_CARET:                                        return  5;
    case T_PIPE:                                         return  4;
    case T_ANDAND:                                       return  3;
    case T_OROR:                                         return  2;
    default:                                             return -1;
    }
}

static struct node *
parse_binop(int min_prec)
{
    struct node *lhs, *rhs, *b;
    int prec, op, line;

    lhs = parse_unary();
    for (;;) {
        prec = binop_prec(cur.kind);
        if (prec < min_prec)
            return lhs;
        op = cur.kind;
        line = cur.line;
        advance();
        rhs = parse_binop(prec + 1);
        b = new_node(N_BINOP, line);
        b->op = op;
        b->a = lhs;
        b->b = rhs;
        lhs = b;
    }
}

static struct node *
parse_assign(void)
{
    struct node *lhs, *rhs, *a;
    int line;

    lhs = parse_binop(2);
    if (cur.kind == T_ASSIGN) {
        line = cur.line;
        advance();
        rhs = parse_assign();
        a = new_node(N_ASSIGN, line);
        a->a = lhs;
        a->b = rhs;
        return a;
    }
    return lhs;
}

static struct node *
parse_expr(void)
{
    return parse_assign();
}

/* ---------- types and declarations ---------- */

static int
is_type_start(int k)
{
    return k == T_INT || k == T_CHAR;
}

static int
parse_type(void)
{
    int t;

    t = T_INT;
    if (cur.kind == T_INT || cur.kind == T_CHAR) {
        t = cur.kind;
        advance();
    }
    return t;
}

/* ---------- statements ---------- */

static struct node *parse_stmt(void);

static struct node *
parse_local_decls(void)
{
    struct node *head, **tail, *n;
    int base;

    head = NULL;
    tail = &head;
    while (is_type_start(cur.kind)) {
        base = parse_type();
        for (;;) {
            int is_ptr = 0;
            int line = cur.line;
            int arr = 0;
            if (accept(T_STAR))
                is_ptr = 1;
            if (cur.kind != T_IDENT)
                die("parse:%d: expected identifier in declaration", cur.line);
            n = new_node(N_GLOBAL, line);     /* reused: treat as local decl */
            n->name = cur.sval;
            n->base_type = base;
            n->is_ptr = is_ptr;
            advance();
            if (accept(T_LBRACK)) {
                if (cur.kind == T_NUMBER) {
                    arr = (int)cur.nval;
                    advance();
                } else {
                    arr = -1;
                }
                expect(T_RBRACK, "]");
            }
            n->arr_size = arr;
            *tail = n;
            tail = &n->next;
            if (!accept(T_COMMA))
                break;
        }
        expect(T_SEMI, ";");
    }
    return head;
}

static struct node *
parse_compound(void)
{
    struct node *n, *decls, *stmts, **tail, *s;
    int line;

    line = cur.line;
    expect(T_LBRACE, "{");
    decls = parse_local_decls();
    stmts = NULL;
    tail = &stmts;
    while (cur.kind != T_RBRACE && cur.kind != T_EOF) {
        s = parse_stmt();
        *tail = s;
        tail = &s->next;
    }
    expect(T_RBRACE, "}");
    n = new_node(N_BLOCK, line);
    n->a = decls;
    n->b = stmts;
    return n;
}

static struct node *
parse_stmt(void)
{
    struct node *n, *e;
    int line;

    line = cur.line;
    if (cur.kind == T_LBRACE)
        return parse_compound();

    if (cur.kind == T_IF) {
        advance();
        expect(T_LPAREN, "(");
        n = new_node(N_IF, line);
        n->a = parse_expr();
        expect(T_RPAREN, ")");
        n->b = parse_stmt();
        if (accept(T_ELSE))
            n->c = parse_stmt();
        return n;
    }
    if (cur.kind == T_WHILE) {
        advance();
        expect(T_LPAREN, "(");
        n = new_node(N_WHILE, line);
        n->a = parse_expr();
        expect(T_RPAREN, ")");
        n->b = parse_stmt();
        return n;
    }
    if (cur.kind == T_BREAK) {
        advance();
        expect(T_SEMI, ";");
        return new_node(N_BREAK, line);
    }
    if (cur.kind == T_CONTINUE) {
        advance();
        expect(T_SEMI, ";");
        return new_node(N_CONTINUE, line);
    }
    if (cur.kind == T_RETURN) {
        advance();
        n = new_node(N_RETURN, line);
        if (cur.kind != T_SEMI)
            n->a = parse_expr();
        expect(T_SEMI, ";");
        return n;
    }
    if (accept(T_SEMI))
        return new_node(N_EXPR_STMT, line);

    e = parse_expr();
    expect(T_SEMI, ";");
    n = new_node(N_EXPR_STMT, line);
    n->a = e;
    return n;
}

/* ---------- top level ---------- */

static struct node *
parse_initializer(int *is_string, int *string_len)
{
    struct node *head, **tail, *e;

    *is_string = 0;
    *string_len = 0;

    if (cur.kind == T_STRING) {
        e = parse_primary();
        *is_string = 1;
        *string_len = e->slen;
        return e;
    }
    if (accept(T_LBRACE)) {
        head = NULL;
        tail = &head;
        if (cur.kind != T_RBRACE) {
            for (;;) {
                e = parse_assign();
                *tail = e;
                tail = &e->next;
                if (!accept(T_COMMA))
                    break;
                if (cur.kind == T_RBRACE)
                    break;
            }
        }
        expect(T_RBRACE, "}");
        {
            struct node *n = new_node(N_INIT_LIST, cur.line);
            n->a = head;
            return n;
        }
    }
    return parse_assign();
}

static struct node *
parse_function(int ret_type, char *name, int line)
{
    struct node *fn, *params, **ptail, *p;
    int base;

    (void)ret_type;
    fn = new_node(N_FUNC, line);
    fn->name = name;

    /* Parameter-name list. */
    expect(T_LPAREN, "(");
    params = NULL;
    ptail = &params;
    if (cur.kind != T_RPAREN) {
        for (;;) {
            if (cur.kind != T_IDENT)
                die("parse:%d: expected parameter name", cur.line);
            p = new_node(N_GLOBAL, cur.line);
            p->name = cur.sval;
            p->base_type = T_INT;
            p->is_ptr = 0;
            advance();
            *ptail = p;
            ptail = &p->next;
            if (!accept(T_COMMA))
                break;
        }
    }
    expect(T_RPAREN, ")");

    /* Typed parameter declarations. One per line, semi-terminated. */
    while (is_type_start(cur.kind)) {
        base = parse_type();
        for (;;) {
            int is_ptr = 0;
            if (accept(T_STAR))
                is_ptr = 1;
            if (cur.kind != T_IDENT)
                die("parse:%d: expected parameter name", cur.line);
            /* find the param by name and set its type */
            for (p = params; p; p = p->next) {
                if (strcmp(p->name, cur.sval) == 0) {
                    p->base_type = base;
                    p->is_ptr = is_ptr;
                    break;
                }
            }
            if (!p)
                die("parse:%d: %s is not a parameter", cur.line, cur.sval);
            advance();
            if (!accept(T_COMMA))
                break;
        }
        expect(T_SEMI, ";");
    }

    fn->a = params;
    fn->b = parse_compound();
    return fn;
}

static struct node *
parse_top_decl(void)
{
    struct node *n;
    char *name;
    int base, is_ptr, arr, line;

    line = cur.line;
    base = T_INT;
    if (is_type_start(cur.kind))
        base = parse_type();

    is_ptr = 0;
    if (accept(T_STAR))
        is_ptr = 1;

    if (cur.kind != T_IDENT)
        die("parse:%d: expected top-level declarator", cur.line);
    name = cur.sval;
    advance();

    if (cur.kind == T_LPAREN)
        return parse_function(base, name, line);

    /* Global variable. */
    arr = 0;
    if (accept(T_LBRACK)) {
        if (cur.kind == T_NUMBER) {
            arr = (int)cur.nval;
            advance();
        } else {
            arr = -1;
        }
        expect(T_RBRACK, "]");
    }

    n = new_node(N_GLOBAL, line);
    n->name = name;
    n->base_type = base;
    n->is_ptr = is_ptr;
    n->arr_size = arr;

    if (accept(T_ASSIGN)) {
        int is_string = 0;
        int string_len = 0;
        n->a = parse_initializer(&is_string, &string_len);
        if (arr == -1) {
            if (is_string)
                n->arr_size = string_len + 1;
            else if (n->a && n->a->kind == N_INIT_LIST) {
                int cnt = 0;
                struct node *e;
                for (e = n->a->a; e; e = e->next)
                    cnt++;
                n->arr_size = cnt;
            }
        }
    }
    expect(T_SEMI, ";");
    return n;
}

/* ---------- entry point ---------- */

struct node *
parse_program(void)
{
    struct node *prog, *head, **tail, *d;

    advance();
    head = NULL;
    tail = &head;
    while (cur.kind != T_EOF) {
        d = parse_top_decl();
        *tail = d;
        tail = &d->next;
    }
    prog = new_node(N_PROGRAM, 1);
    prog->a = head;
    return prog;
}
