/* ir.h : portable compiler IR data structures, builder API, and shared declarations */

#ifndef IR_H
#define IR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/****************************************************************
 * Base types
 ****************************************************************/

enum ir_basetype {
	IR_I32 = 1,
	IR_I8  = 2,
};

/****************************************************************
 * Diagnostics
 ****************************************************************/

void util_set_progname(const char *name);
void die(const char *fmt, ...);
void warn(const char *fmt, ...);

/****************************************************************
 * Memory helpers
 ****************************************************************/

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/****************************************************************
 * IR opcodes
 ****************************************************************/

enum ir_op {
	IR_NOP,
	IR_LIC,
	IR_LEA,
	IR_ADL,
	IR_MOV,

	IR_ADD, IR_SUB, IR_MUL,
	IR_DIVS, IR_DIVU, IR_MODS, IR_MODU,
	IR_AND, IR_OR, IR_XOR,
	IR_SHL, IR_SHRS, IR_SHRU,
	IR_NEG, IR_NOT,

	IR_LB, IR_LBS, IR_LH, IR_LHS, IR_LW,
	IR_SB, IR_SH, IR_SW,

	IR_LDL, IR_STL,

	IR_CMPEQ, IR_CMPNE,
	IR_CMPLTS, IR_CMPLES, IR_CMPGTS, IR_CMPGES,
	IR_CMPLTU, IR_CMPLEU, IR_CMPGTU, IR_CMPGEU,

	IR_JMP, IR_BZ, IR_BNZ,

	IR_ARG,
	IR_CALL,
	IR_CALLI,
	IR_RET, IR_RETV,

	IR_FUNC, IR_ENDF, IR_LABEL,

	IR_MARK, IR_CAPTURE, IR_RESUME,
};

/****************************************************************
 * IR data structures
 ****************************************************************/

struct ir_insn {
	int op;
	int dst;
	int a, b;
	long imm;
	char *sym;
	int slot;
	int label;
	int nargs;
	struct ir_insn *next;
};

struct ir_func {
	char *name;
	int nparams;
	int nslots;
	int *slot_size;
	int ntemps;
	int nlabels;
	int nspills;
	int *temp_reg;
	int *temp_spill;
	struct ir_insn *head;
	struct ir_insn *tail;
	struct ir_func *next;
};

struct ir_global {
	char *name;
	int base_type;
	int arr_size;
	int is_ptr;
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

/****************************************************************
 * IR builder API
 ****************************************************************/

struct ir_func *ir_new_func(const char *name);
int ir_new_temp(struct ir_func *fn);
int ir_new_label(struct ir_func *fn);
struct ir_insn *ir_emit(struct ir_func *fn, int op);

/****************************************************************
 * Register allocation
 ****************************************************************/

void regalloc(struct ir_func *fn);

/****************************************************************
 * ColdFire back-end
 ****************************************************************/

void cf_emit(FILE *out, struct ir_program *prog);

#endif /* IR_H */
