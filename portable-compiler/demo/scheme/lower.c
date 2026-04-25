/* lower.c : S-expression to IR lowering for TinScheme
 *
 * Tagged value representation on the 32-bit ColdFire target:
 *   fixnum:  n<<2 | 1      (TAG_FIX)
 *   pointer: raw ptr | 0   (TAG_OBJ, nil = 0)
 *   boolean: #f = 2, #t = 6 (TAG_IMM)
 *
 * Tagged arithmetic:
 *   (+ a b) = a + b - 1
 *   (- a b) = a - b + 1
 *   (* a b) = (a>>2) * (b-1) + 1
 *   (- a)   = 2 - a
 *
 * Boolean from comparison:
 *   IR cmp yields 0/1  ->  (cmp << 2) | 2  ->  2 or 6 = #f or #t
 *
 * R5RS falsiness: only #f (value 2) is falsy.
 */

#include "scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************************************************
 * IR builder shortcuts
 ****************************************************************/

static struct ir_func *cur_fn;
static struct ir_program *cur_prog;

static struct ir_insn *
emit(int op)
{
	return ir_emit(cur_fn, op);
}

static int
new_temp(void)
{
	return ir_new_temp(cur_fn);
}

static int
new_label(void)
{
	return ir_new_label(cur_fn);
}

static int
lower_const(long v)
{
	struct ir_insn *ins;

	ins = emit(IR_LIC);
	ins->dst = new_temp();
	ins->imm = v;
	return ins->dst;
}

static void
emit_label(int lab)
{
	struct ir_insn *ins;

	ins = emit(IR_LABEL);
	ins->label = lab;
}

static void
emit_jmp(int lab)
{
	struct ir_insn *ins;

	ins = emit(IR_JMP);
	ins->label = lab;
}

/****************************************************************
 * S-expression helpers
 ****************************************************************/

static int
is_sym(val_t v, const char *name)
{
	struct gc_string *s;

	if (!IS_PTR(v))
		return 0;
	if (PTR(v)->type != OBJ_SYMBOL)
		return 0;
	s = (struct gc_string *)PTR(v);
	return strcmp(s->data, name) == 0;
}

static const char *
sym_name(val_t v)
{
	return ((struct gc_string *)PTR(v))->data;
}

static int
is_pair(val_t v)
{
	return IS_PTR(v) && PTR(v)->type == OBJ_PAIR;
}

static int
is_symbol(val_t v)
{
	return IS_PTR(v) && PTR(v)->type == OBJ_SYMBOL;
}

/****************************************************************
 * Global symbol table
 ****************************************************************/

enum { GSYM_FUNC, GSYM_VAR, GSYM_RUNTIME };

struct gsym {
	char *name;
	int kind;
	int nparams;
};

static struct gsym *gsyms;
static int ngsyms;
static int gsym_cap;

static void
add_gsym(const char *name, int kind, int nparams)
{
	if (ngsyms == gsym_cap) {
		gsym_cap = gsym_cap ? gsym_cap * 2 : 16;
		gsyms = realloc(gsyms, gsym_cap * sizeof(*gsyms));
		if (!gsyms)
			die("oom");
	}
	gsyms[ngsyms].name = xstrdup(name);
	gsyms[ngsyms].kind = kind;
	gsyms[ngsyms].nparams = nparams;
	ngsyms++;
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

/****************************************************************
 * Per-function locals
 ****************************************************************/

struct lsym {
	char *name;
	int slot;
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
		slot_sizes = realloc(slot_sizes,
		    slot_cap * sizeof(*slot_sizes));
		if (!slot_sizes)
			die("oom");
	}
	slot_sizes[nslots] = bytes;
	return nslots++;
}

static void
add_lsym(const char *name)
{
	if (nlsyms == lsym_cap) {
		lsym_cap = lsym_cap ? lsym_cap * 2 : 16;
		lsyms = realloc(lsyms, lsym_cap * sizeof(*lsyms));
		if (!lsyms)
			die("oom");
	}
	lsyms[nlsyms].name = xstrdup(name);
	lsyms[nlsyms].slot = alloc_slot(4);
	nlsyms++;
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

/****************************************************************
 * Lambda lifting
 ****************************************************************/

struct lifted {
	char name[64];
	val_t params;
	val_t body;
	struct lifted *next;
};

static struct lifted *lifted_head;
static struct lifted *lifted_tail;
static int lambda_serial;

/****************************************************************
 * Forward declaration
 ****************************************************************/

static int lower_expr(val_t expr);

/****************************************************************
 * Boolean conversion: IR cmp result (0/1) -> Scheme boolean
 *
 * #f = 2 = 0<<2|2, #t = 6 = 1<<2|2, so: (cmp << 2) | 2
 ****************************************************************/

static int
bool_from_cmp(int cmp_temp)
{
	struct ir_insn *ins;
	int t_two, t_shifted;

	t_two = lower_const(2);
	ins = emit(IR_SHL);
	ins->dst = new_temp();
	ins->a = cmp_temp;
	ins->b = t_two;
	t_shifted = ins->dst;

	ins = emit(IR_OR);
	ins->dst = new_temp();
	ins->a = t_shifted;
	ins->b = t_two;
	return ins->dst;
}

/****************************************************************
 * Special form lowering
 ****************************************************************/

static int
lower_cmp(val_t args, int ir_cmp_op)
{
	struct ir_insn *ins;
	int ta, tb, tcmp;

	ta = lower_expr(gc_car(args));
	tb = lower_expr(gc_car(gc_cdr(args)));
	ins = emit(ir_cmp_op);
	ins->dst = new_temp();
	ins->a = ta;
	ins->b = tb;
	tcmp = ins->dst;
	return bool_from_cmp(tcmp);
}

/* (if test then [else]) — with shortcut for comparison tests */
static int
lower_if(val_t args)
{
	struct ir_insn *ins;
	val_t test_form, then_form, else_form;
	int t_result;
	int ltrue, lfalse, lend;
	int has_else;

	test_form = gc_car(args);
	then_form = gc_car(gc_cdr(args));
	has_else = !IS_NIL(gc_cdr(gc_cdr(args)));
	else_form = has_else ? gc_car(gc_cdr(gc_cdr(args))) : VAL_NIL;

	ltrue = new_label();
	lfalse = new_label();
	lend = new_label();

	if (is_pair(test_form) && is_symbol(gc_car(test_form))) {
		int ir_cmp = -1;

		if (is_sym(gc_car(test_form), "="))
			ir_cmp = IR_CMPEQ;
		else if (is_sym(gc_car(test_form), "<"))
			ir_cmp = IR_CMPLTS;
		else if (is_sym(gc_car(test_form), ">"))
			ir_cmp = IR_CMPGTS;
		else if (is_sym(gc_car(test_form), "<="))
			ir_cmp = IR_CMPLES;
		else if (is_sym(gc_car(test_form), ">="))
			ir_cmp = IR_CMPGES;

		if (ir_cmp >= 0) {
			val_t test_args = gc_cdr(test_form);
			int ta, tb, tcmp;

			ta = lower_expr(gc_car(test_args));
			tb = lower_expr(gc_car(gc_cdr(test_args)));
			ins = emit(ir_cmp);
			ins->dst = new_temp();
			ins->a = ta;
			ins->b = tb;
			tcmp = ins->dst;
			ins = emit(IR_BNZ);
			ins->a = tcmp;
			ins->label = ltrue;
			emit_jmp(lfalse);
			goto branches;
		}
	}

	/* General case: R5RS falsiness (only #f is false) */
	{
		int t_test, t_false, t_diff;

		t_test = lower_expr(test_form);
		t_false = lower_const((long)VAL_FALSE);
		ins = emit(IR_SUB);
		ins->dst = new_temp();
		ins->a = t_test;
		ins->b = t_false;
		t_diff = ins->dst;
		ins = emit(IR_BNZ);
		ins->a = t_diff;
		ins->label = ltrue;
		emit_jmp(lfalse);
	}

branches:
	t_result = new_temp();

	emit_label(ltrue);
	{
		int t_then = lower_expr(then_form);

		ins = emit(IR_MOV);
		ins->dst = t_result;
		ins->a = t_then;
	}
	emit_jmp(lend);

	emit_label(lfalse);
	if (has_else) {
		int t_else = lower_expr(else_form);

		ins = emit(IR_MOV);
		ins->dst = t_result;
		ins->a = t_else;
	} else {
		ins = emit(IR_LIC);
		ins->dst = t_result;
		ins->imm = 0;
	}

	emit_label(lend);
	return t_result;
}

static int
lower_add(val_t args)
{
	struct ir_insn *ins;
	int ta, tb, tsum, tone;

	ta = lower_expr(gc_car(args));
	tb = lower_expr(gc_car(gc_cdr(args)));
	ins = emit(IR_ADD);
	ins->dst = new_temp();
	ins->a = ta;
	ins->b = tb;
	tsum = ins->dst;

	tone = lower_const(1);
	ins = emit(IR_SUB);
	ins->dst = new_temp();
	ins->a = tsum;
	ins->b = tone;
	return ins->dst;
}

static int
lower_sub(val_t args)
{
	struct ir_insn *ins;
	int ta;

	ta = lower_expr(gc_car(args));

	/* Unary negation: (- a) = 2 - a */
	if (IS_NIL(gc_cdr(args))) {
		int t_two;

		t_two = lower_const(2);
		ins = emit(IR_SUB);
		ins->dst = new_temp();
		ins->a = t_two;
		ins->b = ta;
		return ins->dst;
	}

	/* Binary subtraction: (- a b) = a - b + 1 */
	{
		int tb, tdiff, tone;

		tb = lower_expr(gc_car(gc_cdr(args)));
		ins = emit(IR_SUB);
		ins->dst = new_temp();
		ins->a = ta;
		ins->b = tb;
		tdiff = ins->dst;

		tone = lower_const(1);
		ins = emit(IR_ADD);
		ins->dst = new_temp();
		ins->a = tdiff;
		ins->b = tone;
		return ins->dst;
	}
}

/* (* a b) = (a>>2) * (b-1) + 1 */
static int
lower_mul(val_t args)
{
	struct ir_insn *ins;
	int ta, tb, t_two, t_x, t_one, t_b1, t_prod;

	ta = lower_expr(gc_car(args));
	tb = lower_expr(gc_car(gc_cdr(args)));

	t_two = lower_const(2);
	ins = emit(IR_SHRS);
	ins->dst = new_temp();
	ins->a = ta;
	ins->b = t_two;
	t_x = ins->dst;

	t_one = lower_const(1);
	ins = emit(IR_SUB);
	ins->dst = new_temp();
	ins->a = tb;
	ins->b = t_one;
	t_b1 = ins->dst;

	ins = emit(IR_MUL);
	ins->dst = new_temp();
	ins->a = t_x;
	ins->b = t_b1;
	t_prod = ins->dst;

	ins = emit(IR_ADD);
	ins->dst = new_temp();
	ins->a = t_prod;
	ins->b = t_one;
	return ins->dst;
}

static int
lower_call(val_t form)
{
	struct ir_insn *ins;
	val_t fn_form, arg_list, cur;
	int args[16];
	int nargs, i;

	fn_form = gc_car(form);
	arg_list = gc_cdr(form);
	nargs = 0;

	for (cur = arg_list; !IS_NIL(cur); cur = gc_cdr(cur)) {
		if (nargs >= 16)
			die("lower: too many arguments");
		args[nargs++] = lower_expr(gc_car(cur));
	}

	for (i = 0; i < nargs; i++) {
		ins = emit(IR_ARG);
		ins->a = args[i];
		ins->imm = i;
	}

	if (is_symbol(fn_form)) {
		const char *name = sym_name(fn_form);
		struct lsym *ls = find_lsym(name);
		struct gsym *gs;

		if (ls) {
			int tf;

			ins = emit(IR_LDL);
			ins->dst = new_temp();
			ins->slot = ls->slot;
			tf = ins->dst;
			ins = emit(IR_CALLI);
			ins->dst = new_temp();
			ins->a = tf;
			ins->nargs = nargs;
			return ins->dst;
		}
		gs = find_gsym(name);
		if (gs && (gs->kind == GSYM_FUNC ||
		           gs->kind == GSYM_RUNTIME)) {
			ins = emit(IR_CALL);
			ins->dst = new_temp();
			ins->sym = xstrdup(name);
			ins->nargs = nargs;
			return ins->dst;
		}
		if (gs && gs->kind == GSYM_VAR) {
			int taddr, tf;

			ins = emit(IR_LEA);
			ins->dst = new_temp();
			ins->sym = xstrdup(name);
			taddr = ins->dst;
			ins = emit(IR_LW);
			ins->dst = new_temp();
			ins->a = taddr;
			tf = ins->dst;
			ins = emit(IR_CALLI);
			ins->dst = new_temp();
			ins->a = tf;
			ins->nargs = nargs;
			return ins->dst;
		}
		die("lower: undefined '%s'", name);
	}

	{
		int tf;

		tf = lower_expr(fn_form);
		ins = emit(IR_CALLI);
		ins->dst = new_temp();
		ins->a = tf;
		ins->nargs = nargs;
		return ins->dst;
	}
}

static int
lower_reset(val_t args)
{
    struct ir_insn *ins;
    val_t body_expr, handler_expr;
    int slot, tbuf, tbody, lelse, lend;
    int tresult;

    if (IS_NIL(args) || IS_NIL(gc_cdr(args)))
        die("lower: (reset body handler) requires two arguments");
    body_expr = gc_car(args);
    handler_expr = gc_car(gc_cdr(args));

    slot = alloc_slot(12);
    lelse = new_label();
    lend = new_label();
    tresult = new_temp();

    ins = emit(IR_MARK);
    ins->dst = new_temp();
    ins->slot = slot;
    ins->label = new_label();
    tbuf = ins->dst;

    /* if mark returned nonzero (shift fired), branch to handler */
    ins = emit(IR_BNZ);
    ins->a = tbuf;
    ins->label = lelse;

    /* First entry: evaluate body */
    tbody = lower_expr(body_expr);
    ins = emit(IR_MOV);
    ins->dst = tresult;
    ins->a = tbody;
    ins = emit(IR_JMP);
    ins->label = lend;

    /* Else branch: shift fired, call handler with buffer */
    ins = emit(IR_LABEL);
    ins->label = lelse;

    {
        int tf, tcall;
        tf = lower_expr(handler_expr);
        ins = emit(IR_ARG);
        ins->a = tbuf;
        ins->imm = 0;
        ins = emit(IR_CALLI);
        ins->dst = new_temp();
        ins->a = tf;
        ins->nargs = 1;
        tcall = ins->dst;
        ins = emit(IR_MOV);
        ins->dst = tresult;
        ins->a = tcall;
    }

    ins = emit(IR_LABEL);
    ins->label = lend;
    return tresult;
}

static int
lower_shift(void)
{
    struct ir_insn *ins;

    ins = emit(IR_CAPTURE);
    ins->dst = new_temp();
    return ins->dst;
}

static int
lower_resume(val_t args)
{
    struct ir_insn *ins;
    int tbuf, tval;

    if (IS_NIL(args) || IS_NIL(gc_cdr(args)))
        die("lower: (resume buf val) requires two arguments");
    tbuf = lower_expr(gc_car(args));
    tval = lower_expr(gc_car(gc_cdr(args)));

    ins = emit(IR_RESUME);
    ins->a = tbuf;
    ins->b = tval;
    return new_temp();
}

static int
lower_lambda(val_t args)
{
	struct ir_insn *ins;
	struct lifted *l;

	l = xmalloc(sizeof(*l));
	snprintf(l->name, sizeof(l->name), "__lambda_%d",
	    lambda_serial++);
	l->params = gc_car(args);
	l->body = gc_cdr(args);
	l->next = NULL;

	if (!lifted_head)
		lifted_head = l;
	else
		lifted_tail->next = l;
	lifted_tail = l;

	ins = emit(IR_LEA);
	ins->dst = new_temp();
	ins->sym = xstrdup(l->name);
	return ins->dst;
}

/****************************************************************
 * Expression lowering
 ****************************************************************/

static int
lower_expr(val_t expr)
{
	struct ir_insn *ins;

	if (IS_FIX(expr))
		return lower_const((long)GET_FIX(expr) * 4 + 1);

	if (expr == VAL_TRUE)
		return lower_const((long)VAL_TRUE);
	if (expr == VAL_FALSE)
		return lower_const((long)VAL_FALSE);
	if (IS_NIL(expr))
		return lower_const(0);

	if (is_symbol(expr)) {
		const char *name = sym_name(expr);
		struct lsym *ls = find_lsym(name);
		struct gsym *gs;

		if (ls) {
			ins = emit(IR_LDL);
			ins->dst = new_temp();
			ins->slot = ls->slot;
			return ins->dst;
		}
		gs = find_gsym(name);
		if (!gs)
			die("lower: undefined '%s'", name);
		if (gs->kind == GSYM_FUNC) {
			ins = emit(IR_LEA);
			ins->dst = new_temp();
			ins->sym = xstrdup(name);
			return ins->dst;
		}
		/* Global variable: LEA + LW */
		{
			int taddr;

			ins = emit(IR_LEA);
			ins->dst = new_temp();
			ins->sym = xstrdup(name);
			taddr = ins->dst;
			ins = emit(IR_LW);
			ins->dst = new_temp();
			ins->a = taddr;
			return ins->dst;
		}
	}

	if (is_pair(expr)) {
		val_t head = gc_car(expr);
		val_t rest = gc_cdr(expr);

		if (is_sym(head, "if"))
			return lower_if(rest);
		if (is_sym(head, "+"))
			return lower_add(rest);
		if (is_sym(head, "-"))
			return lower_sub(rest);
		if (is_sym(head, "*"))
			return lower_mul(rest);
		if (is_sym(head, "="))
			return lower_cmp(rest, IR_CMPEQ);
		if (is_sym(head, "<"))
			return lower_cmp(rest, IR_CMPLTS);
		if (is_sym(head, ">"))
			return lower_cmp(rest, IR_CMPGTS);
		if (is_sym(head, "<="))
			return lower_cmp(rest, IR_CMPLES);
		if (is_sym(head, ">="))
			return lower_cmp(rest, IR_CMPGES);
		if (is_sym(head, "lambda"))
			return lower_lambda(rest);
		if (is_sym(head, "begin")) {
			int t = lower_const(0);
			val_t cur;

			for (cur = rest; !IS_NIL(cur); cur = gc_cdr(cur))
				t = lower_expr(gc_car(cur));
			return t;
		}
		if (is_sym(head, "quote"))
			die("lower: quoted expressions not yet "
			    "supported in codegen");

		if (is_sym(head, "reset"))
			return lower_reset(rest);
		if (is_sym(head, "shift"))
			return lower_shift();
		if (is_sym(head, "resume"))
			return lower_resume(rest);

		return lower_call(expr);
	}

	die("lower: unhandled expression type");
	return 0;
}

/****************************************************************
 * Function lowering
 ****************************************************************/

static void
finish_slots(struct ir_func *fn)
{
	fn->nslots = nslots;
	if (nslots > 0) {
		fn->slot_size = xmalloc(nslots * sizeof(int));
		memcpy(fn->slot_size, slot_sizes,
		    nslots * sizeof(int));
	} else {
		fn->slot_size = NULL;
	}
}

static struct ir_func *
lower_function(const char *name, val_t params, val_t body)
{
	struct ir_func *fn;
	struct ir_insn *ins;
	val_t cur;
	int nparams = 0;
	int t_result = -1;

	fn = ir_new_func(name);
	cur_fn = fn;
	nlsyms = 0;
	nslots = 0;

	for (cur = params; !IS_NIL(cur); cur = gc_cdr(cur)) {
		add_lsym(sym_name(gc_car(cur)));
		nparams++;
	}
	fn->nparams = nparams;

	ins = emit(IR_FUNC);
	ins->sym = xstrdup(name);
	ins->nargs = nparams;

	for (cur = body; !IS_NIL(cur); cur = gc_cdr(cur))
		t_result = lower_expr(gc_car(cur));

	if (t_result >= 0) {
		ins = emit(IR_RETV);
		ins->a = t_result;
	} else {
		int z = lower_const(0);

		ins = emit(IR_RETV);
		ins->a = z;
	}

	emit(IR_ENDF);
	finish_slots(fn);
	return fn;
}

/****************************************************************
 * Top-level form classification
 *
 * Returns 1 for (define (name params...) body...),
 *         2 for (define name expr),
 *         0 for other forms.
 ****************************************************************/

static int
classify_define(val_t form, val_t *name, val_t *params,
                val_t *body, val_t *init)
{
	val_t binding;

	if (!is_pair(form) || !is_sym(gc_car(form), "define"))
		return 0;
	binding = gc_car(gc_cdr(form));
	if (is_pair(binding)) {
		*name = gc_car(binding);
		*params = gc_cdr(binding);
		*body = gc_cdr(gc_cdr(form));
		return 1;
	}
	if (is_symbol(binding)) {
		*name = binding;
		*init = gc_car(gc_cdr(gc_cdr(form)));
		return 2;
	}
	die("lower: malformed define");
	return 0;
}

/****************************************************************
 * Entry point
 ****************************************************************/

struct ir_program *
scm_lower(struct gc_heap *h, val_t program)
{
	struct ir_program *prog;
	struct ir_func **ftail;
	val_t cur;

	(void)h;
	prog = xcalloc(1, sizeof(*prog));
	cur_prog = prog;
	ngsyms = 0;
	lifted_head = NULL;
	lifted_tail = NULL;
	lambda_serial = 0;

	/* Pass 1: register all global names */
	for (cur = program; !IS_NIL(cur); cur = gc_cdr(cur)) {
		val_t form = gc_car(cur);
		val_t name = VAL_NIL, params = VAL_NIL;
		val_t body = VAL_NIL, init = VAL_NIL;
		int kind = classify_define(form, &name, &params,
		                           &body, &init);

		if (kind == 1) {
			int np = 0;
			val_t pcur;

			for (pcur = params; !IS_NIL(pcur);
			     pcur = gc_cdr(pcur))
				np++;
			add_gsym(sym_name(name), GSYM_FUNC, np);
		} else if (kind == 2) {
			add_gsym(sym_name(name), GSYM_VAR, 0);
		}
	}

	if (!find_gsym("write"))
		add_gsym("write", GSYM_RUNTIME, 3);
	if (!find_gsym("exit"))
		add_gsym("exit", GSYM_RUNTIME, 1);

	/* Create IR globals for variable definitions */
	for (cur = program; !IS_NIL(cur); cur = gc_cdr(cur)) {
		val_t form = gc_car(cur);
		val_t name = VAL_NIL, params = VAL_NIL;
		val_t body = VAL_NIL, init = VAL_NIL;

		if (classify_define(form, &name, &params,
		                    &body, &init) == 2) {
			struct ir_global *g;

			g = xcalloc(1, sizeof(*g));
			g->name = xstrdup(sym_name(name));
			g->base_type = IR_I32;
			g->next = prog->globals;
			prog->globals = g;
		}
	}

	ftail = &prog->funcs;

	/* Pass 2: lower defined functions */
	for (cur = program; !IS_NIL(cur); cur = gc_cdr(cur)) {
		val_t form = gc_car(cur);
		val_t name = VAL_NIL, params = VAL_NIL;
		val_t body = VAL_NIL, init = VAL_NIL;

		if (classify_define(form, &name, &params,
		                    &body, &init) == 1) {
			struct ir_func *fn;

			fn = lower_function(sym_name(name),
			    params, body);
			*ftail = fn;
			ftail = &fn->next;
		}
	}

	/* Pass 3: generate main — init globals, eval top-level exprs,
	 * untag last result for exit code */
	{
		struct ir_func *fn;
		struct ir_insn *ins;
		int t_last = -1;

		fn = ir_new_func("main");
		cur_fn = fn;
		fn->nparams = 0;
		nlsyms = 0;
		nslots = 0;

		ins = emit(IR_FUNC);
		ins->sym = xstrdup("main");
		ins->nargs = 0;

		for (cur = program; !IS_NIL(cur);
		     cur = gc_cdr(cur)) {
			val_t form = gc_car(cur);
			val_t name = VAL_NIL, params = VAL_NIL;
			val_t body = VAL_NIL, init = VAL_NIL;
			int kind = classify_define(form, &name,
			    &params, &body, &init);

			if (kind == 1) {
				continue;
			} else if (kind == 2) {
				int tval, taddr;

				tval = lower_expr(init);
				ins = emit(IR_LEA);
				ins->dst = new_temp();
				ins->sym = xstrdup(sym_name(name));
				taddr = ins->dst;
				ins = emit(IR_SW);
				ins->a = taddr;
				ins->b = tval;
			} else {
				t_last = lower_expr(form);
			}
		}

		if (t_last >= 0) {
			int t_two, t_untagged;

			t_two = lower_const(2);
			ins = emit(IR_SHRS);
			ins->dst = new_temp();
			ins->a = t_last;
			ins->b = t_two;
			t_untagged = ins->dst;
			ins = emit(IR_RETV);
			ins->a = t_untagged;
		} else {
			int z = lower_const(0);

			ins = emit(IR_RETV);
			ins->a = z;
		}

		emit(IR_ENDF);
		finish_slots(fn);
		*ftail = fn;
		ftail = &fn->next;
	}

	/* Pass 4: compile lifted lambdas */
	while (lifted_head) {
		struct lifted *l = lifted_head;
		struct ir_func *fn;

		lifted_head = l->next;
		fn = lower_function(l->name, l->params, l->body);
		*ftail = fn;
		ftail = &fn->next;
		free(l);
	}

	return prog;
}
