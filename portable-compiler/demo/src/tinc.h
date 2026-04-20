/* tinc.h - Shared declarations for the TinC compiler.
 *
 * Pipeline:  source -> lex -> parse -> AST -> lower -> IR
 *                                              -> regalloc -> cf_emit -> .s
 */
#ifndef TINC_H
#define TINC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ---------- Diagnostics ---------- */

void die(const char *fmt, ...);
void warn(const char *fmt, ...);

/* ---------- Lexer ---------- */

enum tok {
    T_EOF = 0,
    T_INT, T_CHAR, T_IF, T_ELSE, T_WHILE,
    T_BREAK, T_CONTINUE, T_RETURN,
    T_IDENT, T_NUMBER, T_STRING, T_CHARLIT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACK, T_RBRACK,
    T_COMMA, T_SEMI, T_ASSIGN,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_AMP, T_PIPE, T_CARET, T_TILDE, T_BANG,
    T_ANDAND, T_OROR,
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
    T_SHL, T_SHR,
    T_INC, T_DEC
};

struct token {
    int kind;
    long nval;          /* T_NUMBER, T_CHARLIT */
    char *sval;         /* T_IDENT, T_STRING (nul-terminated, owned) */
    int slen;           /* T_STRING length (strings may contain NULs) */
    int line;
};

void lex_init(const char *src, const char *filename);
struct token lex_next(void);
struct token lex_peek(void);

/* ---------- AST ---------- */

enum node_kind {
    N_PROGRAM, N_FUNC, N_GLOBAL,
    N_BLOCK, N_IF, N_WHILE, N_BREAK, N_CONTINUE, N_RETURN, N_EXPR_STMT,
    N_BINOP, N_UNOP, N_ASSIGN, N_INDEX, N_CALL, N_NAME,
    N_NUM, N_STR, N_CHARLIT,
    N_INIT_LIST
};

struct node {
    int kind;
    int op;             /* token kind for BINOP/UNOP */
    int line;

    /* generic children */
    struct node *a, *b, *c;

    /* lists (siblings via ->next) */
    struct node *next;

    /* leaves */
    char *name;         /* identifier / symbol */
    long ival;
    char *sval;         /* string literal */
    int slen;

    /* declarations */
    int is_ptr;
    int arr_size;       /* 0 = not an array, -1 = deduced */
    int base_type;      /* T_INT or T_CHAR */

    /* resolved during lower */
    int slot;           /* local slot, param index, or temp id */
};

struct node *parse_program(void);

/* ---------- IR ---------- */

enum ir_op {
    IR_NOP,
    IR_LIC,                     /* dst = imm */
    IR_LEA,                     /* dst = &sym */
    IR_ADL,                     /* dst = &local(slot) */
    IR_MOV,                     /* dst = a */

    IR_ADD, IR_SUB, IR_MUL,
    IR_DIVS, IR_DIVU, IR_MODS, IR_MODU,
    IR_AND, IR_OR, IR_XOR,
    IR_SHL, IR_SHRS, IR_SHRU,
    IR_NEG, IR_NOT,

    IR_LB, IR_LBS, IR_LH, IR_LHS, IR_LW,
    IR_SB, IR_SH, IR_SW,

    IR_LDL, IR_STL,             /* local-slot load/store */

    IR_CMPEQ, IR_CMPNE,
    IR_CMPLTS, IR_CMPLES, IR_CMPGTS, IR_CMPGES,
    IR_CMPLTU, IR_CMPLEU, IR_CMPGTU, IR_CMPGEU,

    IR_JMP, IR_BZ, IR_BNZ,

    IR_ARG,
    IR_CALL,                    /* dst = @sym(nargs) */
    IR_CALLI,                   /* dst = %fp(nargs)  */
    IR_RET, IR_RETV,

    IR_FUNC, IR_ENDF, IR_LABEL
};

struct ir_insn {
    int op;
    int dst;                    /* temp id, or -1 */
    int a, b;                   /* temp ids, or -1 */
    long imm;
    char *sym;
    int slot;                   /* local slot for ADL/LDL/STL */
    int label;                  /* label id for JMP/BZ/BNZ/LABEL */
    int nargs;                  /* for CALL */
    struct ir_insn *next;
};

struct ir_func {
    char *name;
    int nparams;                /* first nparams slots are parameters */
    int nslots;                 /* total slots (params + locals) */
    int *slot_size;             /* bytes per slot */
    int ntemps;
    int nlabels;
    int nspills;                /* filled in by regalloc */
    int *temp_reg;              /* filled in by regalloc; -1 == spilled */
    int *temp_spill;            /* spill slot offset, if spilled */
    struct ir_insn *head;
    struct ir_insn *tail;
    struct ir_func *next;
};

struct ir_global {
    char *name;
    int base_type;              /* T_INT or T_CHAR */
    int arr_size;               /* 0 = scalar */
    int is_ptr;
    /* initializer: list of longs (for int/char arrays and scalars) or a
     * string blob (char[]). Left as TODO in the skeleton. */
    long *init_ivals;
    int init_count;
    char *init_string;
    int init_strlen;
    struct ir_global *next;
};

struct ir_program {
    struct ir_func *funcs;
    struct ir_global *globals;
};

/* ---------- Lowering ---------- */

struct ir_program *lower_program(struct node *ast);

/* ---------- Register allocation ---------- */

/* Annotates each ir_insn with physical-register assignments in-place,
 * inserts spill loads/stores, and sets fn->nspills. */
void regalloc(struct ir_func *fn);

/* ---------- ColdFire back-end ---------- */

void cf_emit(FILE *out, struct ir_program *prog);

/* ---------- Memory helpers ---------- */

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

#endif /* TINC_H */
