/* main.c : TinScheme compiler driver */

#include "scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
slurp(const char *path)
{
	FILE *f;
	long n;
	char *buf;

	f = fopen(path, "rb");
	if (f == NULL)
		die("cannot open %s", path);
	if (fseek(f, 0, SEEK_END) != 0)
		die("seek failed on %s", path);
	n = ftell(f);
	if (n < 0)
		die("ftell failed on %s", path);
	rewind(f);
	buf = xmalloc((size_t)n + 1);
	if (fread(buf, 1, (size_t)n, f) != (size_t)n)
		die("read failed on %s", path);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

static void
usage(void)
{
	die("usage: tinscheme [-p] [-o out.s] input.scm");
}

int
main(int argc, char **argv)
{
	const char *inpath;
	const char *outpath;
	char *src;
	struct gc_heap heap;
	val_t program = VAL_NIL;
	int print_only;
	int i;

	util_set_progname("tinscheme");

	inpath = NULL;
	outpath = NULL;
	print_only = 0;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			outpath = argv[++i];
		} else if (strcmp(argv[i], "-p") == 0) {
			print_only = 1;
		} else if (argv[i][0] == '-') {
			die("unknown option: %s", argv[i]);
		} else {
			inpath = argv[i];
		}
	}
	if (inpath == NULL)
		usage();

	gc_init(&heap);
	gc_push(&heap, &program);

	src = slurp(inpath);
	lex_init(src, inpath);
	program = scm_read_all(&heap);

	if (print_only) {
		val_t cur;
		for (cur = program; !IS_NIL(cur); cur = gc_cdr(cur)) {
			scm_println(gc_car(cur));
		}
		gc_pop(&heap, 1);
		gc_destroy(&heap);
		return 0;
	}

	{
		struct ir_program *ir;
		struct ir_func *fn;
		FILE *out;

		ir = scm_lower(&heap, program);
		gc_pop(&heap, 1);
		gc_destroy(&heap);

		for (fn = ir->funcs; fn; fn = fn->next)
			regalloc(fn);

		out = fopen(outpath ? outpath : "a.out.s", "w");
		if (!out)
			die("cannot open output file");
		cf_emit(out, ir);
		fclose(out);
	}
	return 0;
}
