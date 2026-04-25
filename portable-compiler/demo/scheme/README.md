# TinScheme - A Tiny Scheme for ColdFire

TinScheme is a minimal Scheme subset that reuses the same IR and
ColdFire backend as TinC.  It exists to prove the Plan 9-style
shared-source split actually works: a second front end plugs into the
existing `ir/` and `backend/` layers with zero changes to either.

## Language

- Fixnum-only arithmetic (`+`, `-`, `*`), comparison (`=`, `<`, `>`,
  `<=`, `>=`), and booleans (`#t`, `#f`).
- `define` for top-level functions and variables.
- `if` expressions with R5RS falsiness (only `#f` is false).
- `lambda` (no free variables — lifted to top-level functions).
- Recursive function calls.

Deliberately omitted: `call/cc`, hygienic macros, tail-call
optimization, closures over free variables, the full numeric tower,
pairs/lists at runtime, strings, and I/O.

## Value Representation

All values are 32-bit tagged words (2-bit tag):

| Tag bits | Meaning | Encoding |
|----------|---------|----------|
| `01` | fixnum | `n << 2 \| 1` |
| `00` | heap pointer / nil | raw pointer (nil = 0) |
| `10` | immediate | `#f` = 2, `#t` = 6 |

Tagged arithmetic avoids untagging where possible:
`(+ a b)` = `a + b - 1`,
`(- a b)` = `a - b + 1`,
`(* a b)` = `(a >> 2) * (b - 1) + 1`.

## Layout

```
scheme/
    scheme.h       front-end header: tokens, parser, lowering entry point
    gc.h           value representation and GC API
    gc.c           mark-sweep garbage collector (~230 LOC)
    lex.c          hand-written S-expression lexer
    parse.c        recursive-descent parser producing cons-cell AST
    print.c        S-expression printer (debugging)
    lower.c        S-expression AST -> 3AC IR (~530 LOC)
    main.c         compiler driver
    test_gc.c      standalone GC unit test
```

## Build and Run

```sh
make build/tinscheme          # build the compiler
./build/tinscheme -p test.scm # parse only — print S-expressions
./build/tinscheme -o out.s test.scm  # compile to ColdFire assembly
```

The `make check` target in the top-level Makefile compiles and runs
all `tests/scm_*.scm` files alongside the TinC tests.

## Using `tinscheme` Directly

```sh
./build/tinscheme -o out.s tests/scm_fact.scm
m68k-linux-gnu-as -o out.o out.s
m68k-linux-gnu-as -o start.o runtime/start.S
m68k-linux-gnu-ld -o prog start.o out.o
qemu-m68k ./prog ; echo $?
```

## Status

Parser, GC, and code generation are complete.  The compiler handles
recursive fixnum programs (see `tests/scm_fact.scm`).  Runtime
support for heap-allocated pairs, closures with captured variables,
and tail-call optimization are not yet implemented.
