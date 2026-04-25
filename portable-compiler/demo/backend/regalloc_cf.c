/* regalloc_cf.c : linear-scan register allocator for ColdFire */
/*
 * Poletto & Sarkar (1999) linear-scan register allocation.
 *
 * ColdFire / m68k register parameters
 * ------------------------------------
 * Allocatable (callee-save): d2..d7  ->  NUM_REGS = 6, FIRST_REG = 2
 *
 * Reserved:
 *   d0  return value / caller-save scratch
 *   d1  spill-fix-up scratch (reloads)
 *   a0  call lowering / address scratch
 *   a1  address scratch
 *   a6  frame pointer
 *   a7  stack pointer
 *
 * To retarget: copy this file, change NUM_REGS and FIRST_REG to match
 * the new ISA's allocatable register file, and adjust the callee-save
 * set accordingly.
 *
 * The emitter inspects fn->temp_reg[t]:
 *   >= 0 -> physical register number (FIRST_REG..FIRST_REG+NUM_REGS-1)
 *   -1   -> spilled; fn->temp_spill[t] is the byte offset
 *           within the spill area.
 *
 * fn->nspills is the number of spill slots used (each 4 bytes).
 */

#include "ir.h"

#include <stdlib.h>

#define NUM_REGS  6
#define FIRST_REG 2

struct interval {
	int temp;
	int start;
	int end;
	int reg;
	int spill;
};

static int
sort_by_start(const void *pa, const void *pb)
{
	const struct interval *a = pa;
	const struct interval *b = pb;

	if (a->start != b->start)
		return a->start - b->start;
	return a->end - b->end;
}

void
regalloc(struct ir_func *fn)
{
	struct ir_insn *i;
	struct interval *ivs;
	struct interval **active;
	int *first_def;
	int *last_use;
	int pool[NUM_REGS];
	int ntemps;
	int niv;
	int nactive;
	int nspills;
	int pos;
	int t;
	int k;
	int p;
	int r;
	int ins_at;
	int w;
	int got;

	ntemps = fn->ntemps;
	fn->temp_reg = xmalloc(ntemps > 0 ? ntemps * sizeof(int) : 1);
	fn->temp_spill = xmalloc(ntemps > 0 ? ntemps * sizeof(int) : 1);
	for (t = 0; t < ntemps; t++) {
		fn->temp_reg[t] = -1;
		fn->temp_spill[t] = -1;
	}
	if (ntemps == 0) {
		fn->nspills = 0;
		return;
	}

	first_def = xmalloc(ntemps * sizeof(int));
	last_use = xmalloc(ntemps * sizeof(int));
	for (t = 0; t < ntemps; t++) {
		first_def[t] = -1;
		last_use[t] = -1;
	}

	pos = 0;
	for (i = fn->head; i; i = i->next, pos++) {
		if (i->dst >= 0 && i->dst < ntemps) {
			if (first_def[i->dst] < 0)
				first_def[i->dst] = pos;
			if (last_use[i->dst] < pos)
				last_use[i->dst] = pos;
		}
		if (i->a >= 0 && i->a < ntemps && last_use[i->a] < pos)
			last_use[i->a] = pos;
		if (i->b >= 0 && i->b < ntemps && last_use[i->b] < pos)
			last_use[i->b] = pos;
	}

	ivs = xmalloc(ntemps * sizeof(*ivs));
	niv = 0;
	for (t = 0; t < ntemps; t++) {
		if (first_def[t] < 0)
			continue;
		ivs[niv].temp = t;
		ivs[niv].start = first_def[t];
		ivs[niv].end = last_use[t] >= first_def[t]
			       ? last_use[t] : first_def[t];
		ivs[niv].reg = -1;
		ivs[niv].spill = -1;
		niv++;
	}
	qsort(ivs, niv, sizeof(*ivs), sort_by_start);

	for (k = 0; k < NUM_REGS; k++)
		pool[k] = 1;

	active = xmalloc((NUM_REGS + 1) * sizeof(*active));
	nactive = 0;
	nspills = 0;

	for (p = 0; p < niv; p++) {
		struct interval *iv = &ivs[p];

		w = 0;
		for (r = 0; r < nactive; r++) {
			if (active[r]->end < iv->start) {
				pool[active[r]->reg - FIRST_REG] = 1;
			} else {
				active[w++] = active[r];
			}
		}
		nactive = w;

		got = -1;
		for (k = 0; k < NUM_REGS; k++) {
			if (pool[k]) {
				got = k + FIRST_REG;
				pool[k] = 0;
				break;
			}
		}

		if (got >= 0) {
			iv->reg = got;
			ins_at = nactive;
			for (r = 0; r < nactive; r++) {
				if (active[r]->end > iv->end) {
					ins_at = r;
					break;
				}
			}
			for (r = nactive; r > ins_at; r--)
				active[r] = active[r - 1];
			active[ins_at] = iv;
			nactive++;
		} else {
			struct interval *sp = active[nactive - 1];
			if (sp->end > iv->end) {
				iv->reg = sp->reg;
				sp->reg = -1;
				sp->spill = nspills++ * 4;
				nactive--;
				ins_at = nactive;
				for (r = 0; r < nactive; r++) {
					if (active[r]->end > iv->end) {
						ins_at = r;
						break;
					}
				}
				for (r = nactive; r > ins_at; r--)
					active[r] = active[r - 1];
				active[ins_at] = iv;
				nactive++;
			} else {
				iv->spill = nspills++ * 4;
			}
		}
	}

	for (p = 0; p < niv; p++) {
		fn->temp_reg[ivs[p].temp] = ivs[p].reg;
		fn->temp_spill[ivs[p].temp] = ivs[p].spill;
	}
	fn->nspills = nspills;

	free(active);
	free(ivs);
	free(first_def);
	free(last_use);
}
