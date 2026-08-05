---
title: "What Bit Manipulation Buys an RV32 Interpreter"
date: 2026-08-03
abstract: "Adding Zba, Zbb and Zbs to a small RISC-V emulator, and measuring what the RP2350's extension set is actually worth"
category: systems
---

## Introduction

The [RV32 engine](/rv32-emulator/) built in the previous article implements
RV32IMAFC with Zicsr, Zifencei and Zcmp. That set was chosen by asking what
a compiler needs to produce working code, which is the right question when
the goal is to run programs at all. It is not the question a real chip
answers.

The Raspberry Pi Pico 2 is a useful check on that. Its RP2350 carries two
Hazard3 RISC-V cores alongside the Arm ones, configured as RV32IMAC with
Zicsr, Zifencei, Zba, Zbb, Zbs, Zbkb, Zcb and Zcmp, plus machine and user
mode, physical memory protection with up to sixteen regions, and a few
vendor extensions for power and interrupt control. Someone
building a small in-order core for a cost-sensitive part chose to spend gates
on those. The interesting three are Zba, Zbb and Zbs, because together they
are exactly the ratified B extension, so implementing them is standard work
rather than a chase after one vendor's core.

This article adds those three to the interpreter and measures what they are
worth. The measurement matters more than the addition, because an emulator
is the one place where "fewer instructions" and "less time" are different
claims, and here they disagree by a factor of four.

## Abstract

Zba, Zbb and Zbs are implemented in 269 lines of C, taking the interpreter
from 1,998 to 2,267 lines and its freestanding WebAssembly module from
19,394 to 20,410 bytes. Correctness is established the same way as before:
29 of the riscv-arch-test B suite tests pass with signatures compared
against `qemu-system-riscv32`, the randomized fuzzer generates all 29
encodings and compares 102,400 instructions against `qemu-riscv32` with no
divergence, a compiled guest runs in lockstep for 6,464 instructions, and 80
directed unit tests cover the boundaries a fuzzer reaches only by accident.
Rebuilding four guests with the extensions and changing nothing else cuts
retired instructions by 9.8% on a bit-mixing benchmark, 3.0% on CoreMark and
1.8% on Lua. Wall-clock time falls by much less, 2.6% and 1.9%, because in
an interpreter a bit-manipulation instruction costs more to execute than the
two or three instructions it replaced. Where the interpreter's own decoder
was extended also mattered: testing for the new encodings before the base
integer set cost 7.8% of throughput on code that uses none of them, and
moving the test into the rejection path brought that down to 2.3%.

## The Series

Part two of five on one small RV32 interpreter, built to be embedded in
another program.

1. [A Compact RV32 Engine, Verified Against QEMU](/rv32-emulator/) — the
   interpreter, and four independent ways of establishing it is correct
2. **What Bit Manipulation Buys an RV32 Interpreter** — Zba, Zbb and Zbs,
   measured in retired instructions
3. [Zcb: The Same Instructions in Less Space](/rv32-zcb/) — compressed byte
   and halfword forms, measured in code size
4. [Interrupts, and the Bug Three Test Methods Missed](/rv32-interrupts/) —
   interrupt delivery, and a defect no reference model could catch
5. [Coverage Told Us About the Bug and We Did Not Listen](/rv32-coverage/) —
   auditing the rig with coverage and mutation testing

Every article measures the same interpreter at a different point in its
life. `rv32.c` is that interpreter, the single evolving copy the whole
project builds against, and
[`rv32-orig.c`](/rv32-emulator/demo/rv32-orig.c) beside it is the version
the first article described, kept unchanged so that its measurements stay
reproducible. Every figure below is against the core as it stood when this
article was written, which is `rv32-orig.c` plus the work described here.

## What the Three Extensions Are

Twenty-nine instructions on RV32, all of them pure functions of one or two
registers. There is no new architectural state, no memory access and no trap
any of them can raise.

| Extension | Count | What it does |
|---|---|---|
| Zba | 3 | `sh1add`, `sh2add`, `sh3add`: shift by 1, 2 or 3 and add |
| Zbb | 18 | `clz`, `ctz`, `cpop`, `andn`, `orn`, `xnor`, `min`, `minu`, `max`, `maxu`, `sext.b`, `sext.h`, `zext.h`, `rol`, `ror`, `rori`, `orc.b`, `rev8` |
| Zbs | 8 | `bset`, `bclr`, `binv`, `bext` and their immediate forms |

Zba exists because array indexing is the most common arithmetic in compiled
code: `a[i]` for a four-byte element is a shift by two and an add, and one
instruction is enough for it. Zbb is the general-purpose group, and the two
members worth naming are `orc.b`, which sets every bit of a byte if any bit
of that byte was set and is what makes a word-at-a-time `strlen` cheap, and
`rev8`, which is a byte swap. Zbs treats a register as a bit array.

The rest of the Hazard3 set was left out deliberately. Zbkb adds `pack`,
`brev8` and the RV32-only `zip` and `unzip`, which no compiler emits on its
own; they exist for cipher implementations. Zbc is carry-less multiply,
which is not part of B and is not enabled in the RP2350's configuration,
though Hazard3 can be built with it. Zcb adds
eleven compressed encodings and is a reasonable follow-on, but it touches
the 16-bit decoder rather than the 32-bit one and is a different piece of
work. Physical memory protection is architecturally interesting and
practically redundant here: this emulator's isolation boundary is already
one address space with the host reachable only through `ecall`, and the
optional access-check callback refuses anything outside it.

The three that were taken are also the three the specification groups
together. `misa` has a single B bit, and it means Zba, Zbb and Zbs all
present. There is no way to advertise two of the three.

## Implementation

The decoder gains two functions, one for the register-register forms and one
for the register-immediate forms, and the CPU struct gains one switch beside
the existing ones for Zcmp and the atomics.

```c
static int
bitmanip_reg(uint32_t insn, uint32_t f3, uint32_t f7,
             uint32_t a, uint32_t b, uint32_t *out);

static int
bitmanip_imm(uint32_t insn, uint32_t f3, uint32_t f7,
             uint32_t a, uint32_t *out);
```

Each returns 1 with the result in `*out` when the encoding belongs to it and
0 when it does not. Every `funct7` value they accept is one the base integer
set leaves unassigned, so the two decoders cannot disagree about an encoding
and the order in which they are tried is a free choice.

That free choice turned out to cost 7.8% of interpreter throughput, which is
the first result of this article and is discussed below.

Three instructions are worth showing, because they are where a
straightforward implementation goes wrong.

```c
static uint32_t
rotr32(uint32_t v, uint32_t n)
{
    n &= 31;
    return n ? (v >> n) | (v << (32 - n)) : v;
}
```

A rotate by zero has to be special-cased. Writing it as `(v >> n) | (v << (32
- n))` shifts by 32 when `n` is zero, which is undefined behaviour in C and
in practice returns whatever the host's shift instruction does with a count
of 32. On x86 that is a shift by 0, which happens to give the right answer,
so the bug survives every test run on the machine where it was written.

```c
static uint32_t
clz32(uint32_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return v ? (uint32_t)__builtin_clz(v) : 32;
#else
    /* portable loop */
#endif
}
```

`clz`, `ctz` and `cpop` are compiler builtins for a reason. On any host with
the instruction they become one machine instruction, and in WebAssembly they
are the single opcodes `i32.clz`, `i32.ctz` and `i32.popcnt`. This is the
one place where the RISC-V and WebAssembly instruction sets line up exactly,
which is a pleasant reversal of the [floating-point
situation](/rv32-emulator/) where they line up almost nowhere.

The third is the counting behaviour at zero. `clz` and `ctz` of a word with
no set bits are both 32, where the C builtins are undefined. The guard is
not defensive coding, it is the specification.

### Where the Test Goes

The obvious arrangement is to try the new decoders first:

```c
case OP_REG:
    if (cpu->bitmanip && bitmanip_reg(insn, f3, f7, a, b, &val)) {
        cpu->x[rd] = val;
        break;
    }
    /* ... the base integer set ... */
```

This is wrong for a reason that has nothing to do with correctness. It puts
a function call in front of every `add`, every `sub` and every shift the
guest executes, and those are the most common instructions in any program.

The alternative is to reach the new decoder only after the base decoder has
rejected the encoding. Each rejection site becomes a `goto` to a label at the
end of the function:

```c
        case 1:
            if (f7 != 0) goto zb_reg;
            cpu->x[rd] = a << (b & 0x1f);       /* sll */
            break;
```

Measured on the same base-ISA guest, mean of fifteen interleaved runs each:

| Interpreter | Throughput | vs published |
|---|---|---|
| Published, no bit manipulation | 33.15 MIPS | -- |
| New decoder tested first | 30.57 MIPS | **-7.8%** |
| New decoder in the rejection path | 32.39 MIPS | **-2.3%** |

A guest that uses none of the new instructions still pays 2.3%, which is the
cost of a larger function and worse instruction-cache behaviour rather than
of any test it executes. That is the price of the feature, and it is charged
whether the feature is used or not.

## Establishing Correctness

The previous article argued that for an emulator a reference model is not an
addition to the test suite, it is the test suite. Nothing about that changes
here, and this extension is unusually well suited to it.

### The Fuzzer Reaches All of It

The randomized fuzzer generates valid encodings, randomizes every register
and steps both models one instruction at a time. It deliberately generates
no control flow and no memory access, which is why it could never reach the
A extension and why the atomics needed 51 hand-written tests instead.

Bit manipulation is entirely inside what it can generate. All 29 encodings
were added to the generator, roughly 12% of the stream, and the operand
randomizer already produced the values that matter: zero, all ones, and the
sign bit.

```
102400 instructions compared, 32092 of them floating-point
no divergence found
```

A generator can silently produce nothing, so the claim was checked rather
than assumed. Running the reference with the extensions turned off should
kill it immediately, and does:

```
$ ./fuzz fuzz_target.elf -n 2 -cpu "rv32,zba=false,zbb=false,zbs=false"
gdb: guest stopped with signal 4
fuzz: remote stopped unexpectedly at round 0 instruction 4
  encoding 202b23b3  opcode=33 rd=7 rs1=22 rs2=2 funct3=2 funct7=10
```

Four instructions in, the reference takes an illegal-instruction signal on
`sh1add`. The generator is producing them.

### The Compliance Suite Has a B Directory

riscv-arch-test carries 32 tests under `rv32i_m/B`. Three of them are the
carry-less multiplies from Zbc, which is a separate extension that neither
Hazard3 nor this emulator implements, and they are skipped. The other 29 are
exactly one per instruction:

```
riscv-arch-test: 257 passed, 0 failed, 4 skipped, 11 did not build
```

The count was 228 before this work. Every one of the 29 new tests passes with
its signature region compared byte for byte against `qemu-system-riscv32`.

Assembling them needed nothing unusual, which is worth saying because the
last extension added to this emulator did. Zcmp could not be assembled by
GNU as at all, had to be built with clang and linked with GNU ld, and needed
a qemu CPU model spelled out instruction by instruction because `zcd=false`
is ignored while `c=true`. Zba, Zbb and Zbs assemble with binutils 2.42,
compile with both GCC 13 and clang, and qemu enables them with three
properties.

### A Guest the Compiler Filled In

Unit tests drive these instructions from hand-built encodings and the fuzzer
drives them from random ones. Neither covers instructions a compiler chose to
emit, inside real control flow, on values that came from earlier results.
`bitmanip_guest.c` is written so that GCC produces them: `__builtin_clz` and
friends, arrays of three different element widths for the shifted adds,
ternary comparisons for `min` and `max`, and inline assembly for the six
forms a compiler will not choose on its own.

It compiles to 20 of the 29 instructions and runs in lockstep against qemu,
comparing all 32 integer registers, the program counter, all 32
floating-point registers and `fcsr` after every step:

```
guest memory identical over [00010000,00014690)
6464 instructions in lockstep, no divergence
exit code 0 on both models
```

### Directed Tests for the Boundaries

Eighty unit tests cover what a random generator hits only by luck: a rotate
by zero, a bit index of 31 and one of 32 that has to wrap, `clz` and `ctz` of
a word with no set bits, `cpop` of all ones, `min` and `max` at `INT_MIN`,
and `orc.b` of a word whose only set bit is the top one. Expected values are
written as constants rather than computed, so a test agrees with the emulator
only when both agree with the specification.

Two of them are about the switch rather than the instructions. Every one of
the 29 encodings has to raise an illegal-instruction trap when the extensions
are turned off, so that an embedding can still offer a plain RV32IMAFC
machine, and `misa` has to advertise the B bit only when all three are
present.

### No Bugs

The fuzzer found two real bugs when this interpreter was first written. It
found none here. That is the expected outcome for 29 instructions with no
state and no memory access, and it is the argument for adding this kind of
extension rather than a harder one: the verification story is complete before
the work starts.

## What It Buys

Every guest below is the same source, built twice, differing only in
`-march`, and run to completion on the same emulator.

| Guest | Instructions, base | With Zba, Zbb, Zbs | Change | `.text` change |
|---|---|---|---|---|
| test_program | 3,432 | 3,432 | 0.00% | 0.00% |
| bench | 612,030 | 552,029 | **-9.80%** | -0.73% |
| CoreMark | 31,362,052 | 30,414,094 | **-3.02%** | -1.14% |
| Lua 5.4.7 | 15,252,142 | 14,974,101 | **-1.82%** | -0.22% |

The first row is the useful control. `test_program.c` computes Fibonacci
numbers, a greatest common divisor, a sum and a square root, and the compiler
found nothing at all to use. An extension that helps some code helps no other
code, and a benchmark suite that only contains the former is measuring the
author's taste in benchmarks.

### The Gain Is Concentrated, Not Spread

The benchmark runs four workloads, and building it one phase at a time shows
where the 9.8% came from:

| Phase | Base | With Zba, Zbb, Zbs | Change |
|---|---|---|---|
| Recursive Fibonacci | 299,202 | 299,202 | 0.00% |
| gcd and divide | 16,826 | 16,826 | 0.00% |
| Bit mixing | 220,024 | 160,023 | **-27.27%** |
| Floating point | 76,025 | 76,025 | 0.00% |

All of it is in one phase, and that phase is the one whose name says it
manipulates bits. The saving is exactly three instructions per iteration
across 20,000 iterations, and the disassembly says where they went. The loop
body goes from eleven instructions to eight:

```
  h = (h << 13) | (h >> 19);      slli + srli + or    ->  rol       -2
  h += h << 3;                    slli + add          ->  sh3add    -1
  h &= 0x7fffffff;                and                 ->  bclri      0
```

The mask is the interesting non-result. It was already one instruction,
because the compiler hoisted the constant into a register before the loop, so
`bclri` saves nothing inside the loop and only removes the two instructions
that built the constant once. This is what a compiler does with these
extensions when it is given code they fit. It is also the honest shape of the result: the extensions do not make
programs faster in general, they make a particular kind of code shorter, and
how much of your program is that kind of code is a question about your
program.

### Where CoreMark and Lua Spend It

A static count of what the compiler emitted:

| Instruction | CoreMark | Lua |
|---|---|---|
| `sh2add` | 29 | 141 |
| `sh3add` | 1 | 73 |
| `sh1add` | 11 | 64 |
| `zext.h` | 45 | 5 |
| `sext.h` | 20 | 5 |
| `bset`, `bext` and the other single-bit forms | 3 | 59 |
| `min`, `max` and their unsigned forms | 3 | 25 |

The shifted adds dominate both, which is the expected result: they are the
extension that helps ordinary code rather than clever code. CoreMark's
unusual `zext.h` count follows from its list workload, which stores its data
in 16-bit fields, so every load of one is a zero extension. Lua's single-bit
instructions track the bit-field flags in its object headers.

One limitation is worth stating rather than hiding. Lua is linked against a
prebuilt picolibc, and the multilib that is selected is `rv32imafc/ilp32f`,
which was compiled without these extensions. Every `strlen`, `memcpy` and
`memchr` Lua calls is therefore the ordinary version, and none of them get
`orc.b`. The 1.8% is what Lua's own code gained. A system that rebuilt its C
library too would do better, and the difference between those two numbers is
exactly the argument for a distribution shipping a bit-manipulation multilib.

### Instructions Are Not Time

Here the emulator is a more interesting machine to measure than a real chip
would be.

| Workload | Instructions | Wall clock |
|---|---|---|
| bench | -9.80% | **-2.6%** |
| CoreMark | -3.02% | **-1.9%** |

On silicon, `rol` and the shift-shift-or triple it replaces take one cycle
and three, so the instruction count is roughly the answer. In an interpreter
the cost of an instruction is the cost of decoding and dispatching it, which
is almost the same for all of them, and the new instructions are slightly
more expensive than the average because they are reached only after the base
decoder rejects the encoding and then go through a helper function.

Measured on the benchmark, the base build ran 612,030 instructions at 32.2
MIPS and the bit-manipulation build ran 552,029 at 29.8 MIPS. Nine point
eight percent fewer instructions, each about 8% more expensive, leaving 2.6%.

This is not an argument against the extensions. It is an argument about which
number to quote, and it points at the case where the instruction count is the
number that matters: the browser demonstration runs the guest in slices with
an instruction budget so that a runaway program cannot freeze the page. A
guest that needs 9.8% fewer instructions to finish its work gets 9.8% more
work done per slice, whatever the wall clock says.

## What It Costs

| | Published | With Zba, Zbb, Zbs | Change |
|---|---|---|---|
| `rv32.c` | 1,998 lines | 2,267 lines | +13.5% |
| WebAssembly module | 19,394 B | 20,410 B | +5.2% |
| Throughput, base-ISA guest | 33.15 MIPS | 32.39 MIPS | -2.3% |
| `sizeof(rv_cpu)` | 4,536 B | 4,536 B | 0.00% |

The struct did not grow at all. The one new `int` landed in padding that was
already there.

The 269 lines are the whole of it: two decoders, six helper functions, one
switch in the CPU struct, one bit in `misa`. There is no new state to save,
nothing to reset, and nothing an embedding has to configure. Turning the
switch off gives back a machine that traps on all 29 encodings and behaves
exactly as the published version does.

Both source files are in the download so the two can be compared and built
side by side. `rv32-orig.c` is the published interpreter, unchanged, and
`rv32.c` is the current one. The `bench_rv32_orig` target links the first and
`bench_rv32` links the second, which is how the throughput row above was
measured.

## Conclusion

The RP2350's extension set is a reasonable guide for a small RV32 engine, and
the three worth taking from it are the three that make up the ratified B
extension. They are 29 instructions with no architectural state, they are
fully reachable by a fuzzer that compares against a reference model, the
compliance suite has a directory for them, and no part of the toolchain
argues.

What they are worth depends on the code. A benchmark whose inner loop rotates
and masks a hash gets 27% of its instructions back. CoreMark gets 3% and Lua
gets 1.8%, both almost entirely through the shifted adds that turn array
indexing into one instruction. A program that computes Fibonacci numbers and
square roots gets nothing, and it is worth owning that result rather than
choosing different benchmarks.

The cost is 269 lines, 1 KB of WebAssembly and 2.3% of interpreter throughput
charged to every guest whether it uses the extensions or not. The last of
those three was the only surprise, and it was almost three times larger
before the new decoder was moved out of the path that every `add` takes.

The next candidate from the RP2350's list is Zcb, which adds eleven
compressed encodings and would shrink guest images rather than shorten
instruction streams. That is a different measurement with a different unit,
and this emulator already has the harness to make it. It was made in
[Zcb: The Same Instructions in Less Space](/rv32-zcb/), which found 2.31% of
CoreMark's image, no change at all in what it executes, and the same
decoder-placement mistake waiting one layer further down.

## Source

The emulator, both versions of the interpreter, the unit tests, the fuzzer
extension and the A/B benchmark targets are in the
[companion download for the previous article](/rv32-emulator/rv32-emulator-source.zip),
which is the single copy of this code that the whole project builds against.
The setup guide is in the demo
[README.md](/rv32-emulator/demo/README.md)
([HTML](/rv32-emulator/demo/README.html)).

```sh
make check                 # unit tests, including 80 for Zba, Zbb and Zbs
make lockstep-bitmanip     # a compiled guest against qemu, step by step
make fuzz-run              # randomized encodings, 12% of them Zba/Zbb/Zbs
make archtest              # the compliance suite, including the B directory
make bitmanip-compare      # the instruction-count table above
make bitmanip-overhead     # the throughput cost of the wider decoder
```
