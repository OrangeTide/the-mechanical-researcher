/* cf_emit.c : ColdFire / m68k back-end, emits GAS-syntax assembly */
/*
 * Instruction selection is direct: one IR op -> a short burst of m68k
 * instructions.  Every IR temp is assumed to live in a data register
 * (d2..d7 after regalloc) or a spill slot; when a temp is needed as
 * an address, we movea.l it into %a0 or %a1 on the spot.
 *
 * SysV m68k calling convention subset:
 *   - Args pushed right-to-left on the stack, 32 bits each; caller pops.
 *   - Return value in d0.
 *   - Callee-save: d2..d7, a2..a5, fp (a6).
 *   - Scratch:     d0, d1, a0, a1.
 *
 * Frame (after link.w %fp,#-framesize and movem.l d2-d7,-(sp)):
 *
 *      +8 + 4*i (%fp)   param i
 *      +4       (%fp)   return address
 *      +0       (%fp)   saved fp
 *      -locals_size     bottom of locals
 *      -locals_size-4   first spill slot
 *      ...              saved d2..d7 live below sp, re-popped at exit
 */

#include "ir.h"

#include <stdio.h>
#include <string.h>

static const char *dregs[] = {
	"%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6", "%d7",
};

/****************************************************************
 * Frame layout helpers
 ****************************************************************/

static int
locals_size(struct ir_func *fn)
{
	int i, s;

	s = 0;
	for (i = fn->nparams; i < fn->nslots; i++) {
		int sz = (fn->slot_size[i] + 3) & ~3;
		s += sz;
	}
	return s;
}

static int
slot_offset(struct ir_func *fn, int slot)
{
	int i, off;

	if (slot < fn->nparams)
		return 8 + 4 * slot;
	off = 0;
	for (i = fn->nparams; i <= slot; i++) {
		int sz = (fn->slot_size[i] + 3) & ~3;
		off += sz;
	}
	return -off;
}

static int
spill_byte_offset(struct ir_func *fn, int temp)
{
	return -locals_size(fn) - (fn->temp_spill[temp] + 4);
}

/****************************************************************
 * Temp -> register materialisation
 ****************************************************************/

static const char *
rs(FILE *out, struct ir_func *fn, int t, int scratch)
{
	int r = fn->temp_reg[t];

	if (r >= 0)
		return dregs[r];
	fprintf(out, "\tmove.l %d(%%fp), %s\n",
		spill_byte_offset(fn, t), dregs[scratch]);
	return dregs[scratch];
}

static const char *
rd(struct ir_func *fn, int t, int scratch)
{
	int r = fn->temp_reg[t];

	if (r >= 0)
		return dregs[r];
	return dregs[scratch];
}

static void
wd(FILE *out, struct ir_func *fn, int t, const char *reg)
{
	if (fn->temp_reg[t] >= 0)
		return;
	fprintf(out, "\tmove.l %s, %d(%%fp)\n",
		reg, spill_byte_offset(fn, t));
}

/****************************************************************
 * Binary op helper
 ****************************************************************/

static void
emit_binop(FILE *out, struct ir_func *fn, struct ir_insn *i,
           const char *mnem)
{
	const char *sa, *sb, *sd;

	sa = rs(out, fn, i->a, 0);
	sb = rs(out, fn, i->b, 1);
	sd = rd(fn, i->dst, 0);
	if (strcmp(sb, sd) == 0 && strcmp(sa, sd) != 0) {
		fprintf(out, "\tmove.l %s, %%d1\n", sb);
		sb = dregs[1];
	}
	if (strcmp(sa, sd) != 0)
		fprintf(out, "\tmove.l %s, %s\n", sa, sd);
	fprintf(out, "\t%s %s, %s\n", mnem, sb, sd);
	wd(out, fn, i->dst, sd);
}

static void
emit_unop(FILE *out, struct ir_func *fn, struct ir_insn *i,
          const char *mnem)
{
	const char *sa, *sd;

	sa = rs(out, fn, i->a, 0);
	sd = rd(fn, i->dst, 0);
	if (strcmp(sa, sd) != 0)
		fprintf(out, "\tmove.l %s, %s\n", sa, sd);
	fprintf(out, "\t%s %s\n", mnem, sd);
	wd(out, fn, i->dst, sd);
}

static void
emit_cmp(FILE *out, struct ir_func *fn, struct ir_insn *i,
         const char *scc)
{
	const char *sa, *sb, *sd;

	sa = rs(out, fn, i->a, 0);
	sb = rs(out, fn, i->b, 1);
	sd = rd(fn, i->dst, 0);
	fprintf(out, "\tmoveq #0, %s\n", sd);
	fprintf(out, "\tcmp.l %s, %s\n", sb, sa);
	fprintf(out, "\t%s %s\n", scc, sd);
	fprintf(out, "\tneg.b %s\n", sd);
	wd(out, fn, i->dst, sd);
}

/****************************************************************
 * Per-instruction emission
 ****************************************************************/

static int arg_temps[16];
static int narg;
static int label_prefix;

static void
emit_load(FILE *out, struct ir_func *fn, struct ir_insn *i,
          const char *sz, int sign_extend, int clear_first)
{
	const char *sa, *sd;

	sa = rs(out, fn, i->a, 0);
	sd = rd(fn, i->dst, 0);
	fprintf(out, "\tmovea.l %s, %%a0\n", sa);
	if (clear_first)
		fprintf(out, "\tmoveq #0, %s\n", sd);
	fprintf(out, "\tmove.%s (%%a0), %s\n", sz, sd);
	if (sign_extend) {
		if (sz[0] == 'b')
			fprintf(out, "\text.w %s\n", sd);
		fprintf(out, "\text.l %s\n", sd);
	}
	wd(out, fn, i->dst, sd);
}

static void
emit_store(FILE *out, struct ir_func *fn, struct ir_insn *i,
           const char *sz)
{
	const char *sa, *sb;

	sa = rs(out, fn, i->a, 0);
	sb = rs(out, fn, i->b, 1);
	fprintf(out, "\tmovea.l %s, %%a0\n", sa);
	fprintf(out, "\tmove.%s %s, (%%a0)\n", sz, sb);
}

static void
emit_prologue(FILE *out, struct ir_func *fn)
{
	int frame = locals_size(fn) + fn->nspills * 4;

	fprintf(out, "\tlink.w %%fp, #%d\n", -frame);
	fprintf(out, "\tmovem.l %%d2-%%d7, -(%%sp)\n");
}

static void
emit_epilogue(FILE *out)
{
	fprintf(out, "\tmovem.l (%%sp)+, %%d2-%%d7\n");
	fprintf(out, "\tunlk %%fp\n");
	fprintf(out, "\trts\n");
}

static void
emit_call_flush(FILE *out, struct ir_func *fn, struct ir_insn *i,
                int indirect)
{
	int k;

	for (k = narg - 1; k >= 0; k--) {
		const char *sa = rs(out, fn, arg_temps[k], 0);
		fprintf(out, "\tmove.l %s, -(%%sp)\n", sa);
	}
	if (indirect) {
		const char *sa = rs(out, fn, i->a, 0);
		fprintf(out, "\tmovea.l %s, %%a0\n", sa);
		fprintf(out, "\tjsr (%%a0)\n");
	} else {
		fprintf(out, "\tjsr %s\n", i->sym);
	}
	if (narg > 0)
		fprintf(out, "\tlea %d(%%sp), %%sp\n", 4 * narg);
	narg = 0;

	if (i->dst >= 0) {
		const char *sd = rd(fn, i->dst, 0);
		if (strcmp(sd, "%d0") != 0)
			fprintf(out, "\tmove.l %%d0, %s\n", sd);
		wd(out, fn, i->dst, sd);
	}
}

static void
emit_insn(FILE *out, struct ir_func *fn, struct ir_insn *i)
{
	switch (i->op) {
	case IR_NOP:
		break;

	case IR_LIC: {
		const char *sd = rd(fn, i->dst, 0);
		if (i->imm >= -128 && i->imm <= 127)
			fprintf(out, "\tmoveq #%ld, %s\n", i->imm, sd);
		else
			fprintf(out, "\tmove.l #%ld, %s\n", i->imm, sd);
		wd(out, fn, i->dst, sd);
		break;
	}

	case IR_LEA: {
		const char *sd = rd(fn, i->dst, 0);
		fprintf(out, "\tlea %s, %%a0\n", i->sym);
		fprintf(out, "\tmove.l %%a0, %s\n", sd);
		wd(out, fn, i->dst, sd);
		break;
	}

	case IR_ADL: {
		const char *sd = rd(fn, i->dst, 0);
		int off = slot_offset(fn, i->slot);
		fprintf(out, "\tlea %d(%%fp), %%a0\n", off);
		fprintf(out, "\tmove.l %%a0, %s\n", sd);
		wd(out, fn, i->dst, sd);
		break;
	}

	case IR_MOV: {
		const char *sa = rs(out, fn, i->a, 0);
		const char *sd = rd(fn, i->dst, 0);
		if (strcmp(sa, sd) != 0)
			fprintf(out, "\tmove.l %s, %s\n", sa, sd);
		wd(out, fn, i->dst, sd);
		break;
	}

	case IR_ADD:  emit_binop(out, fn, i, "add.l");  break;
	case IR_SUB:  emit_binop(out, fn, i, "sub.l");  break;
	case IR_MUL:  emit_binop(out, fn, i, "muls.l"); break;
	case IR_AND:  emit_binop(out, fn, i, "and.l");  break;
	case IR_OR:   emit_binop(out, fn, i, "or.l");   break;
	case IR_XOR:  emit_binop(out, fn, i, "eor.l");  break;
	case IR_SHL:  emit_binop(out, fn, i, "lsl.l");  break;
	case IR_SHRS: emit_binop(out, fn, i, "asr.l");  break;
	case IR_SHRU: emit_binop(out, fn, i, "lsr.l");  break;

	case IR_DIVS: emit_binop(out, fn, i, "divs.l"); break;
	case IR_DIVU: emit_binop(out, fn, i, "divu.l"); break;

	case IR_MODS:
	case IR_MODU: {
		const char *sa = rs(out, fn, i->a, 0);
		const char *sb = rs(out, fn, i->b, 1);
		const char *sd = rd(fn, i->dst, 0);
		const char *divop = (i->op == IR_MODS) ? "divs.l" : "divu.l";
		if (strcmp(sb, "%d1") != 0) {
			fprintf(out, "\tmove.l %s, %%d1\n", sb);
			sb = "%d1";
		}
		fprintf(out, "\tmove.l %s, %%d0\n", sa);
		fprintf(out, "\t%s %s, %%d0\n", divop, sb);
		fprintf(out, "\tmuls.l %s, %%d0\n", sb);
		fprintf(out, "\tmove.l %s, %s\n", sa, sd);
		fprintf(out, "\tsub.l %%d0, %s\n", sd);
		wd(out, fn, i->dst, sd);
		break;
	}

	case IR_NEG: emit_unop(out, fn, i, "neg.l"); break;
	case IR_NOT: emit_unop(out, fn, i, "not.l"); break;

	case IR_LB:  emit_load(out, fn, i, "b", 0, 1); break;
	case IR_LBS: emit_load(out, fn, i, "b", 1, 0); break;
	case IR_LH:  emit_load(out, fn, i, "w", 0, 1); break;
	case IR_LHS: emit_load(out, fn, i, "w", 1, 0); break;
	case IR_LW:  emit_load(out, fn, i, "l", 0, 0); break;

	case IR_SB: emit_store(out, fn, i, "b"); break;
	case IR_SH: emit_store(out, fn, i, "w"); break;
	case IR_SW: emit_store(out, fn, i, "l"); break;

	case IR_LDL: {
		const char *sd = rd(fn, i->dst, 0);
		int off = slot_offset(fn, i->slot);
		fprintf(out, "\tmove.l %d(%%fp), %s\n", off, sd);
		wd(out, fn, i->dst, sd);
		break;
	}

	case IR_STL: {
		const char *sa = rs(out, fn, i->a, 0);
		int off = slot_offset(fn, i->slot);
		fprintf(out, "\tmove.l %s, %d(%%fp)\n", sa, off);
		break;
	}

	case IR_CMPEQ:  emit_cmp(out, fn, i, "seq"); break;
	case IR_CMPNE:  emit_cmp(out, fn, i, "sne"); break;
	case IR_CMPLTS: emit_cmp(out, fn, i, "slt"); break;
	case IR_CMPLES: emit_cmp(out, fn, i, "sle"); break;
	case IR_CMPGTS: emit_cmp(out, fn, i, "sgt"); break;
	case IR_CMPGES: emit_cmp(out, fn, i, "sge"); break;
	case IR_CMPLTU: emit_cmp(out, fn, i, "scs"); break;
	case IR_CMPLEU: emit_cmp(out, fn, i, "sls"); break;
	case IR_CMPGTU: emit_cmp(out, fn, i, "shi"); break;
	case IR_CMPGEU: emit_cmp(out, fn, i, "scc"); break;

	case IR_JMP:
		fprintf(out, "\tbra .L%d_%d\n", label_prefix, i->label);
		break;
	case IR_BZ: {
		const char *sa = rs(out, fn, i->a, 0);
		fprintf(out, "\ttst.l %s\n\tbeq .L%d_%d\n",
			sa, label_prefix, i->label);
		break;
	}
	case IR_BNZ: {
		const char *sa = rs(out, fn, i->a, 0);
		fprintf(out, "\ttst.l %s\n\tbne .L%d_%d\n",
			sa, label_prefix, i->label);
		break;
	}
	case IR_LABEL:
		fprintf(out, ".L%d_%d:\n", label_prefix, i->label);
		break;

	case IR_ARG:
		if (narg >= 16)
			die("cf_emit: too many args");
		arg_temps[narg++] = i->a;
		break;
	case IR_CALL:
		emit_call_flush(out, fn, i, 0);
		break;
	case IR_CALLI:
		emit_call_flush(out, fn, i, 1);
		break;

	case IR_RET:
		emit_epilogue(out);
		break;
	case IR_RETV: {
		const char *sa = rs(out, fn, i->a, 0);
		if (strcmp(sa, "%d0") != 0)
			fprintf(out, "\tmove.l %s, %%d0\n", sa);
		emit_epilogue(out);
		break;
	}

	case IR_FUNC:
	case IR_ENDF:
		break;

	default:
		die("cf_emit: unhandled op %d", i->op);
	}
}

/****************************************************************
 * Per-function emission
 ****************************************************************/

static int fn_serial;

static void
emit_function(FILE *out, struct ir_func *fn)
{
	struct ir_insn *i;

	narg = 0;
	label_prefix = ++fn_serial;

	fprintf(out, "\n\t.text\n\t.align 2\n\t.globl %s\n%s:\n",
		fn->name, fn->name);
	emit_prologue(out, fn);

	for (i = fn->head; i; i = i->next)
		emit_insn(out, fn, i);

	if (!fn->tail ||
	    (fn->tail->op != IR_RET && fn->tail->op != IR_RETV))
		emit_epilogue(out);
}

/****************************************************************
 * Globals
 ****************************************************************/

static void
emit_string_bytes(FILE *out, const char *s, int n)
{
	int k;

	fputs("\t.ascii \"", out);
	for (k = 0; k < n; k++) {
		unsigned char c = (unsigned char)s[k];
		if (c == '\\')
			fputs("\\\\", out);
		else if (c == '"')
			fputs("\\\"", out);
		else if (c == '\n')
			fputs("\\n", out);
		else if (c == '\t')
			fputs("\\t", out);
		else if (c == '\r')
			fputs("\\r", out);
		else if (c >= 0x20 && c < 0x7f)
			fputc(c, out);
		else
			fprintf(out, "\\%03o", c);
	}
	fputs("\"\n", out);
}

static void
emit_globals(FILE *out, struct ir_program *prog)
{
	struct ir_global *g;

	fputs("\n\t.data\n", out);
	for (g = prog->globals; g; g = g->next) {
		int is_char_arr = (g->base_type == IR_I8 && g->arr_size > 0);
		int elsz = is_char_arr ? 1 : 4;

		fprintf(out, "\t.align 2\n\t.globl %s\n%s:\n",
			g->name, g->name);
		if (g->init_string) {
			emit_string_bytes(out, g->init_string,
					  g->init_strlen);
		} else if (g->init_count > 0) {
			int k;
			for (k = 0; k < g->init_count; k++) {
				if (elsz == 1)
					fprintf(out, "\t.byte %ld\n",
						g->init_ivals[k]);
				else
					fprintf(out, "\t.long %ld\n",
						g->init_ivals[k]);
			}
			if (g->arr_size > g->init_count)
				fprintf(out, "\t.space %d\n",
					(g->arr_size - g->init_count) * elsz);
		} else {
			int sz = (g->arr_size > 0)
				 ? g->arr_size * elsz
				 : 4;
			fprintf(out, "\t.space %d\n", sz);
		}
	}
}

/****************************************************************
 * Entry point
 ****************************************************************/

void
cf_emit(FILE *out, struct ir_program *prog)
{
	struct ir_func *fn;

	fputs("| tinc: generated ColdFire / m68k assembly\n", out);
	for (fn = prog->funcs; fn; fn = fn->next)
		emit_function(out, fn);
	emit_globals(out, prog);
}
