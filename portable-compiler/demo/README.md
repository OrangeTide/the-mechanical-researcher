# TinC - A Tiny C for ColdFire

TinC ("tiny C" / "tin-the-metal C") is a small pre-ANSI C compiler
that targets ColdFire/m68k assembly. It is the demo companion for the
*portable-compiler* article: a toy-but-honest pipeline that exercises a
real 3AC IR and a real linear-scan register allocator, and produces
code you can run under `qemu-m68k`.

## Language, in one screen

- One type: `int` (32-bit). `char` is accepted and treated as `int`
  for arithmetic; width only matters at loads and stores.
- No prototypes, no `sizeof`, no multidimensional arrays, no `static`.
  K&R function definitions (identifier list in parens, typed parameter
  decls between `)` and `{`).
- Globals may be scalars, 1D arrays, or char-array string literals.
- Statements: `if / else`, `while`, `break`, `continue`, `return`,
  expression, compound.
- Full C operator set with precedence climbing, including `&&` and
  `||` with short-circuit evaluation.
- Hex literals (`0x2a`) and `\xNN` string escapes are supported; other
  escapes are the usual `\n \t \\ \' \" \0`.

See `tests/*.tc` for worked examples.

## Layout

```
demo/
    Makefile           host + cross build rules
    src/               the compiler, in ~1500 LOC of C
        tinc.h         shared header: tokens, AST, IR, function decls
        util.c         die / xmalloc / xstrdup helpers
        lex.c          hand-written lexer
        parse.c        recursive-descent parser with precedence climb
        lower.c        AST -> 3AC IR (SSA temps, locals in memory)
        regalloc.c     linear-scan allocator + coalescing
        cf_emit.c      ColdFire/m68k GAS-syntax back end
        main.c         driver
    runtime/
        start.S        _start, write(2), exit(2) for Linux/m68k
    tests/
        hello.tc       globals, write, strlen
        fib.tc         recursion, callee-save discipline
        memcpy.tc      pointer arithmetic, char loads/stores
        bsearch.tc     int[] search - walkthrough subject
        loop.tc        primitive fnmatch - break/continue stress
        spill.tc       nested 6-arg calls force register spilling
        run-tests.sh   runs each test under qemu-m68k
```

## Prerequisites

- A host C compiler (`cc`, C99).
- GNU binutils for m68k, providing `m68k-linux-gnu-as` and
  `m68k-linux-gnu-ld`. On Debian/Ubuntu:
  ```sh
  sudo apt install binutils-m68k-linux-gnu
  ```
- `qemu-m68k` user-mode emulator. On Debian/Ubuntu:
  ```sh
  sudo apt install qemu-user
  ```

Override `M68K_AS`, `M68K_LD`, and `QEMU` on the `make` command line
if your toolchain uses different names.

## Build and run

```sh
make              # build the tinc compiler -> ./tinc
make check        # compile every tests/*.tc, assemble, link, run under qemu
make clean        # remove ./tinc and build/
```

`make check` prints `PASS` / `FAIL` per test. Each test program
encodes its expected exit status in `tests/<name>.expect` (one
integer); tests without such a file must merely exit 0.

## Using `tinc` directly

```sh
./tinc -o out.s tests/bsearch.tc
m68k-linux-gnu-as -o out.o out.s
m68k-linux-gnu-as -o start.o runtime/start.S
m68k-linux-gnu-ld -o prog start.o out.o
qemu-m68k ./prog ; echo $?
```

## Status

The skeleton lands the interfaces, build system, runtime, and test
corpus. The compiler passes (lex, parse, lower, regalloc, cf_emit)
are stubbed; the article walks through filling each one in.
