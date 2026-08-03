---
title: "A Compact RV32 Engine, Verified Against QEMU"
date: 2026-08-01
revised: 2026-08-03
abstract: "Building a 32-bit RISC-V interpreter for embedding in games, and proving it correct by comparing every instruction against QEMU"
category: systems
---

## Introduction

The [ColdFire V4e emulator](/coldfire-emulator/) built in an earlier article
answered a specific question: which real CPU architecture is cheapest to
emulate while still having a working GCC behind it. The answer was
Motorola's ColdFire, at 2,641 lines of C.

That emulator has a problem it cannot fix. It is big-endian, and the place
this kind of engine is most useful now is inside a browser, where
WebAssembly is little-endian and every guest memory access needs a byte
swap. It is also a coprocessor-era design, so its floating-point
instructions live in a scattered corner of the encoding space rather than
under one opcode.

This article builds the same class of engine for RV32, and then spends most
of its effort on a harder question than "does it work": how do you establish
that a brand new emulator is correct, to a standard that lets you compare it
fairly against one that has been in use for months?

The answer turned out to matter. Two real bugs survived a compiled test
program that passed every check, and were caught only by comparing against a
reference implementation instruction by instruction.

## Abstract

We implement an embeddable RV32IMAFC_Zicsr_Zifencei interpreter in 1,998
lines of C, extended with the Zcmp whole-frame push and pop instructions.
Correctness is established by four independent means: lockstep
co-simulation against `qemu-riscv32` comparing all 32 integer registers, the
program counter, 32 floating-point registers and `fcsr` after every
instruction; randomized instruction fuzzing over 2.4 million instructions
with deliberately awkward operands; the RISC-V International compliance
suite, 228 tests passing with signatures compared against
`qemu-system-riscv32`; and 108 hand-derived IEEE-754 conformance tests. The
fuzzer found two bugs the compiled tests missed. Against the ColdFire V4e
emulator running the same C source, the RV32 engine is 24% smaller, executes
19% fewer instructions, and runs 1.6 times faster in wall-clock terms. It
builds unchanged to a 19 KB freestanding WebAssembly module with no
imports.

## Choosing the Target

The starting point was a set of notes arguing for RISC-V over the
alternatives for in-browser scripting. Several of those claims are testable,
so rather than restate them, this article checks them.

The uncontroversial parts hold up. RV32 is little-endian, so it maps onto
WebAssembly's memory without byte swapping. Its opcode field is always in
the same seven bits. Its floating-point instructions live under a single
primary opcode, `0x53`, which a decoder can route to one tight secondary
switch, where ColdFire reaches its FPU through line-F coprocessor
encodings.

Two claims did not survive contact with measurement, and both are discussed
where the evidence appears: that implementing the compressed extension would
destroy interpreter performance, and that RISC-V floating point maps one to
one onto WebAssembly's native floats at zero cost.

## The Shape of the Interpreter

The public interface mirrors the ColdFire emulator deliberately, so that the
two can be compared without the harness getting in the way. A CPU is a
struct, memory is six callbacks, and nothing is allocated.

```c
typedef struct rv_cpu {
    uint32_t x[32];         /* x0 is hardwired to zero */
    uint32_t pc;
    uint32_t f[32];         /* single-precision, raw bit patterns */
    uint32_t fcsr;
    /* machine-mode control registers, the reservation an lr.w holds,
       extension switches, a trace ring and the bus callbacks */
} rv_cpu;

void rv_init(rv_cpu *cpu,
             rv_read_fn r8, rv_read_fn r16, rv_read_fn r32,
             rv_write_fn w8, rv_write_fn w16, rv_write_fn w32,
             void *bus_ctx);
int  rv_step(rv_cpu *cpu);
```

Two additions matter for the intended use. An optional access-check callback
lets an embedding refuse any address outside the guest's sandbox, covering
instruction fetch as well as loads and stores. And `ecall` is routed to a
host callback before the trap is taken, which is the foreign-function
interface: a guest puts a service number in `a7`, arguments in `a0` onwards,
and executes one instruction.

The compressed extension is handled by expanding each 16-bit encoding into
the 32-bit instruction it stands for, then executing that. This keeps the
main decoder unaware that the C extension exists, at the cost of one
function that is mostly bit shuffling.

### Floating Point Without a Floating-Point Environment

The notes claimed that RISC-V floating point maps one to one onto
WebAssembly's `f32` operations, so an interpreter can pass the bits straight
through to native instructions. This is the one claim that is substantially
wrong, and it shapes the whole implementation.

RV32F specifies five rounding modes, selected per instruction or from a
control register. It requires five sticky exception flags. It requires that
every NaN produced by an arithmetic operation be the canonical quiet NaN,
discarding whatever payload the inputs carried. WebAssembly's float
operations offer round-to-nearest-even only, expose no exception flags, and
propagate NaN payloads. Three of the four requirements have nowhere to land.

The usual escape is `<fenv.h>`, setting the host rounding mode before each
operation. That fails here too: WebAssembly has no rounding-mode control at
all, and on a native host it makes results depend on whatever the embedding
process last left in the control register.

So this interpreter does its own rounding. Every single-precision result is
produced by computing an exact intermediate in double precision, then
rounding that to single precision in software:

- Addition uses an error-free two-sum, giving the exact result as a pair of
  doubles.
- Multiplication of two floats is exact in double precision, needing at most
  48 bits.
- Division and square root are correctly rounded in double, with a remainder
  computed by Dekker's exact product to determine which side of the rounded
  value the true result falls on.

One routine then rounds that exact pair to single precision under any of the
five modes and computes the flags. It also decides tininess after rounding,
as the specification requires, which is the difference between a result that
underflows and one that rounds up into the normal range.

The consequence is that `rv32.c` needs no math library and no `<fenv.h>`.
The same source builds for the host and for WebAssembly, and produces
identical results on both.

## Establishing Correctness

A compiled test program that prints "all tests passed" is close to
worthless as evidence. It exercises the instructions a compiler chose to
emit, with the operand values that program happened to produce. The
interesting failures are elsewhere.

Four independent methods carry most of the weight, in increasing order of
what they can catch. Directed tests fill the gaps each one leaves.

### Lockstep Co-simulation

The strongest tool loads the same ELF into the interpreter and into
`qemu-riscv32` under its GDB stub, then advances both one instruction at a
time. After every step it compares all 32 integer registers, the program
counter, the 32 floating-point registers and `fcsr`, and at the end compares
guest memory byte for byte.

```
guest memory identical over [00010000,00014690)
3433 instructions in lockstep, no divergence
exit code 0 on both models
```

The value is not the pass. It is that a failure names the instruction that
caused it, rather than a wrong number printed thousands of instructions
later.

Making this work required more care than expected. QEMU does not put the
floating-point registers in the bulk register-read packet, and `fcsr` is not
where a naive reading of the GDB register numbering would put it: the
floating-point file, vector registers and control registers are separate
features, so `fcsr`'s number depends on the CPU model. The client therefore
parses the target description and looks registers up by name. Reading 35
floating-point registers costs a round trip each, so the comparison only
pays for them after an instruction that could have changed them.

There is one thing lockstep cannot do. At roughly 1,500 instructions per
second, dominated by protocol round trips, it is a correctness tool and not
a way to validate long-running programs.

### Randomized Instruction Fuzzing

Compiled code never generates a signalling NaN, rarely divides the most
negative integer by minus one, and never asks for round-toward-zero. The
fuzzer fills a buffer with random valid encodings, randomizes every
architectural register with a mix of uniform bit patterns and deliberately
awkward values -- signalling NaNs, subnormals, exact rounding midpoints,
values either side of a binade boundary, the signed overflow operand -- and
steps both models.

It generates no control flow and no memory access. Both are covered
elsewhere, and excluding them keeps the program counter advancing linearly
so that a random encoding cannot wander into unmapped memory and end the
run.

Two bugs turned up, neither reachable from the compiled tests.

**A rounding error at binade boundaries.** A fused multiply-add whose exact
result fell just below a power of two, rounded toward zero, produced a value
one unit in the last place too low. The decomposition into mantissa and
exponent was done at the wrong binade: below a power of two the spacing
halves, so subtracting one step in the higher binade takes two steps in the
lower one. Found after 198 instructions of the first run.

**An exception flag being cleared.** A floating-point to integer conversion
that saturates raises the invalid flag and not the inexact flag. The
implementation set inexact first and then cleared it, which also cleared any
inexact flag accrued by earlier instructions. The flags are sticky, so
clearing one is a visible architectural error.

Both are small. Both would have shipped. Neither is the kind of thing a
test written by the same person who wrote the implementation tends to catch,
which is the argument for differential testing against something written by
someone else.

Four campaigns of 614,400 instructions each, from different seeds, found
nothing further. The last of them ran against the finished interpreter,
with the atomics and the whole-frame push and pop in place.

### The Compliance Suite

The RISC-V International architecture tests are the external standard. They
write results into a signature region which is compared against a reference
model.

The suite ships no reference signatures, so the runner produces them: the
target port's halt routine dumps the signature as hex over the board's
serial port, and the same binary is run on `qemu-system-riscv32` to obtain
the same text.

Dumping from inside the guest rather than reading memory through a debugger
turned out to be necessary. The first version set a breakpoint at the halt
address and read memory over the GDB stub, which works until the test being
run is the one for `ebreak` -- QEMU's stub claims the guest's own breakpoint
instruction instead of delivering it as a trap, so the signature was captured
mid-test. Attaching a debugger changed the thing being measured.

```
riscv-arch-test: 228 passed, 0 failed, 1 skipped, 11 did not build
```

The eleven that do not build are Zcb instructions, outside what this
interpreter implements. The one skipped test stores `mstatus`, a register
whose readable bits depend on which privilege modes exist; QEMU's board has
supervisor and user mode where this engine has machine mode only, so the two
read back different but equally legal values. It is not comparable between
them, which is a different thing from failing.

Getting the suite to build at all needed the older framework branch. The
current one relies on the assembler padding `.p2align` with no-ops while
relaxation is disabled; binutils 2.42 fills it with zeros instead, which
makes the entry sequence fall into a run of illegal instructions. Those
binaries hang on any model, QEMU included.

### Conformance Against the Specification

Differential testing has a blind spot: it cannot catch an error shared with
the reference. So 108 tests encode expected results worked out by hand from
the IEEE-754 and RISC-V rules -- all five rounding modes at exact midpoints
and either side of them, overflow delivering different results per mode,
tininess detected after rounding, NaN canonicalization, the ordering of
negative and positive zero under minimum and maximum, saturating conversions,
and the ten-way classification.

These also reach what the fuzzer cannot generate. A reserved rounding mode
must raise an illegal instruction; asking QEMU to execute one ends the
reference process rather than producing a comparable result.

### What the Layers Cost

| Method | Scale | Catches |
|---|---|---|
| Compiled test program | 3,432 instructions | gross errors only |
| IEEE-754 conformance | 108 tests | errors shared with the reference |
| Memory and sandbox tests | 30 tests | alignment, access checks |
| Atomic tests | 51 tests | reservations, alignment, arithmetic |
| Compliance suite | 228 tests | externally defined coverage |
| Lockstep | 3,433 instructions | anything, localized exactly |
| Fuzzing | 2,457,600 instructions | rare operands and encodings |

Line coverage of the interpreter is 86.3%. The remainder is defensive:
double-fault handling, vectored trap vectors, and range-extension branches
in the math helpers that single-precision operands cannot reach.

## Comparison With the ColdFire Emulator

Both emulators run `bench.c`, cross-compiled from one source for each
target. Both produce identical results, which is itself a cross-architecture
check on the whole stack.

### Size

| | rv32 | coldfire |
|---|---|---|
| Implementation | 1,998 lines | 2,641 lines |
| Header | 318 lines | 250 lines |
| Architectural state | 432 bytes | 320 bytes |

The RV32 interpreter is 24% smaller while implementing four extensions plus
Zicsr, Zifencei and Zcmp. The state is slightly larger because RISC-V has 32
integer and 32 floating-point registers where ColdFire has eight of each,
which is also why it needs fewer instructions to do the same work.

### Code Density

The notes claimed that implementing the compressed extension means writing a
variable-length decoder and destroying interpreter performance. Both halves
are worth checking.

| Target | `.text` | vs ColdFire |
|---|---|---|
| RV32IMFC | 914 B | -12% |
| ColdFire V4e | 1,038 B | -- |
| RV32IMF | 1,420 B | +37% |

Compressed RV32 is denser than ColdFire's variable-length encoding, which is
the opposite of the usual expectation about RISC versus CISC. And the cost
of decoding it is not what was claimed: the compressed and uncompressed
builds retire *identical* instruction counts, because compressed forms are
one-to-one substitutions, and the compressed build measured slightly faster
across five trials. The smaller guest image behaves better in the host's
caches than the expansion step costs.

### Instructions and Throughput

| Phase | rv32imfc | coldfire | ratio |
|---|---|---|---|
| Recursive Fibonacci | 299,202 | 265,402 | 1.13 |
| gcd and divide | 16,826 | 24,411 | 0.69 |
| Bit mixing | 220,024 | 340,010 | 0.65 |
| Floating point | 76,025 | 126,017 | 0.60 |
| **Total** | **612,030** | **755,823** | **0.81** |

ColdFire wins exactly one category, and it is the one its instruction set was
built for: `movem` saves and restores a register list in a single
instruction, where RISC-V emits one store per register. Everywhere else the
three-operand encodings win, most decisively in floating point, where
ColdFire's two-operand FPU needs extra moves.

Throughput, median of seven runs of fifty repetitions: **32.5 MIPS** for
rv32 against **25.7 MIPS** for ColdFire, a ratio of 1.26. Combined with the
lower instruction count, the same work takes 18.8 ms instead of 29.4 ms,
**1.6 times faster overall**.

Some of that gap is the byte swapping the notes predicted. A big-endian
guest on a little-endian host pays for it on every fetch and every data
access.

## Closing the Recursion Gap

ColdFire's one win has a direct answer in the RISC-V ecosystem. The Zcmp
extension, part of the ratified code-size reduction work, adds `cm.push`,
`cm.pop`, `cm.popret` and `cm.popretz`: whole-frame save and restore in one
16-bit encoding, with the stack adjustment folded in, and the return folded
in as well on the popping forms.

A five-instruction prologue becomes one instruction:

```
rv32imfc                        rv32imfc_zcmp
  addi sp,sp,-16                  cm.push {ra, s0-s2}, -16
  sw   ra,12(sp)
  sw   s0,8(sp)
  sw   s1,4(sp)
  sw   s2,0(sp)
```

Implementing it took 161 lines. The frame layout was determined by running
the instructions on QEMU and dumping the resulting frame rather than from
prose, which was the right call: the registers sit at the top of the frame
with padding beneath them, and `ra` occupies the *lowest* of the register
slots with the saved registers ascending above it, which is the opposite of
what the obvious reading suggests.

Measured within one compiler, since GNU `as` cannot yet assemble these:

| | rv32imfc | +Zcmp | |
|---|---|---|---|
| Recursion phase | 400,606 | 241,203 | **-39.8%** |
| Full benchmark | 711,029 | 551,626 | -22.4% |
| `.text` | 372 B | 346 B | -7.0% |

There is a constraint worth knowing. Zcmp occupies the encoding space of the
compressed double-precision loads and stores, so an implementation may have
Zcmp or Zcd but never both. That is free here, because this engine has no
double precision at all. An engine that needed double precision would have
to give up the 16-bit forms of double load and store to gain whole-frame
push and pop -- which, for code that mostly calls functions, is a trade
worth making.

## Atomics as a Convenience

The A extension was added last, and for a reason that has nothing to do
with concurrency. Without it, a guest that uses C11 `_Atomic`, the
`__atomic_*` builtins or C++ `std::atomic` does not fail slowly or run
incorrectly. It fails to link:

```
rv32imfc    undefined reference to `__atomic_fetch_add_4'
rv32imafc   links
```

The compiler emits a call into a runtime library that is not built for
this target, for the same reason there is no 32-bit libgcc. That single
line is what stands between the engine and a large class of ordinary
software, so eleven instructions buy a lot.

What they do not buy is concurrency. This is a single hart with no
interrupts. Nothing can interleave with a read-modify-write, so each
atomic is a load, an operation and a store, and the acquire and release
bits order accesses that no other agent can observe. They are a
compatibility feature, and the demo README says so.

Three things still had to be right. The arithmetic and the returned value,
which is the value from before the update. The reservation that `lr.w` and
`sc.w` carry, including that a store conditional fails without one, fails
against a different address, fails on a second use, and is dropped by any
trap so a handler cannot complete a sequence begun by the code it
interrupted. And alignment: an atomic must be naturally aligned whatever
the setting for ordinary misaligned access, because the guarantee cannot
be offered on an operand split across two words.

The test story turned out better than expected. The compliance suite ships
nine atomic tests on the branch that builds here, and all nine pass with
signatures matching `qemu-system-riscv32`. Fifty-one directed tests cover
the reservation and alignment rules, and lockstep covers all eleven forms
including the signed and unsigned minimum and maximum, which the compiler
never emitted on its own. The one gap is the fuzzer, which excludes memory
access by design and so cannot reach them.

## Running Real Software

The question a new emulator has to answer eventually is whether real
software runs on it, as opposed to programs written alongside it. Two
programs answer it here, and neither was written for this project.

### CoreMark

CoreMark is third-party, it is not small, and it validates its own results
by checksumming each workload.

It runs. A hundred-line board port supplying a clock, a character sink and
the seed variables was all it needed:

```
2K performance run parameters for coremark.
CoreMark Size    : 666
Total ticks      : 31334283
Iterations       : 100
seedcrc          : 0xe9f5
[0]crclist       : 0xe714
[0]crcmatrix     : 0x1fd7
[0]crcstate      : 0x8e3a
[0]crcfinal      : 0x988c
Correct operation validated.
```

Thirty-one million instructions, and every checksum matches both the values
CoreMark documents as correct and the same binary run under
`qemu-riscv32`.

The iterations-per-second figure is not a CoreMark score. The clock in the
port reads the retired-instruction counter, so a second here is a million
instructions rather than a unit of time.

### Lua

CoreMark is a benchmark, and benchmarks are gentle. It brings its own
formatted output and allocates from the stack, so it never asks for a C
library. Lua asks for all of one. It allocates constantly, uses setjmp and
longjmp for error handling, formats floating-point numbers, and can be
asked to check its own answers.

Lua 5.4.7 runs unmodified. Twenty-seven checks covering integer and float
arithmetic, closures, tables and sorting, string patterns and formatting,
metatables, coroutines, error handling, garbage collection and recursion:

```
lua Lua 5.4  integer max 2147483647  1e300 -> 1e+300
ok   closure upvalue        3
ok   gsub                   hell0 w0rld
ok   metatable __add        11,22
ok   coroutine resume       14
ok   pcall caught           false
ok   gc reclaims            true
ok   fib(20)                6765
all lua checks passed

15252142 instructions retired, 954 syscalls, exit code 0
```

The same binary produces the same output under `qemu-riscv32`.

The interesting part is what this build does to the arithmetic. Lua gets
32-bit integers and IEEE double floats here, and RV32F is single precision
only, so every Lua float operation runs in software through libgcc. Lua
leans on the integer core and the M extension far more than on the F
extension, which is close to the opposite of what the particle demo below
exercises.

The obstacle was the C library, not the instruction set. A stock Ubuntu
cross toolchain has no 32-bit RISC-V libc at all, so until one is
installed a program calling `printf` or `malloc` does not link. With
picolibc the port is one C file and one assembly file: `sbrk` over a
linked region, the three standard streams, and clean refusals for the
filesystem and clock calls that Lua's os and io libraries reach for.

The syscall layer is the remaining limit. It provides `write` and `exit`
and answers everything else with `-ENOSYS`, which is enough for a program
that computes and prints, and not enough for one that wants real files.

One detail is worth recording because the emulator hid it. The first
linker script placed the heap and stack past the end of the image by
moving the location counter, which left them outside any loadable segment.
The flat memory model here did not care and Lua ran. qemu maps only what
the program headers declare, and segfaulted. Reserving them as real
sections fixed it, and only then could the two models be compared at all.

## Embedding in WebAssembly

The interpreter builds to a freestanding `wasm32` module with **no
imports**:

```
rv32.wasm  19,389 bytes
guest.bin     764 bytes
```

Nothing is imported because nothing needs to be. The guest's memory is an
array inside the module's own linear memory, reached through the same
callbacks a native embedding uses. The rounding work described earlier is
what makes this possible: an implementation that depended on `<fenv.h>`
could not be built this way at all.

[Run the demonstration page](wasm/). It is a single file. The build
base64-embeds both the module and the guest image into the HTML, so the page
carries no separate fetches, needs no server, and opens just as well from a
local filesystem as from this site.

The demonstration page runs a particle simulation written in C and compiled
for RV32. The physics, the collisions and the square roots that size each
dot all execute inside the interpreter. The page never computes a position;
it only draws what the guest asks it to draw through `ecall`, one call per
particle and one per frame to yield.

Gravity eventually wins, and a demonstration that has gone still is not
showing anything, so once three quarters of the particles have come to rest
on the floor the simulation scatters them again. That decision is the
guest's, not the page's. It counts its own particles and calls its own
initialiser, and the page learns about it only by being handed a different
set of coordinates to draw.

A per-frame instruction budget is what makes this safe for untrusted
scripts. A guest that loops forever is simply cut off at the end of its
slice and the frame is drawn anyway. Combined with the access-check callback,
a misbehaving script cannot freeze the page or reach memory it was not
given.

## Conclusion

An RV32 engine is a better fit than a ColdFire engine for embedding in a
browser-hosted game, and the margin is larger than the endianness argument
alone suggests: 24% less code to maintain, 19% fewer instructions for the
same work, 1.6 times faster on the same workload, and denser guest binaries
once the compressed extension is included. Two claims from the design notes
that argued against the compressed extension and for a free mapping onto
WebAssembly floats were both wrong, and measuring them was cheap.

The more transferable result is about verification. The compiled test
program passed from the first run and would have shipped two architectural
bugs. Both were caught by comparing against an independent implementation at
instruction granularity, and one of them only with operand values no
compiler would emit. For an emulator, a reference model is not a nice
addition to a test suite. It is the test suite, and the hand-written tests
exist mainly to cover what the reference cannot be asked.

Real software runs on it. CoreMark and Lua both execute unmodified and
validate their own results, which is a different and more convincing claim
than passing a test suite written alongside the emulator. Neither needed
anything from the interpreter that was not already there; what they needed
was a C library and a board port, and the board ports are a hundred lines
each.

What this engine is not is an operating-system host. It has machine mode
only, no supervisor mode and no MMU. Running Linux would need all of those
plus an interrupt controller and timers, which is a project comparable in
size to everything described here. It would also need a different
comparison method: lockstep at 1,500 instructions per second cannot
validate a kernel boot, and the applications above are already checked by
final state rather than instruction by instruction.

## Source

The complete emulator, the verification tools, the benchmark, the board
ports for CoreMark and Lua, and the WebAssembly demonstration are in the
companion download.

The demo README has a step-by-step setup guide, covering the packages
needed, what each `make` target does, and how to point the differential
tools at a reference model:
[README.md](demo/README.md) ([HTML](demo/README.html)).

```sh
make            # build the interpreter, the tools and the guests
make check      # the suites that need no reference model
make lockstep-run    # compare against qemu, instruction by instruction
make fuzz-run        # randomized encodings and operands
make bench-compare   # against the ColdFire V4e emulator
make wasm            # the browser demonstration
```
