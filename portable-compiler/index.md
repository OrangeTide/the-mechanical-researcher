---
title: Portable Compilers - IR Design and Register Allocation
date: 2026-04-19
revised: 2026-04-25
abstract: How multi-target compilers split the frontend from the backend, and what a toy ColdFire compiler teaches about IR shape and linear-scan register allocation.
category: compilers
---

## Introduction

A portable compiler is a compiler with a seam. On one side, a frontend
turns source text into something that no longer knows what language it
came from. On the other side, a backend turns that something into
machine code without knowing what source code it came from. The
"something" in the middle — the intermediate representation — is
where the compiler's politics live. How typed is it? How much does it
commit to control flow? Is the register allocator part of the IR, or
something that happens to it?

This article walks that middle. It surveys the choices made by four
compilers that span the design space — LLVM, GCC, TCC, and Plan 9's
`8c`/`6c` family — then works through the choices made by a toy
compiler, **TinC**, that ships as a companion demo. TinC is a pre-ANSI
C compiler in roughly 3,000 lines of C that targets ColdFire/m68k and
runs its tests under `qemu-m68k`. It is not a production tool; it is
the smallest vehicle that still has to answer every question a
portable compiler has to answer.

## Abstract

The IR design space has three axes: *typing* (how much the IR knows
about the language types above it), *form* (SSA or not; flat 3AC or
regional trees), and *analysis surface* (whether the IR is meant to be
optimised on, or just traversed). LLVM maxes out all three. GCC runs
two IRs back to back. TCC elides the IR entirely. Plan 9 keeps a small
IR and pushes most portability into convention. TinC picks the Plan 9
end of the spectrum: a flat 3AC IR with about fifty opcodes, "SSA
within, memory between" for locals, and Poletto-Sarkar linear-scan
with six allocatable data registers. A walkthrough compiles `bsearch`
from source to ColdFire assembly and reads the output instruction by
instruction.

## The IR Design Space

An IR is a data structure plus a contract. The data structure is
usually a list or graph of instructions; the contract says what the
frontend is allowed to hand the backend, and what the backend is
allowed to assume in return. Three questions shape that contract.

**Typing.** Does the IR carry types, and are those types language
types (C's `int`, `char *`) or machine types (`i32`, `ptr`)? A typed
IR makes some optimisations trivial (dead-store elimination across an
`int` and a `char` uses the type to disambiguate) and others hard (a
backend that wants to lower a `struct` now has to understand
`struct`). An untyped IR makes lowering local but pushes every
analysis to recompute what it needs.

**Form.** Three-address code (3AC) is linear: each instruction names
its inputs and its output by temp id. Static Single Assignment extends
3AC with the rule that every temp is written exactly once, which makes
def-use chains trivial but forces `φ`-nodes at every join. Tree-shaped
IRs keep expressions nested, which is natural for pattern-matching
code generators but awkward for optimisation.

**Analysis surface.** Is the IR a medium for passes, or a waypoint
between them? A medium is stable, well-documented, and verifiable
(LLVM's `opt -verify` is a public contract). A waypoint is internal:
the frontend produces it, one or two passes run, the backend consumes
it, and nobody writes tools against it.

| Compiler | Typing | Form | Surface |
|---|---|---|---|
| LLVM | machine-typed, explicit | SSA, flat | public, stable, verifiable |
| GCC | language-typed (GIMPLE) then untyped (RTL) | SSA (GIMPLE), flat (RTL) | public but two IRs |
| TCC | none | none (one-pass direct) | no surface |
| Plan 9 `8c` | lightly typed | tree + 3AC stack | internal convention |

The rest of this section walks each compiler in a paragraph or two.

### LLVM: Maximalist Typed SSA

LLVM's IR is the clearest working answer to "what if the middle was a
product?" Every instruction has a type drawn from a small machine-like
algebra (`i1`, `i8`, `i32`, `ptr`, vectors, structs). Every value is
in SSA. The text form is round-trippable with the binary form. The
verifier rejects ill-typed or ill-formed IR before any pass sees it.

The cost is the size of the surface. LLVM's IR reference is a
book-length document; a backend must cope with every opcode and every
type combination the middle-end might produce. The benefit is that
passes can be written as if they were libraries — InstCombine,
LoopVectorize, and GVN don't know about each other and don't need to.

### GCC: Two IRs for Two Jobs

GCC lowers through two IRs in sequence. **GIMPLE** is a
language-level, typed, 3AC-shaped SSA form; it is where high-level
optimisations live (inlining, loop transforms, devirtualization).
After those passes, GIMPLE is lowered to **RTL** — Register Transfer
Language, an untyped, target-parameterised tree form that looks like
LISP describing machine instructions. RTL is where instruction
selection, scheduling, and graph-coloring register allocation happen.

The two-IR design lets each phase use a form that fits its job.
GIMPLE is good for reasoning about loops; RTL is good for reasoning
about which instruction to pick. The price is that every target has
to describe itself twice — once in the `.md` machine description that
drives RTL, once in the target hooks that GIMPLE consults.

### TCC: No IR At All

Fabrice Bellard's Tiny C Compiler has no IR. The parser emits x86
machine code directly as it walks the input, using a small value stack
to track whether the "current expression" lives in a register, on the
stack, in a global, or as an immediate. A conditional branch turns
into a forward jump with a patch list; a backward branch resolves when
the label appears.

TCC is fast (it is often used as a JIT) and small (roughly 20 kLOC
for the core — lexer, preprocessor, and a single-architecture
backend; all backends and utilities together reach around 55 kLOC,
but that is no longer an apples-to-apples comparison),
and it produces mediocre code. The one-pass design is incompatible
with most optimisations because there is no place to put an
optimisation — by the time you could think about a value, its bytes
are already in the output buffer. TCC's trade is deliberate: compile
speed over output quality.

### Plan 9: Library of Small Passes

Ken Thompson's `8c`/`6c`/`5c` family (x86, amd64, ARM) takes a third
position: a small, internal IR threaded through a sequence of small
passes, none of which is globally aware of the others. The frontend
produces a tree IR with light typing. A series of passes lowers it to
a 3AC-ish form ("Prog" lists) specific to the target. Register
allocation is linear-scan-like but written per-target in a few hundred
lines. There is no common middle-end: each compiler is a separate
program sharing a libbio/libc runtime and a common assembly syntax.

The Plan 9 position is that portability comes from *convention*
(identical syntax, identical calling conventions, identical
driver interface) rather than from *abstraction* (a shared typed IR).
The cost is that a new optimisation has to be ported to each target.
The benefit is that each compiler stays under ten thousand lines, and
a reader can hold one in their head.

## Register Allocation in One Picture

The register allocator is where the IR's fiction of unbounded virtual
temps meets the hardware's small fixed set of physical registers. Two
algorithmic families dominate:

**Graph coloring** (Chaitin-Briggs and descendants) builds an
interference graph whose nodes are temps and whose edges connect
temps that are live at the same time, then k-colors it with a register
per color, spilling temps whose nodes cannot be colored. Graph
coloring produces near-optimal allocations but is O(n²) in the
interference graph, which makes it expensive for very large functions.
GCC's reload and LLVM's older allocators used variants of this.

**Linear scan** (Poletto and Sarkar, 1999) assumes instructions are
already laid out in a straight line and computes, for each temp, an
*interval* `[first_def, last_use]`. It walks intervals in order of
`first_def`, maintains an active set of intervals currently using
registers, retires intervals whose end has passed, and assigns a free
register to each new interval. When no register is free, it spills
the active interval with the latest end — or the new interval, if
the new one ends even later. The algorithm is O(n log n) in the
number of temps and produces allocations within a few percent of
graph coloring on straight-line code.

**SSA-based allocation** (Hack and Goos, 2006) exploits the fact that
SSA programs have chordal interference graphs, which can be k-colored
in polynomial time. LLVM supports multiple allocator strategies —
Simple, Local, Linear Scan, and Greedy — selectable at compile time.

TinC uses straight linear scan. The demo is small enough that the
spill rate is essentially zero on realistic programs — the loop in
`bsearch` uses seven temps across its inner block; six fit in `d2..d7`.

## TinC: The Companion Demo

TinC compiles a subset of pre-ANSI C. It accepts `int` and `char`,
pointers and arrays of those, globals, functions with old-style
parameter declarations, the obvious operators, `if`/`else`/`while`, and recursion. It rejects `struct`, `typedef`,
floating point, `#include`, and the preprocessor. The test
programs cover `hello`, `fib`, `loop`, `spill`, `mod`,
`memcpy`/`strcpy`, `bsearch`, and `cont` (delimited continuations).

The source is split into three directories — `ir/` for the portable
IR library, `backend/` for ColdFire code generation, and `tinc/` for
the front end — so the IR and backend can be reused by other front
ends.  The pipeline:

| Stage | File | Lines | Output |
|---|---|---|---|
| Lexer | `tinc/lex.c` | 304 | token stream |
| Parser | `tinc/parse.c` | 574 | AST |
| Lowering | `tinc/lower.c` | 973 | IR |
| Register alloc | `backend/regalloc_cf.c` | 201 | IR + `temp_reg[]` |
| ColdFire emit | `backend/cf_emit.c` | 561 | `.s` |

### The IR

Fifty-four opcodes, grouped by purpose:

- **Data moves.** `IR_LIC` loads an immediate, `IR_LEA` takes the
  address of a global symbol, `IR_ADL` takes the address of a local
  slot, `IR_MOV` copies one temp to another.
- **Arithmetic.** `IR_ADD`, `IR_SUB`, `IR_MUL`, signed/unsigned
  divide and modulo, bitwise, shifts, `IR_NEG`, `IR_NOT`.
- **Memory.** Byte/half/word loads (signed and unsigned variants
  where sign matters), matching stores; plus `IR_LDL`/`IR_STL` for
  local-slot load/store, which sit one level above raw addresses.
- **Comparisons.** `IR_CMPEQ`..`IR_CMPGEU` produce a 0/1 result in a
  temp.
- **Control.** `IR_JMP`, `IR_BZ`, `IR_BNZ`, labels.
- **Calls.** `IR_ARG` pushes an argument, `IR_CALL` calls by name,
  `IR_CALLI` calls through a temp, `IR_RET`/`IR_RETV` return.
- **Continuations.** `IR_MARK` sets a delimiter, `IR_CAPTURE` saves
  the stack segment up to it, `IR_RESUME` restores and jumps back.
- **Framing.** `IR_FUNC`, `IR_ENDF`, `IR_LABEL`.

The memory model is "SSA within, memory between". Inside a basic
block, temps are written once; across basic blocks, values live in
local slots (`IR_LDL`/`IR_STL`). This avoids `φ`-nodes — every
cross-block value is a memory round-trip — at the cost of some extra
loads and stores that a better allocator would elide. The trade-off
is defensible for a toy and catastrophic for a real compiler.

### The Allocator

`backend/regalloc_cf.c` is a straight transcription of Poletto-Sarkar. It
reserves `d0` (return value scratch), `d1` (spill-reload scratch),
`a0`/`a1` (address scratch for the backend), `a6` (frame pointer),
and `a7` (stack pointer), leaving `d2..d7` — six callee-save
registers — as the allocatable pool.

The interval build is a single pass over the instruction list: for
each temp, `first_def` is the position of its first write,
`last_use` is the position of its last read or write. Intervals are
sorted by `first_def` and walked in order. The active set is kept
sorted by `end` so that the spill candidate — the active interval
with the latest end — is at the tail.

The spill rule is the textbook one: when no register is free, pick
whichever of `{new interval, tail of active}` ends later and spill
it. The non-spilled one gets the register. Spill slots are allocated
linearly in `fn->nspills * 4` bytes of stack, and the backend emits
loads and stores whenever it touches a spilled temp.

There is no coalescing. A MOV between two temps that could have
shared a register produces two instructions instead of zero. For the
test suite, this costs a handful of redundant moves per function; for
a real program it would be the first thing to add.

### The ColdFire Backend

`backend/cf_emit.c` is a per-opcode lowering. Each IR instruction turns
into one or a few m68k instructions. The backend materialises
addresses in `a0`/`a1`, handles the frame with `link.w`/`unlk`, saves
`d2..d7` at function entry with `movem.l`, and follows the SysV m68k
calling convention (args pushed right to left by caller, caller pops,
`d0` holds the return value, `d2..d7` are callee-save).

Two traps worth naming:

- **`moveq` clobbers condition codes.** The comparison sequence is
  `moveq #0, dst; cmp.l b, a; scc dst; neg.b dst`. The `moveq` has to
  come *before* the `cmp.l`, because `moveq` sets N and Z as a side
  effect; putting it between `cmp` and `scc` destroys the flags that
  `scc` needs. This cost a debugging session.
- **qemu-m68k's default stack is one page.** The `fib` test overflows
  it after a handful of recursive frames with a fault at
  `0xc0000ffc`. The runtime in `runtime/start.S` reserves a 64 KB
  stack in `.bss` and switches `%sp` to it before calling `main`.

### Walkthrough: `bsearch`

The test program is a twelve-element sorted array and a classic
half-open binary search:

```c
int sorted[] = { 1, 3, 5, 7, 9, 11, 13, 17, 19, 23, 29, 31 };
int nsorted = 12;

bsearch(key)
int key;
{
    int lo, hi, mid, x;
    int found;

    lo = 0;
    hi = nsorted;
    found = -1;
    while (lo < hi) {
        mid = (lo + hi) / 2;
        x = sorted[mid];
        if (x == key) { found = mid; break; }
        if (x < key) { lo = mid + 1; continue; }
        hi = mid;
    }
    return found;
}
```

The frame is twenty bytes: five 4-byte slots for `lo`, `hi`, `mid`,
`x`, `found`. The parameter `key` lives at `8(%fp)` — above the
saved frame pointer and return address. The function prologue:

```asm
bsearch:
    link.w %fp, #-20
    movem.l %d2-%d7, -(%sp)
```

`link.w` pushes the old frame pointer, sets `%fp` to the new frame
base, and subtracts 20 for locals. `movem.l` saves the six callee-save
data registers in one instruction.

The interesting inner bit is the array load at `sorted[mid]`. In IR:

```
LIC  t = 4             ; element width
MUL  m = mid * t
LEA  p = &sorted
ADD  q = p + m
LW   x = *q
```

The emitter lowers each opcode locally. The MUL uses `muls.l`, the
LEA uses `lea sorted, %a0` then moves `%a0` into a data register, the
ADD uses `add.l`, and the load uses `movea.l` followed by `move.l
(%a0), dst`. The resulting asm (with spill-store elided) is ten
instructions; a peephole pass could cut it in half by folding the
LEA into the address expression and recognising `muls.l #4` as a
shift. TinC does neither.

The comparison `lo < hi` turns into the sequence the earlier trap
note describes:

```asm
    move.l -4(%fp), %d2        ; lo
    move.l -8(%fp), %d3        ; hi
    moveq #0, %d4              ; clear dst BEFORE cmp
    cmp.l %d3, %d2             ; sets flags
    slt %d4                    ; %d4 low byte = 0xFF if less, else 0
    neg.b %d4                  ; 0xFF -> 1, 0 -> 0
    tst.l %d4
    bne .L1_2                  ; loop body
    bra .L1_1                  ; loop exit
```

Every cross-block value (`lo`, `hi`, `mid`, `x`, `found`) round-trips
through its local slot. A real allocator with live-range analysis
across basic blocks would keep most of these in registers across the
loop back-edge. TinC's inner loop executes roughly thirty
instructions per iteration; hand-written m68k would do it in eight.

## What TinC Gets Right, and What It Ducks

**Gets right.** The shape is honest. The IR opcode set is the one a
real C compiler needs, minus the long tail of float, struct, and
alignment. Lowering is separated cleanly from code generation.
Register allocation is a real linear scan, not an ad-hoc heuristic.
The backend respects a real calling convention. Adding a second target
would mean writing a new emitter and allocator in `backend/` — the IR
does not know what it came from or where it's going.

**Ducks.** No optimisation. No dataflow. No loop transforms. No
coalescing. No peephole. No scheduling. Cross-block values are
memory round-trips by construction, which leaves performance on the
table in the most visible place. Type checking is minimal. Error
recovery is non-existent — `die` and quit. Float is unsupported.
Struct is unsupported. Preprocessor is unsupported. Inline assembly
is unsupported. The runtime is four instructions of `_start`.

This is the right set of compromises for a didactic implementation
that has to fit in a weekend of reading. The piece of machinery the
article exists to show — "unlimited virtual temps meet six physical
registers, and here is the algorithm that mediates" — is fully
present and fully working, on a real ISA, through a real emulator, on
a real test suite.

## The Compounding Effect: Delimited Continuations

The payoff of a shared IR is not code reuse in the abstract — it is
that a single backend change compounds across every front end.
Delimited continuations provide a concrete example.

A delimited continuation captures a slice of the call stack — from the
current point up to a designated delimiter — packages it into a
buffer, and later resumes execution from that buffer with a new return
value. Think of `setjmp`/`longjmp` except the saved context is
copy-on-capture and one-shot: the stack segment between delimiter and
capture point is memcpy'd to a bump-allocated arena, and resume
memcpy's it back. The mechanism supports generators, coroutine-style
producers, and cooperative multitasking patterns that would otherwise
require OS threads or manual state machines.

Three new IR opcodes implement it:

- **`IR_MARK`** places a delimiter. It saves the frame pointer, stack
  pointer, and the address of a re-entry label into a 12-byte slot in
  the local frame, then publishes that slot's address through a global
  pointer. On first entry, the mark writes zero to its destination
  temp; after a capture fires, execution resumes at the re-entry label
  with the destination set to the captured buffer's address.

- **`IR_CAPTURE`** fires the continuation. Inline code pushes
  `d2`–`d7` (callee-saves), then calls a runtime helper that reads the
  mark slot, copies the stack segment between the current SP and the
  mark's saved SP into a heap buffer, and longjmps to the mark's
  re-entry label with d0 pointing to the buffer.

- **`IR_RESUME`** restores the continuation. The runtime reads the
  buffer header, restores FP and SP to their captured values, copies the
  saved stack data back, and executes `rts` — which pops the return
  address that was on the stack at capture time, landing at the
  instruction after `IR_CAPTURE` with the resume value in d0.

The runtime implementation (in `start.S`) is roughly 70 instructions
of ColdFire assembly: a capture routine, a resume routine, a global
mark-slot pointer, and a 64 KB arena. The key design choice is that
`__cont_capture` uses `jmp` (not `rts`) to reach the mark's re-entry
label, because the mark's saved SP points into the middle of the
function's frame — there is no return address on top. Resume *can* use
`rts` because the captured stack segment includes the `jsr
__cont_capture` return address from the capture site.

With these three opcodes in the backend, TinC gains intrinsics
`__mark()`, `__capture()`, and `__resume(buf, val)`. A TinC program
builds delimited continuations with explicit control flow:

```c
produce()
{
    int k;
    k = __capture();
    return k + 100;
}
main()
{
    int buf;
    buf = __mark();
    if (buf) return __resume(buf, 42);
    return produce() - 142;
}
```

TinScheme gets `shift`, `reset`, and `resume` as special forms — the
same three opcodes, wrapped in Scheme syntax with tagged-value
semantics. See [TinScheme's README](demo/scheme/README.html) for
details.

The engineering lesson: adding those three opcodes required touching
one backend file (the emitter), one runtime file, and zero lines of
the register allocator or IR library. Each front end wired in support
independently — about 30 lines in TinC's lowering, about 60 in
TinScheme's. One backend investment, two language features, zero
coordination.

## Conclusion

Portable compilers differ mostly in how they answer one question: how
much does the middle know? LLVM's answer is "almost everything, and
we publish the spec." GCC's is "enough for high-level passes, and
then again enough for instruction selection." TCC's is "nothing,
there is no middle." Plan 9's is "just enough to share a backend
skeleton." TinC sits closest to Plan 9 — a small, private, untyped
3AC with "SSA within, memory between" for locals, and a linear-scan
allocator on top.

The lesson of writing one is that the interesting part is not the
frontend or the backend; it is the contract in the middle. Once the
contract is fixed, each side becomes almost mechanical.  The IR is
the compiler.  To test that claim, the demo includes a second front
end — [TinScheme](demo/scheme/) — that compiles a minimal Scheme
subset to the same IR with zero changes to the backend.  Adding
delimited continuations to the backend took three opcodes and 70
instructions of runtime — and both front ends gained the feature
independently.

## Source

The compiler and test programs live in [demo/](demo/). See
[demo/README.md](demo/README.md) ([HTML](demo/README.html)) for build and run instructions.
