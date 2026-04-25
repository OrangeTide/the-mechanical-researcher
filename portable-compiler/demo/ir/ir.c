/* ir.c : IR construction helpers */

#include "ir.h"

struct ir_func *
ir_new_func(const char *name)
{
	struct ir_func *fn;

	fn = xcalloc(1, sizeof(*fn));
	fn->name = xstrdup(name);
	return fn;
}

int
ir_new_temp(struct ir_func *fn)
{
	return fn->ntemps++;
}

int
ir_new_label(struct ir_func *fn)
{
	return fn->nlabels++;
}

struct ir_insn *
ir_emit(struct ir_func *fn, int op)
{
	struct ir_insn *i;

	i = xcalloc(1, sizeof(*i));
	i->op = op;
	i->dst = -1;
	i->a = -1;
	i->b = -1;
	i->slot = -1;
	i->label = -1;
	if (!fn->head)
		fn->head = i;
	else
		fn->tail->next = i;
	fn->tail = i;
	return i;
}
