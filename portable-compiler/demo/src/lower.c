/* lower.c - AST -> IR lowering.
 *
 * Strategy:
 *   - Every expression evaluates to a fresh 32-bit temp.
 *   - Mutable locals live in stack slots (no phi nodes). Access is
 *     via IR_LDL/IR_STL, or IR_ADL when the address is taken
 *     (arrays, subscripts).
 *   - Globals and string literals live in the program's global list;
 *     references emit IR_LEA.
 *   - break/continue use a stack of (break_label, cont_label) pairs
 *     pushed by the enclosing while.
 *   - && and || lower to control flow, materialising 0/1 in a temp.
 *   - Call args are evaluated left-to-right into temps, then emitted
 *     as IR_ARGs followed by IR_CALL.
 */

#include <stdlib.h>
#include <string.h>

#include "tinc.h"

/* ---------- global symbol table ---------- */

struct gsym {
    char *name;
    struct ir_global *g;        /* NULL for forward-declared functions */
    int is_func;
    int base_type;              /* T_INT or T_CHAR */
    int is_array;
    int is_ptr;
};

static struct gsym *gsyms;
static int ngsyms;
static int gsym_cap;

static void
add_gsym(struct gsym s)
{
    if (ngsyms == gsym_cap) {
        gsym_cap = gsym_cap ? gsym_cap * 2 : 16;
        gsyms = realloc(gsyms, gsym_cap * sizeof(*gsyms));
        if (!gsyms) die("oom");
    }
    gsyms[ngsyms++] = s;
}

static struct gsym *
find_gsym(const char *name)
{
    int i;
    for (i = 0; i < ngsyms; i++)
        if (strcmp(gsyms[i].name, name) == 0)
            return &gsyms[i];
    return NULL;
}

/* ---------- per-function locals ---------- */

struct lsym {
    char *name;
    int slot;
    int base_type;
    int is_array;
    int is_ptr;
    int arr_size;              /* element count for arrays */
};

static struct lsym *lsyms;
static int nlsyms;
static int lsym_cap;

static int *slot_sizes;
static int nslots;
static int slot_cap;

static int
alloc_slot(int bytes)
{
    if (nslots == slot_cap) {
        slot_cap = slot_cap ? slot_cap * 2 : 16;
        slot_sizes = realloc(slot_sizes, slot_cap * sizeof(*slot_sizes));
        if (!slot_sizes) die("oom");
    }
    slot_sizes[nslots] = bytes;
    return nslots++;
}

static void
add_lsym(struct lsym s)
{
    if (nlsyms == lsym_cap) {
        lsym_cap = lsym_cap ? lsym_cap * 2 : 16;
        lsyms = realloc(lsyms, lsym_cap * sizeof(*lsyms));
        if (!lsyms) die("oom");
    }
    lsyms[nlsyms++] = s;
}

static struct lsym *
find_lsym(const char *name)
{
    int i;
    for (i = nlsyms - 1; i >= 0; i--)
        if (strcmp(lsyms[i].name, name) == 0)
            return &lsyms[i];
    return NULL;
}

/* ---------- IR emission ---------- */

static struct ir_func *cur_fn;
static int next_temp;
static int next_label;

static int
new_temp(void)
{
    return next_temp++;
}

static int
new_label(void)
{
    return next_label++;
}

static struct ir_insn *
emit(int op)
{
    struct ir_insn *i;

    i = xcalloc(1, sizeof(*i));
    i->op = op;
    i->dst = -1;
    i->a = -1;
    i->b = -1;
    i->slot = -1;
    i->label = -1;
    if (!cur_fn->head)
        cur_fn->head = i;
    else
        cur_fn->tail->next = i;
    cur_fn->tail = i;
    return i;
}

/* ---------- break/continue stack ---------- */

struct loop { int brk; int cont; };
static struct loop loops[64];
static int nloops;

/* ---------- expression type info ---------- */

struct val {
    int temp;                   /* temp holding the value */
    int base_type;              /* T_INT / T_CHAR for value width cues */
    int is_ptr;                 /* pointer-typed? */
    int is_array;               /* array-typed? (for decay) */
};

static struct val lower_expr(struct node *n);
static void lower_stmt(struct node *n);
static void lower_cond(struct node *n, int ltrue, int lfalse);
static int elem_width(const struct val *v);

static struct val
mkval(int temp, int bt, int is_ptr)
{
    struct val v;
    v.temp = temp;
    v.base_type = bt;
    v.is_ptr = is_ptr;
    v.is_array = 0;
    return v;
}

static int
elem_width(const struct val *v)
{
    /* Pointer-to-char or char[] dereferences as byte. */
    if ((v->is_ptr || v->is_array) && v->base_type == T_CHAR)
        return 1;
    return 4;
}

/* ---------- address-of an lvalue ---------- */

struct addr {
    int temp;                   /* temp holding the address */
    int base_type;              /* element type at that address */
};

static struct addr
lower_addr(struct node *n)
{
    struct addr a;
    struct lsym *ls;
    struct gsym *gs;
    struct ir_insn *ins;
    struct val av, iv;
    int w, tmul, tadd;

    if (n->kind == N_NAME) {
        ls = find_lsym(n->name);
        if (ls) {
            ins = emit(IR_ADL);
            ins->dst = new_temp();
            ins->slot = ls->slot;
            a.temp = ins->dst;
            a.base_type = ls->base_type;
            return a;
        }
        gs = find_gsym(n->name);
        if (!gs)
            die("lower:%d: undefined '%s'", n->line, n->name);
        ins = emit(IR_LEA);
        ins->dst = new_temp();
        ins->sym = xstrdup(n->name);
        a.temp = ins->dst;
        a.base_type = gs->base_type;
        return a;
    }
    if (n->kind == N_INDEX) {
        av = lower_expr(n->a);
        iv = lower_expr(n->b);
        w = elem_width(&av);
        if (w != 1) {
            struct ir_insn *li;
            tmul = new_temp();
            li = emit(IR_LIC);
            li->dst = tmul;
            li->imm = w;
            ins = emit(IR_MUL);
            ins->dst = new_temp();
            ins->a = iv.temp;
            ins->b = tmul;
            iv.temp = ins->dst;
        }
        ins = emit(IR_ADD);
        ins->dst = new_temp();
        ins->a = av.temp;
        ins->b = iv.temp;
        tadd = ins->dst;
        a.temp = tadd;
        a.base_type = av.base_type;
        return a;
    }
    if (n->kind == N_UNOP && n->op == T_STAR) {
        av = lower_expr(n->a);
        a.temp = av.temp;
        a.base_type = av.base_type;
        return a;
    }
    die("lower:%d: not an lvalue", n->line);
    a.temp = -1;
    a.base_type = T_INT;
    return a;
}

/* ---------- expression lowering ---------- */

static int
bin_op_for_tok(int op)
{
    switch (op) {
    case T_PLUS:    return IR_ADD;
    case T_MINUS:   return IR_SUB;
    case T_STAR:    return IR_MUL;
    case T_SLASH:   return IR_DIVS;
    case T_PERCENT: return IR_MODS;
    case T_AMP:     return IR_AND;
    case T_PIPE:    return IR_OR;
    case T_CARET:   return IR_XOR;
    case T_SHL:     return IR_SHL;
    case T_SHR:     return IR_SHRS;
    case T_EQ:      return IR_CMPEQ;
    case T_NE:      return IR_CMPNE;
    case T_LT:      return IR_CMPLTS;
    case T_LE:      return IR_CMPLES;
    case T_GT:      return IR_CMPGTS;
    case T_GE:      return IR_CMPGES;
    default:        return -1;
    }
}

static int
lower_const(long v)
{
    struct ir_insn *ins = emit(IR_LIC);
    ins->dst = new_temp();
    ins->imm = v;
    return ins->dst;
}

static void
emit_label(int lab)
{
    struct ir_insn *ins = emit(IR_LABEL);
    ins->label = lab;
}

static void
emit_jmp(int lab)
{
    struct ir_insn *ins = emit(IR_JMP);
    ins->label = lab;
}

static void
emit_bnz(int t, int lab)
{
    struct ir_insn *ins = emit(IR_BNZ);
    ins->a = t;
    ins->label = lab;
}

static struct val
lower_shortcircuit(struct node *n)
{
    int result, t, ltrue, lfalse, lend;
    struct ir_insn *ins;

    result = alloc_slot(4);  /* dummy: value goes in a temp, not a slot */
    (void)result;

    ltrue = new_label();
    lfalse = new_label();
    lend = new_label();

    lower_cond(n, ltrue, lfalse);

    emit_label(ltrue);
    t = new_temp();
    ins = emit(IR_LIC);
    ins->dst = t;
    ins->imm = 1;
    emit_jmp(lend);

    emit_label(lfalse);
    ins = emit(IR_LIC);
    ins->dst = t;
    ins->imm = 0;

    emit_label(lend);
    /* NB: both branches write 't' — our allocator treats this as two
     * defs merged by regalloc/backend via same temp id. */
    return mkval(t, T_INT, 0);
}

static struct val
lower_expr(struct node *n)
{
    struct ir_insn *ins;
    struct val l, r, v;
    struct addr ad;
    struct lsym *ls;
    struct gsym *gs;
    int op, t;

    if (!n)
        die("lower: null expression");

    switch (n->kind) {
    case N_NUM:
    case N_CHARLIT:
        return mkval(lower_const(n->ival), T_INT, 0);

    case N_STR: {
        /* Add an anonymous global string; emit LEA. */
        static int strctr;
        struct ir_global *g;
        char namebuf[32];
        snprintf(namebuf, sizeof(namebuf), "__str_%d", strctr++);
        g = xcalloc(1, sizeof(*g));
        g->name = xstrdup(namebuf);
        g->base_type = T_CHAR;
        g->arr_size = n->slen + 1;
        g->init_string = xmalloc(n->slen + 1);
        memcpy(g->init_string, n->sval, n->slen);
        g->init_string[n->slen] = '\0';
        g->init_strlen = n->slen + 1;
        /* Prepend to global list via a back-channel pointer. */
        extern struct ir_program *cur_prog;
        g->next = cur_prog->globals;
        cur_prog->globals = g;
        {
            struct gsym s;
            s.name = g->name;
            s.g = g;
            s.is_func = 0;
            s.base_type = T_CHAR;
            s.is_array = 1;
            s.is_ptr = 0;
            add_gsym(s);
        }
        ins = emit(IR_LEA);
        ins->dst = new_temp();
        ins->sym = xstrdup(namebuf);
        v = mkval(ins->dst, T_CHAR, 1);
        return v;
    }

    case N_NAME:
        ls = find_lsym(n->name);
        if (ls) {
            if (ls->is_array) {
                ins = emit(IR_ADL);
                ins->dst = new_temp();
                ins->slot = ls->slot;
                v = mkval(ins->dst, ls->base_type, 1);
                v.is_array = 1;
                return v;
            }
            ins = emit(IR_LDL);
            ins->dst = new_temp();
            ins->slot = ls->slot;
            return mkval(ins->dst, ls->base_type, ls->is_ptr);
        }
        gs = find_gsym(n->name);
        if (!gs)
            die("lower:%d: undefined '%s'", n->line, n->name);
        if (gs->is_func) {
            ins = emit(IR_LEA);
            ins->dst = new_temp();
            ins->sym = xstrdup(n->name);
            return mkval(ins->dst, T_INT, 1);
        }
        if (gs->is_array) {
            ins = emit(IR_LEA);
            ins->dst = new_temp();
            ins->sym = xstrdup(n->name);
            v = mkval(ins->dst, gs->base_type, 1);
            v.is_array = 1;
            return v;
        }
        {
            /* global scalar: take address, load. */
            int addr;
            ins = emit(IR_LEA);
            ins->dst = new_temp();
            ins->sym = xstrdup(n->name);
            addr = ins->dst;
            ins = emit(gs->base_type == T_CHAR ? IR_LB : IR_LW);
            ins->dst = new_temp();
            ins->a = addr;
            return mkval(ins->dst, gs->base_type, gs->is_ptr);
        }

    case N_INDEX:
        ad = lower_addr(n);
        {
            struct val av = lower_expr(n->a);
            /* element type is av.base_type; element width already
             * folded into address by lower_addr. */
            ins = emit((av.base_type == T_CHAR &&
                        (av.is_ptr || av.is_array)) ? IR_LB : IR_LW);
            ins->dst = new_temp();
            ins->a = ad.temp;
            return mkval(ins->dst, av.base_type, 0);
        }

    case N_UNOP:
        if (n->op == T_STAR) {
            l = lower_expr(n->a);
            ins = emit(l.base_type == T_CHAR ? IR_LB : IR_LW);
            ins->dst = new_temp();
            ins->a = l.temp;
            return mkval(ins->dst, l.base_type, 0);
        }
        if (n->op == T_AMP) {
            ad = lower_addr(n->a);
            return mkval(ad.temp, ad.base_type, 1);
        }
        if (n->op == T_MINUS) {
            l = lower_expr(n->a);
            ins = emit(IR_NEG);
            ins->dst = new_temp();
            ins->a = l.temp;
            return mkval(ins->dst, T_INT, 0);
        }
        if (n->op == T_TILDE) {
            l = lower_expr(n->a);
            ins = emit(IR_NOT);
            ins->dst = new_temp();
            ins->a = l.temp;
            return mkval(ins->dst, T_INT, 0);
        }
        if (n->op == T_BANG) {
            l = lower_expr(n->a);
            ins = emit(IR_CMPEQ);
            ins->dst = new_temp();
            ins->a = l.temp;
            ins->b = lower_const(0);
            return mkval(ins->dst, T_INT, 0);
        }
        die("lower:%d: bad unop", n->line);
        return mkval(0, T_INT, 0);

    case N_BINOP:
        if (n->op == T_ANDAND || n->op == T_OROR)
            return lower_shortcircuit(n);
        l = lower_expr(n->a);
        r = lower_expr(n->b);
        /* Pointer + int scales int by element width. */
        if (n->op == T_PLUS || n->op == T_MINUS) {
            if ((l.is_ptr || l.is_array) && !(r.is_ptr || r.is_array)) {
                int w = elem_width(&l);
                if (w != 1) {
                    int scl = lower_const(w);
                    ins = emit(IR_MUL);
                    ins->dst = new_temp();
                    ins->a = r.temp;
                    ins->b = scl;
                    r.temp = ins->dst;
                }
            }
        }
        op = bin_op_for_tok(n->op);
        if (op < 0) die("lower:%d: bad binop", n->line);
        ins = emit(op);
        ins->dst = new_temp();
        ins->a = l.temp;
        ins->b = r.temp;
        v = mkval(ins->dst, T_INT, 0);
        if ((n->op == T_PLUS || n->op == T_MINUS) &&
            (l.is_ptr || l.is_array)) {
            v.is_ptr = 1;
            v.base_type = l.base_type;
        }
        return v;

    case N_ASSIGN:
        r = lower_expr(n->b);
        if (n->a->kind == N_NAME) {
            ls = find_lsym(n->a->name);
            if (ls && !ls->is_array) {
                ins = emit(IR_STL);
                ins->a = r.temp;
                ins->slot = ls->slot;
                return r;
            }
        }
        ad = lower_addr(n->a);
        /* Decide store width by the address's element type when it's
         * from a char-typed lvalue. */
        {
            int is_byte = (ad.base_type == T_CHAR);
            /* If lhs is a name that's a plain int scalar, use word. */
            if (n->a->kind == N_NAME) {
                ls = find_lsym(n->a->name);
                if (!ls) {
                    gs = find_gsym(n->a->name);
                    if (gs && !gs->is_array)
                        is_byte = (gs->base_type == T_CHAR);
                }
            }
            ins = emit(is_byte ? IR_SB : IR_SW);
            ins->a = ad.temp;
            ins->b = r.temp;
        }
        return r;

    case N_CALL: {
        /* Evaluate args L-to-R into temps first. */
        int args[16];
        int nargs = 0;
        struct node *a;
        for (a = n->b; a; a = a->next) {
            struct val av = lower_expr(a);
            if (nargs >= 16) die("lower: too many args");
            args[nargs++] = av.temp;
        }
        {
            int i;
            for (i = 0; i < nargs; i++) {
                ins = emit(IR_ARG);
                ins->a = args[i];
                ins->imm = i;   /* argument index */
            }
        }
        if (n->a->kind == N_NAME) {
            ins = emit(IR_CALL);
            ins->dst = new_temp();
            ins->sym = xstrdup(n->a->name);
            ins->nargs = nargs;
            t = ins->dst;
        } else {
            struct val fv = lower_expr(n->a);
            ins = emit(IR_CALLI);
            ins->dst = new_temp();
            ins->a = fv.temp;
            ins->nargs = nargs;
            t = ins->dst;
        }
        return mkval(t, T_INT, 0);
    }

    default:
        die("lower: unhandled expr kind %d", n->kind);
    }
    return mkval(0, T_INT, 0);
}

/* ---------- conditional lowering (for &&/||/if/while) ---------- */

static void
lower_cond(struct node *n, int ltrue, int lfalse)
{
    struct val v;

    if (n->kind == N_BINOP && n->op == T_ANDAND) {
        int lmid = new_label();
        lower_cond(n->a, lmid, lfalse);
        emit_label(lmid);
        lower_cond(n->b, ltrue, lfalse);
        return;
    }
    if (n->kind == N_BINOP && n->op == T_OROR) {
        int lmid = new_label();
        lower_cond(n->a, ltrue, lmid);
        emit_label(lmid);
        lower_cond(n->b, ltrue, lfalse);
        return;
    }
    if (n->kind == N_UNOP && n->op == T_BANG) {
        lower_cond(n->a, lfalse, ltrue);
        return;
    }
    v = lower_expr(n);
    emit_bnz(v.temp, ltrue);
    emit_jmp(lfalse);
}

/* ---------- statement lowering ---------- */

static void
lower_stmt(struct node *n)
{
    struct ir_insn *ins;
    struct node *s;
    int ltrue, lfalse, lend, ltop, lcont, lbrk;
    struct val v;

    if (!n) return;

    switch (n->kind) {
    case N_BLOCK:
        /* decls already bound when entering function; skip n->a */
        for (s = n->b; s; s = s->next)
            lower_stmt(s);
        return;

    case N_EXPR_STMT:
        if (n->a)
            (void)lower_expr(n->a);
        return;

    case N_IF:
        ltrue = new_label();
        lfalse = new_label();
        lend = new_label();
        lower_cond(n->a, ltrue, lfalse);
        emit_label(ltrue);
        lower_stmt(n->b);
        if (n->c) {
            emit_jmp(lend);
            emit_label(lfalse);
            lower_stmt(n->c);
            emit_label(lend);
        } else {
            emit_label(lfalse);
        }
        return;

    case N_WHILE:
        ltop = new_label();
        lbrk = new_label();
        lcont = ltop;
        emit_label(ltop);
        /* Inline constant-true optimisation for while(1). */
        if (n->a->kind == N_NUM && n->a->ival != 0) {
            /* no test */
        } else {
            int lbody = new_label();
            lower_cond(n->a, lbody, lbrk);
            emit_label(lbody);
        }
        if (nloops >= 64) die("lower: nested loops too deep");
        loops[nloops].brk = lbrk;
        loops[nloops].cont = lcont;
        nloops++;
        lower_stmt(n->b);
        nloops--;
        emit_jmp(ltop);
        emit_label(lbrk);
        return;

    case N_BREAK:
        if (nloops == 0) die("lower:%d: break outside loop", n->line);
        emit_jmp(loops[nloops - 1].brk);
        return;

    case N_CONTINUE:
        if (nloops == 0) die("lower:%d: continue outside loop", n->line);
        emit_jmp(loops[nloops - 1].cont);
        return;

    case N_RETURN:
        if (n->a) {
            v = lower_expr(n->a);
            ins = emit(IR_RETV);
            ins->a = v.temp;
        } else {
            ins = emit(IR_RET);
        }
        return;

    default:
        die("lower: unhandled stmt kind %d", n->kind);
    }
}

/* ---------- function lowering ---------- */

struct ir_program *cur_prog;

static void
collect_local_decls(struct node *block)
{
    struct node *d;

    if (!block || block->kind != N_BLOCK) return;
    for (d = block->a; d; d = d->next) {
        struct lsym ls;
        int bytes;
        ls.name = d->name;
        ls.base_type = d->base_type;
        ls.is_ptr = d->is_ptr;
        ls.is_array = (d->arr_size > 0);
        ls.arr_size = d->arr_size;
        if (ls.is_array) {
            int ew = (d->base_type == T_CHAR) ? 1 : 4;
            bytes = d->arr_size * ew;
            if (bytes < 1) bytes = 1;
        } else {
            bytes = 4;
        }
        ls.slot = alloc_slot(bytes);
        add_lsym(ls);
    }
}

static struct ir_func *
lower_function(struct node *fn_ast)
{
    struct ir_func *fn;
    struct node *p;
    struct ir_insn *ins;
    int nparams = 0;

    fn = xcalloc(1, sizeof(*fn));
    fn->name = xstrdup(fn_ast->name);

    cur_fn = fn;
    next_temp = 0;
    next_label = 0;
    nloops = 0;
    nlsyms = 0;
    nslots = 0;

    /* Params become slots 0..nparams-1. */
    for (p = fn_ast->a; p; p = p->next) {
        struct lsym ls;
        ls.name = p->name;
        ls.base_type = p->base_type;
        ls.is_ptr = p->is_ptr;
        ls.is_array = 0;
        ls.arr_size = 0;
        ls.slot = alloc_slot(4);
        add_lsym(ls);
        nparams++;
    }
    fn->nparams = nparams;

    /* Locals next. */
    collect_local_decls(fn_ast->b);

    ins = emit(IR_FUNC);
    ins->sym = xstrdup(fn_ast->name);
    ins->nargs = nparams;

    lower_stmt(fn_ast->b);

    /* Implicit "return 0;" if the body falls off the end. */
    if (!fn->tail || (fn->tail->op != IR_RET && fn->tail->op != IR_RETV)) {
        int z = lower_const(0);
        ins = emit(IR_RETV);
        ins->a = z;
    }

    ins = emit(IR_ENDF);

    fn->ntemps = next_temp;
    fn->nlabels = next_label;
    fn->nslots = nslots;
    fn->slot_size = xmalloc(nslots * sizeof(int));
    memcpy(fn->slot_size, slot_sizes, nslots * sizeof(int));
    return fn;
}

/* ---------- global lowering ---------- */

static void
lower_global(struct node *gn)
{
    struct ir_global *g;
    struct gsym s;

    g = xcalloc(1, sizeof(*g));
    g->name = xstrdup(gn->name);
    g->base_type = gn->base_type;
    g->arr_size = gn->arr_size;
    g->is_ptr = gn->is_ptr;

    if (gn->a) {
        struct node *init = gn->a;
        if (init->kind == N_STR) {
            g->init_string = xmalloc(init->slen + 1);
            memcpy(g->init_string, init->sval, init->slen);
            g->init_string[init->slen] = '\0';
            g->init_strlen = init->slen + 1;
        } else if (init->kind == N_INIT_LIST) {
            struct node *e;
            int n = 0, i;
            for (e = init->a; e; e = e->next) n++;
            g->init_ivals = xmalloc(n * sizeof(long));
            i = 0;
            for (e = init->a; e; e = e->next) {
                if (e->kind == N_NUM || e->kind == N_CHARLIT)
                    g->init_ivals[i++] = e->ival;
                else if (e->kind == N_UNOP && e->op == T_MINUS &&
                         e->a && (e->a->kind == N_NUM || e->a->kind == N_CHARLIT))
                    g->init_ivals[i++] = -e->a->ival;
                else
                    die("lower:%d: non-constant initializer", gn->line);
            }
            g->init_count = n;
            if (g->arr_size == 0)
                g->arr_size = n;
        } else if (init->kind == N_NUM || init->kind == N_CHARLIT) {
            g->init_ivals = xmalloc(sizeof(long));
            g->init_ivals[0] = init->ival;
            g->init_count = 1;
        } else {
            die("lower:%d: non-constant initializer", gn->line);
        }
    }

    g->next = cur_prog->globals;
    cur_prog->globals = g;

    s.name = g->name;
    s.g = g;
    s.is_func = 0;
    s.base_type = g->base_type;
    s.is_array = (g->arr_size > 0);
    s.is_ptr = g->is_ptr;
    add_gsym(s);
}

static void
register_function(struct node *fn_ast)
{
    struct gsym s;
    s.name = xstrdup(fn_ast->name);
    s.g = NULL;
    s.is_func = 1;
    s.base_type = T_INT;
    s.is_array = 0;
    s.is_ptr = 0;
    add_gsym(s);
}

struct ir_program *
lower_program(struct node *ast)
{
    struct ir_program *p;
    struct node *d;
    struct ir_func *fn, **ftail;

    p = xcalloc(1, sizeof(*p));
    cur_prog = p;
    ngsyms = 0;

    /* Pass 1: register globals and function names. */
    for (d = ast ? ast->a : NULL; d; d = d->next) {
        if (d->kind == N_GLOBAL)
            lower_global(d);
        else if (d->kind == N_FUNC)
            register_function(d);
    }
    /* Register externs that the runtime provides. */
    {
        struct gsym s;
        const char *names[] = { "write", "exit", NULL };
        int i;
        for (i = 0; names[i]; i++) {
            if (find_gsym(names[i])) continue;
            s.name = xstrdup(names[i]);
            s.g = NULL;
            s.is_func = 1;
            s.base_type = T_INT;
            s.is_array = 0;
            s.is_ptr = 0;
            add_gsym(s);
        }
    }

    /* Pass 2: lower functions. */
    ftail = &p->funcs;
    for (d = ast ? ast->a : NULL; d; d = d->next) {
        if (d->kind != N_FUNC) continue;
        fn = lower_function(d);
        *ftail = fn;
        ftail = &fn->next;
    }
    return p;
}
