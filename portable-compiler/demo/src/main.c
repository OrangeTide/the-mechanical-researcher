/* main.c - TinC driver.
 *
 * Usage:  tinc [-o out.s] input.tc
 *
 * Pipeline: read -> lex_init -> parse_program -> lower_program
 *                -> regalloc (per fn) -> cf_emit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinc.h"

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

int
main(int argc, char **argv)
{
    const char *inpath;
    const char *outpath;
    FILE *out;
    char *src;
    struct node *ast;
    struct ir_program *prog;
    struct ir_func *fn;
    int i;

    inpath = NULL;
    outpath = NULL;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outpath = argv[++i];
        } else if (argv[i][0] == '-') {
            die("unknown option: %s", argv[i]);
        } else {
            inpath = argv[i];
        }
    }
    if (inpath == NULL)
        die("usage: tinc [-o out.s] input.tc");

    src = slurp(inpath);
    lex_init(src, inpath);
    ast = parse_program();
    prog = lower_program(ast);
    for (fn = prog->funcs; fn != NULL; fn = fn->next)
        regalloc(fn);

    if (outpath != NULL) {
        out = fopen(outpath, "w");
        if (out == NULL)
            die("cannot write %s", outpath);
    } else {
        out = stdout;
    }
    cf_emit(out, prog);
    if (out != stdout)
        fclose(out);

    return 0;
}
