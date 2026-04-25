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
- C arithmetic, comparison, logical, and bitwise operators with
  precedence climbing, including `&&` and `||` with short-circuit
  evaluation.  No `++`/`--`.
- Hex literals (`0x2a`) and `\xNN` string escapes are supported; other
  escapes are the usual `\n \t \\ \' \" \0`.

See `tests/*.tc` for worked examples.

## Layout

```
demo/
    Makefile             host + cross build rules
    ir/                  portable IR library
        ir.h             IR types, opcodes, builder API, shared declarations
        ir.c             IR construction helpers (ir_emit, ir_new_temp, ...)
        util.c           die / warn / xmalloc / xstrdup helpers
    backend/             target-specific code generation
        regalloc_cf.c    linear-scan register allocator (ColdFire d2..d7)
        cf_emit.c        ColdFire/m68k GAS-syntax back end
    tinc/                TinC front end
        tinc.h           front-end header: tokens, AST, lowering entry point
        lex.c            hand-written lexer
        parse.c          recursive-descent parser with precedence climb
        lower.c          AST -> 3AC IR (SSA temps, locals in memory)
        main.c           compiler driver
    scheme/              TinScheme front end (minimal Scheme subset)
        scheme.h         front-end header
        gc.h / gc.c      mark-sweep GC and tagged value representation
        lex.c            S-expression lexer
        parse.c          recursive-descent S-expression parser
        lower.c          S-expression AST -> 3AC IR (tagged arithmetic)
        print.c          S-expression printer
        main.c           compiler driver
    runtime/
        start.S          _start, write(2), exit(2) for Linux/m68k
    tests/
        hello.tc         globals, write, strlen
        fib.tc           recursion, callee-save discipline
        memcpy.tc        pointer arithmetic, char loads/stores
        bsearch.tc       int[] search - walkthrough subject
        loop.tc          primitive fnmatch - break/continue stress
        spill.tc         nested 6-arg calls force register spilling
        mod.tc           modulo operator under register pressure
        scm_fact.scm     TinScheme factorial (recursive, tagged fixnum)
        run-tests.sh     runs each test under qemu-m68k
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
make              # build the tinc compiler -> build/tinc
make check        # compile every tests/*.tc, assemble, link, run under qemu
make clean        # remove build/
```

`make check` prints `PASS` / `FAIL` per test. Each test program
encodes its expected exit status in `tests/<name>.expect` (one
integer); tests without such a file must merely exit 0.

## Using `tinc` directly

```sh
./build/tinc -o out.s tests/bsearch.tc
m68k-linux-gnu-as -o out.o out.s
m68k-linux-gnu-as -o start.o runtime/start.S
m68k-linux-gnu-ld -o prog start.o out.o
qemu-m68k ./prog ; echo $?
```

## Status

All compiler passes are implemented.  The code is split into three
layers -- `ir/` (portable IR builder), `backend/` (ColdFire code
generation), and `tinc/` (front end) -- following a Plan 9-style
shared-source approach so the IR and backend can be reused by other
front ends.  TinScheme (`scheme/`) is a second front end that proves
the split works: it compiles a minimal Scheme subset through the same
IR and backend with no changes to either.
