---
title: "Zcb: The Same Instructions in Less Space"
date: 2026-08-04
abstract: "Eleven compressed encodings that shrink a guest image without changing what it executes, and the eleven compliance tests that finally build"
category: systems
---

## Introduction

The [previous article](/rv32-bitmanip/) added Zba, Zbb and Zbs to this RV32
interpreter and measured what they were worth in retired instructions. It
ended by naming the next extension on the RP2350's list and predicting what
kind of measurement it would need:

> The next candidate from the RP2350's list is Zcb, which adds eleven
> compressed encodings and would shrink guest images rather than shorten
> instruction streams. That is a different measurement with a different
> unit, and this emulator already has the harness to make it.

This article makes that measurement. The prediction held: guest images
shrink by up to 2.3% and the number of instructions executed does not move
at all. Two things were not predicted. The compliance suite had been
carrying eleven Zcb tests that never built since the first article was
written, and they now all pass, which takes the suite to 268 tests with
nothing failing and nothing unbuilt for the first time. And the same
mistake about where to put a decoder test, which cost 7.8% of throughput
last time, was available to make again in a different place. It was, and it
cost 5%.

## Abstract

Zcb is implemented in 84 lines of C, taking the interpreter from 2,267 to
2,351 lines and its WebAssembly module from 20,410 to 20,943 bytes. All
eleven RV32 encodings expand one-to-one into instructions the emulator
already had, so unlike Zcmp the extension needs no new execution path.
Correctness rests on three things: 69 directed tests, of which 13 compare
the expander against what GNU as produces for the uncompressed form; the
eleven Zcb tests in riscv-arch-test's `C` directory, which previously did
not build and now pass with signatures compared against
`qemu-system-riscv32`, taking the suite from 257 passing with 11 unbuilt to
268 passing with none; and a compiled guest carrying all eleven forms that
runs 8,970 instructions in lockstep against `qemu-riscv32` with no
divergence. The randomized fuzzer can only reach six of the eleven, because
five are loads and stores and it generates no memory access. Rebuilding
four guests changes `.text` by -2.31% on CoreMark, -0.51% on Lua and 0% on
a benchmark with no byte or halfword traffic, while retired instruction
counts stay identical to within one in a hundred thousand. CoreMark runs
1.85% faster in wall-clock terms while executing the same instructions.

## What Zcb Is

Eleven encodings on RV32, in two groups.

| Group | Instructions |
|---|---|
| Loads and stores | `c.lbu`, `c.lhu`, `c.lh`, `c.sb`, `c.sh` |
| One-operand arithmetic | `c.zext.b`, `c.sext.b`, `c.zext.h`, `c.sext.h`, `c.not`, `c.mul` |

The first group fills a hole. The base compressed extension has `c.lw` and
`c.sw` and nothing narrower, so every byte and halfword access in a
compressed binary costs four bytes of code. Anything that walks a string, a
byte buffer or a packed structure pays that on every access. The second
group is six operations common enough to be worth two bytes each.

The restrictions are what make them fit in sixteen bits. Both registers
must be in `x8` to `x15`, the byte offset is two bits and the halfword
offset is one, and every one-operand form writes its answer back over its
input. `c.zext.w` exists but is RV64 only, which is why eleven and not
twelve.

Three of them require Zbb and one requires M. That is the specification's
own dependency and not a simplification made here: `c.sext.b`, `c.zext.h`
and `c.sext.h` are defined as the compressed forms of Zbb instructions.
Having added Zbb in the previous article, this emulator can offer all
eleven.

## Implementation

Every Zcb encoding stands for exactly one 32-bit instruction. That is the
whole extension, and it makes the implementation a single function:

```c
uint32_t rv_expand_zcb(uint16_t c);
```

It returns the 32-bit instruction, or 0 when the encoding is not a Zcb one.
Nothing else in the interpreter changed. There is no new execution path, no
new trap, and no new architectural state, because after expansion these are
`lbu`, `sh`, `andi`, `mul` and the Zbb instructions that were already
there.

This is worth contrasting with Zcmp, the other compressed extension this
emulator implements. `cm.push` and `cm.pop` build and tear down a whole
stack frame, which no single 32-bit instruction does, so they needed their
own execution function and their own place in `rv_step`. Zcb needed neither.

The dependency on Zbb comes out for free. `c.sext.b` expands into `sext.b`,
and if the `bitmanip` switch is off then `sext.b` raises the
illegal-instruction trap by itself. No separate check exists anywhere in
the emulator, and a test asserts that the trap happens:

```
the forms that expand into Zbb trap when Zbb is off
  ok   c.sext.b traps
  ok   c.zext.h traps
  ok   c.sext.h traps
  ok   c.zext.b still works               00000078
```

### The Offset Is Backwards

There is exactly one trap in the encoding, and it is a good one.

Every other compressed immediate in RISC-V is scrambled, but scrambled the
same way across the whole base extension: the bits appear in an order that
lets one hardware decoder serve several formats. Zcb's byte offset does not
follow it. In `c.lbu` and `c.sb`, bit 6 of the encoding is offset bit 0 and
bit 5 is offset bit 1, the opposite way round to the pattern `c.lw` uses.

An implementation that copies the `c.lw` code and narrows it reads the
right byte for offsets 0 and 3 and the wrong one for 1 and 2, so half the
obvious test cases pass. The directed tests walk all four:

```c
    run(0x8080, 0);                             /* c.lbu s0, 0(s1) */
    check("c.lbu offset 0", cpu.x[8], 0x11);
    run(0x8080 | 0x40, 0);                      /* c.lbu s0, 1(s1) */
    check("c.lbu offset 1", cpu.x[8], 0x82);
    run(0x8080 | 0x20, 0);                      /* c.lbu s0, 2(s1) */
    check("c.lbu offset 2", cpu.x[8], 0x33);
    run(0x8080 | 0x60, 0);                      /* c.lbu s0, 3(s1) */
    check("c.lbu offset 3", cpu.x[8], 0xf4);
```

### Which Expander Goes First

The previous article found that testing for Zba, Zbb and Zbs before the
base integer set cost 7.8% of interpreter throughput, because it put a
function call in front of every `add`. The same choice exists here, one
layer down, and it was made wrongly again:

```c
        insn = cpu->zcb ? rv_expand_zcb((uint16_t)lo) : 0;
        if (insn == 0)
            insn = rv_expand_c((uint16_t)lo);
```

Zcb occupies encodings the base compressed decoder rejects, so the two can
be tried in either order and only the cost differs. Asking Zcb first means
every `c.addi`, `c.li`, `c.mv` and `c.lw` in the program calls a function
that looks at three fields and returns 0.

| Order | Throughput on ordinary compressed code |
|---|---|
| Zcb first | **-5.1%** |
| Base compressed set first | no measurable change |

The rule generalises past this emulator: when an extension lives in another
one's reject path, the reject path is where its decoder belongs. The cost
of a test is paid by whatever runs most often, and an extension is by
definition not that.

## Establishing Correctness

### The Fuzzer Reaches Six of Eleven

The randomized fuzzer compares against `qemu-riscv32` instruction by
instruction, and it covered all 29 bit-manipulation instructions in the
previous article. Here it covers six.

The reason is a deliberate limitation described when the fuzzer was
written: it generates no memory access, because a random address in a
random encoding would end the run instead of testing anything. Five of
Zcb's eleven instructions are loads and stores. The six that remain,
`c.zext.b` through `c.mul`, were added to the generator and are checked the
usual way, including the check that they are really being produced:

```
$ ./fuzz fuzz_target.elf -n 1 -cpu "rv32,...,zcb=false"
gdb: guest stopped with signal 4
  encoding 9cf5 (compressed, expands to fff4c493)
```

That is `c.not s1`, and with Zcb turned off in the reference it takes an
illegal-instruction signal, which is what proves the generator emits it.

The other five needed a different tool, and this is the case the article
before last argued for: a verification story should be assembled from
methods whose blind spots do not overlap.

### The Compliance Suite Was Already Waiting

Since the first article, every compliance run this project has done has
ended with the same three words on it. The passing count moved from 228 to
257 when the bit-manipulation extensions arrived, but the tail did not:

```
riscv-arch-test: 257 passed, 0 failed, 4 skipped, 11 did not build
```

Those 11 were documented from the beginning as "Zcb instructions, which
this interpreter does not implement". They sit in the suite's `C` directory
rather than in one of their own, which is why there was no Zcb suite to
point at and no obvious place to notice them. Adding `_zcb` to the assembler's `-march` makes them
build, and they pass:

```
riscv-arch-test: 268 passed, 0 failed, 4 skipped, 0 did not build
```

Every test in every suite this emulator claims to implement now builds and
passes, with each one's signature region compared byte for byte against
`qemu-system-riscv32`. The four skipped are the three carry-less multiplies
from Zbc and the one `mstatus` test whose readable bits depend on which
privilege modes exist.

This is the strongest single piece of evidence in the article, and it cost
nothing to obtain, because the tests were written years ago by people with
no interest in this emulator.

### The Mapping, Against an Assembler

The whole content of Zcb is a mapping from sixteen bits to thirty-two, so
the mapping is tested directly. Both columns of the table came from GNU as:
one by assembling the compressed mnemonic, the other by assembling the
uncompressed instruction it is supposed to stand for.

```c
static const struct expansion expansions[] = {
    { "c.lbu s0, 0(s1)",    0x8080, 0x0004c403 },
    { "c.lbu a5, 3(a0)",    0x817c, 0x00354783 },
    { "c.lhu s0, 0(s1)",    0x8480, 0x0004d403 },
    ...
```

Neither number came from this emulator, so a test agrees with it only when
both agree with the assembler. Three further groups check that the reserved
encodings are refused rather than expanded into something plausible, that
the base compressed decoder claims none of this space, and that every
encoding traps when the switch is off.

### A Guest Full of Byte Traffic

`zcb_guest.c` covers what the fuzzer cannot. It is written the way the
assembler compresses: four-byte structures so that every field is at offset
0 to 3 from one base register, a byte buffer walked four at a time, signed
and unsigned halfword arrays whose values straddle 0x8000, and a string
loop. The five forms the compiler did not reach on its own are written as
inline assembly with named registers, because compression only happens when
both registers land in `x8` to `x15`.

All eleven forms appear in the binary, and it runs in lockstep against
qemu, comparing every register after every instruction:

```
guest memory identical over [00010000,00014770)
8970 instructions in lockstep, no divergence
exit code 0 on both models
```

## What It Buys

Every guest is the same source built twice, differing only in whether
`_zcb` is on the `-march` string. Both builds already have Zba, Zbb and Zbs,
so this isolates Zcb.

| Guest | `.text` with B | With B and Zcb | Change | Instructions |
|---|---|---|---|---|
| test_program | 1,650 | 1,646 | -0.24% | +0.0000% |
| bench | 1,086 | 1,086 | 0.00% | +0.0000% |
| CoreMark | 11,956 | 11,680 | **-2.31%** | +0.0003% |
| Lua 5.4.7 | 205,832 | 204,776 | **-0.51%** | +0.0010% |

The instruction column is the point. It is not approximately unchanged, it
is unchanged: a one-to-one substitution cannot alter what a program
executes. The two residuals are both explained and neither is the
extension. CoreMark prints its own `-march` string, which is four
characters longer in the Zcb build, so it makes four more write syscalls
and spends 96 more instructions printing. Lua's output is byte for byte
identical, and its 151 extra instructions out of 15 million come from the
image being a kilobyte smaller, which moves every heap address and changes
the allocator's behaviour by a hair.

### Where the Space Goes

Two bytes saved per compressed instruction, and a static count of how many
each program got:

| Instruction | CoreMark | Lua | Zcb guest |
|---|---|---|---|
| `c.lbu` | 30 | 241 | 12 |
| `c.zext.b` | 22 | 184 | 3 |
| `c.lh` | 28 | 0 | 1 |
| `c.sb` | 9 | 51 | 4 |
| `c.zext.h` | 23 | 5 | 1 |
| `c.sext.h` | 9 | 3 | 1 |
| `c.mul` | 8 | 9 | 1 |
| `c.sh`, `c.lhu`, `c.not`, `c.sext.b` | 11 | 11 | 7 |
| **Total** | **140** | **504** | **30** |

Lua is a byte-shovelling program and it shows: 241 `c.lbu` and 184
`c.zext.b`, almost all of it string and character handling. Its saving is
nonetheless the smaller of the two in percentage terms, because those 504
instructions are two bytes each out of 205 KB. CoreMark is a quarter the
number of instructions in a seventeenth of the code.

The counts also say something about the compiler that is easy to miss.
Compression is done by the assembler, not the compiler, and it happens
whenever the operands happen to fit. Nothing in the source is written for
it and no optimisation pass is looking for it. The 504 instructions in Lua
are 504 accidents of register allocation.

### Smaller Code Runs Faster, Slightly

CoreMark executes the same instructions in both builds. It does not take
the same time.

| | With B | With B and Zcb |
|---|---|---|
| Instructions | 30,414,094 | 30,414,190 |
| Wall clock | 0.974 s | 0.956 s |
| | | **-1.85%** |

The guest image is 276 bytes smaller, and the interpreter's fetch path
touches fewer host cache lines to do the same work. The first article found
exactly this when it measured the base compressed extension: identical
instruction counts, slightly faster in practice. It is a small effect
resting on a host cache, and it would be worth nothing on a machine whose
whole guest fits in cache anyway.

## What It Costs

| | With B | With B and Zcb | Change |
|---|---|---|---|
| `rv32.c` | 2,267 lines | 2,351 lines | +3.7% |
| WebAssembly module | 20,410 B | 20,943 B | +2.6% |
| Throughput, base-ISA guest | -2.3% vs published | -2.4% vs published | none measurable |

Eighty-four lines, one function, one switch in the CPU struct. Against the
published interpreter that this project started from, the emulator has now
grown by 353 lines and 1.5 KB of WebAssembly, and lost 2.4% of its
throughput on guests that use none of the new instructions.

That last number has not moved since the previous article, which is the
result the ordering work bought. Zcb's decoder runs only when the base
compressed decoder has already refused an encoding, so a program built
without it never calls the function at all.

## Conclusion

Zcb is the cheapest of the three extensions this project has added and the
easiest to verify. Eleven encodings, 84 lines, no new execution path, and a
compliance suite that was already carrying tests for it. Adding it took the
project to 268 compliance tests passing with none failing and none
unbuilt, which is a better position than the emulator has been in since it
was written.

What it buys is code density and nothing else, and the size of that depends
entirely on how much byte and halfword traffic a program has. CoreMark got
2.31% of its image back, Lua got 0.51%, and a benchmark doing word
arithmetic got nothing. The instruction count did not move, because it
cannot: every encoding is one instruction wearing a shorter name.

The transferable result is the one about decoder placement, and it is the
second time this project has run into it. An extension's decoder belongs in
the path that runs when the common case has already failed. Put it first
and it is a tax on every instruction that is not it. That cost 7.8% for the
bit-manipulation extensions and 5.1% here, both of them recovered in full
by moving one function call.

This is a good place to say what the RP2350 has been doing in these two
articles, because it is easy to mistake for a target. It supplied the
shortlist and nothing else. A team building a small in-order core for a
cost-sensitive part had already done the work of deciding which extensions
earn their gates, and borrowing that judgement was cheaper than forming it
from the specification. What each extension was then judged on here was a
different question: does a compiler emit it from ordinary C. Zba, Zbb, Zbs
and Zcb all do, which is why they were worth measuring and why the
measurements exist.

Zbkb is on the RP2350 and fails that test. Its five new RV32 instructions,
`pack`, `packh`, `brev8`, `zip` and `unzip`, are emitted by GCC from nothing
at all and by clang only for `pack`, so a guest rebuilt with the extension
is byte for byte the guest built without it. There is no article in an
empty table, and no reason for an engine that exists to run compiler output
to carry instructions no compiler produces.

The overlap that remains is still worth having, but it points one way. Code
can be prototyped on this emulator and then run on real Hazard3 silicon,
which is a useful thing for a project that mostly lives inside a browser.
Nothing requires the reverse: this is not an RP2350 emulator, and an RP2350
binary is not expected to run here.

Physical memory protection is the clearest case of the difference, and the
reason is not that this emulator's sandbox happens to be good enough. PMP
exists because a hart is a fixed resource. If two mutually distrusting
things have to run on one, the boundary between them must be drawn inside a
single address space and enforced on every access. An emulator has no such
constraint. A second protection domain is a second instance: 440 bytes of
architectural state, a diagnostic ring that can be trimmed to nothing, and
whatever memory buffer the embedding wants to give it. The interpreter has
no mutable state outside that struct, so instances share nothing: there is
no boundary to configure and none to get wrong.

That is isolation by construction rather than by enforcement, and the two
fail differently. A mistake in a PMP configuration leaks across a boundary
that was supposed to hold. Two instances have no common address space in
which such a mistake could be expressed. Where the arithmetic does favour
PMP is memory: it partitions one region where instances need one region
each. If that ever mattered, the answer would still not be PMP, but carving
a single allocation and giving each instance a base and a limit through the
access-check callback the emulator already has. That is the same job done
by the host, where a guest cannot reach the configuration registers and try
to widen its own sandbox.

## Source

The emulator, both versions of the interpreter, the unit tests, the
lockstep guest and the comparison targets are in the
[companion download for the first article](/rv32-emulator/rv32-emulator-source.zip),
which is the single copy of this code the whole project builds against. The
setup guide is in the demo
[README.md](/rv32-emulator/demo/README.md)
([HTML](/rv32-emulator/demo/README.html)).

```sh
make check              # unit tests, including 69 for Zcb
make lockstep-zcb       # the compiled guest against qemu, step by step
make archtest           # 268 passing, nothing failing, nothing unbuilt
make zcb-compare        # the code-size table above
make bitmanip-overhead  # what the wider decoder costs every guest
```
