---
title: "Interrupts, and the Bug Three Test Methods Missed"
date: 2026-08-04
abstract: "Adding interrupt delivery to an RV32 interpreter, and finding a trap-vectoring bug that no reference model was ever going to catch"
category: systems
---

## Introduction

This emulator has had `mip`, `mie`, `mtvec`, `mepc`, `mcause` and `mret`
since the first article. It has never had an interrupt. The machine knew
the whole vocabulary for taking one and had no way for one to arrive.

The gap was filled somewhere else. A separate project vendored this
interpreter, needed a timer, and added the seam it was missing. This
article takes that work back. The port is the strongest evidence so far that
the emulator's shape is right: the changes
were written against the version published in the first article, applied
cleanly to a core that had since grown by 353 lines and two extensions, and
touched nothing that either set of work owned.

It also brought a bug back with it. Writing tests for interrupts exposed a
defect in trap delivery that has been in every version of this emulator,
that 268 compliance tests did not catch, that a hundred thousand fuzzed
instructions compared against QEMU did not catch, and that every lockstep
run passed straight through. The bug is small. The reason all three methods
missed it is not, and it is the more useful half of this article.

## Abstract

Interrupt delivery is added in 122 lines: three level-sensitive lines
(software, timer and external), the two functions a host needs to drive
them, and delivery between instructions with `mepc` naming the instruction
that has not run. `wfi` remains a legal nop and now records that the guest
is parked, so a host stepping the machine can jump its own clock forward
instead of stepping the wait out. A defect in vectored trap delivery is
fixed: vectored `mtvec` was being applied to exceptions as well as
interrupts, sending an illegal instruction to whatever handler entry its
cause number happened to land on. This is the first part of the machine
with no reference model available, because `qemu-riscv32` runs in user mode
and has no interrupts at all, so 32 directed tests take expected values
from the specification and are checked by mutation instead: five deliberate
defects, including the one that was just fixed, and all five are caught.
Measured with callgrind rather than wall clock, the per-instruction
interrupt check costs 4.3 host instructions per guest instruction placed
naively and 3.0 behind a two-word guard, against a baseline of 201.

## The Series

Part four of five on one small RV32 interpreter, built to be embedded in
another program.

1. [A Compact RV32 Engine, Verified Against QEMU](/rv32-emulator/): the
   interpreter, and four independent ways of establishing it is correct
2. [What Bit Manipulation Buys an RV32 Interpreter](/rv32-bitmanip/): Zba,
   Zbb and Zbs, measured in retired instructions
3. [Zcb: The Same Instructions in Less Space](/rv32-zcb/): compressed byte
   and halfword forms, measured in code size
4. **Interrupts, and the Bug Three Test Methods Missed**: interrupt
   delivery, and a defect no reference model could catch
5. [Coverage Told Us About the Bug and We Did Not Listen](/rv32-coverage/):
   auditing the rig with coverage and mutation testing

Every article measures the same interpreter at a different point in its
life. `rv32.c` is that interpreter, the single evolving copy the whole
project builds against, and
[`rv32-orig.c`](/rv32-emulator/demo/rv32-orig.c) beside it is the version
the first article described, kept unchanged so that its measurements stay
reproducible. That frozen copy matters more here than anywhere else in the
series: the defect below is in it, and comparing the two files is how the
extent of the bug was established.

## The Seam

The whole interface is two functions.

```c
void     rv_set_irq(rv_cpu *cpu, int cause, int level);
uint32_t rv_irq_pending(const rv_cpu *cpu);
```

A timer, an interrupt controller, or in this project's intended use a
browser's frame clock, lives in the host. It decides when a line is high
and says so. The core does the rest between instructions. There is no CLINT
in here, no PLIC, and no timer: those are boards, and this is a CPU.

That division is the same one the emulator already makes for memory, which
is six callbacks and no allocator, and for the environment call, which is a
host function rather than a syscall table. The interrupt lines fit the
existing shape rather than extending it.

Three lines exist because a single hart in machine mode has three, and
their numbers are their bit positions in `mip` and `mie` and the low bits
of the `mcause` they produce:

```c
#define RV_IRQ_SOFT     3
#define RV_IRQ_TIMER    7
#define RV_IRQ_EXT      11
```

### Level, Not Edge

The lines are level-sensitive, and this is the design decision in the
feature rather than an implementation detail.

A raised line stays raised until the host lowers it or the guest clears the
bit in `mip`. A handler that returns without doing either is re-entered
immediately, and a guest that forgets to acknowledge its timer livelocks
inside its own handler. That is what the hardware does, and a test asserts
it:

```
a line left raised re-enters the handler
  ok   re-entered while raised                      00000001
  ok   the interrupted loop never advanced          00000000
```

The alternative would be to clear the line on delivery, which is friendlier
to a guest that forgets and wrong. Firmware developed here and moved to a
real chip would then meet the livelock for the first time on hardware,
which is the worst place to learn it. Modelling the machine is worth more
than sparing the guest.

### Between Instructions

An interrupt is taken before an instruction, not after one:

```c
    if ((cpu->mip & cpu->mie) != 0) {
        uint32_t irq = rv_irq_pending(cpu);

        if (irq) {
            rv_trap(cpu, irq, 0);
            return cpu->halted ? -1 : 0;
        }
    }
```

`mepc` therefore names an instruction that has not run, `mret` resumes at
it, and nothing retires on that step. The instruction counter does not
advance for taking an interrupt, which keeps every instruction count in the
previous two articles meaningful in the presence of one.

Both gates apply, and they are independent: `mstatus.MIE` globally, and the
per-line bit in `mie`. Because a line is a level, enabling a bit later
takes an interrupt that was already raised, rather than losing it.

### wfi

`wfi` is permitted to be a nop, and that is what it was before and remains.
An interpreter has no idle state to enter; the guest simply runs on and
takes the interrupt when it arrives. What is new is that it records the
fact:

```c
    cpu->waiting = 1;
```

Nothing in the interpreter reads that flag. It exists for a host that
drives the machine itself and would rather advance its own clock to the
next scheduled interrupt than step a guest through the idle loop it is
parked in. This is the same choice as everywhere else in the design: the
core reports, the host decides.

### What It Woke Up

One rule in the emulator became reachable for the first time. `rv_trap`
has always dropped any outstanding reservation, on the grounds that a
store conditional in a handler must not be able to complete a `lr.w`
issued by the code it interrupted. Until now the only way to exercise that
was to take an exception between the two halves of an atomic pair, which
no working program does.

An interrupt does it routinely, and the architecture requires the store
conditional to fail so the guest retries:

```
an interrupt between lr.w and sc.w drops the reservation
  ok   the pair succeeds when nothing interrupts it 00000000
  ok   the interrupt was taken                      00002000
  ok   mret resumed at the store conditional        00001004
  ok   the store conditional failed                 00000001
```

The [first article](/rv32-emulator/) described the A extension as a
convenience, present so that code using C11 atomics would link rather than
because anything could interleave. That is still true, and it now has one
less asterisk: a single hart with interrupts taken only between
instructions still cannot interleave a read-modify-write, but it can now
lose a reservation the way real hardware does.

## The Bug

Vectored trap delivery is selected by the low bits of `mtvec`. In vectored
mode, interrupt *n* enters the handler at `base + 4n` instead of at `base`,
so a machine can dispatch on the cause without reading `mcause`. Exceptions
do not vector. They always enter at `base`.

Here is what this emulator did, from the first commit until this week:

```c
    base = cpu->mtvec & ~3u;
    if ((cpu->mtvec & 3) == 1)
        base += cause * 4;              /* vectored mode, exceptions use base */
```

The comment is correct and the code does the opposite of it. With vectored
mode selected, an illegal instruction, cause 2, entered at `base + 8`. A
misaligned load, cause 4, entered at `base + 16`. Whatever those addresses
contained ran as if it were the handler.

The fix is one condition:

```c
    if ((cpu->mtvec & 3) == 1 && is_irq)
        base += (cause & ~RV_CAUSE_INTERRUPT) * 4;
```

### Why Nothing Caught It

This emulator is verified three ways, and the previous two articles argued
that its confidence comes from a reference model rather than from
hand-written tests. All three methods ran over this code hundreds of
thousands of times and none of them could have found this.

**The compliance suite never selects vectored mode.** The evidence is not a
reading of the suite: it is that the run reported in the previous article,
268 passing with nothing failing, was made by an interpreter that had this
bug. Every one of those tests installs a direct-mode `mtvec`, so the line
that gets it wrong never executes.

**The fuzzer cannot reach it.** It generates arithmetic instructions on a
hart with no interrupts and no memory access, deliberately, so that a
random encoding cannot wander off. It never traps, so it never dispatches a
trap.

**Lockstep cannot reach it either, and this is the interesting one.** The
reference model is `qemu-riscv32`, which is user-mode QEMU. There is no
machine mode in a user-mode process, no `mtvec` to write, and no interrupts
at all. The reference against which everything else in this emulator was
checked does not implement the feature the bug is in.

So the bug was not missed through carelessness. It sat in a region all
three methods are structurally blind to, and the only reason it surfaced
now is that someone wrote a test for the feature next door to it.

The general form: a verification strategy built on differential testing
inherits the reference model's scope as its own ceiling. Everything the
reference does not implement is unverified, and it is unverified silently,
because there is no failing test to notice. Knowing where that boundary
falls is part of knowing what your tests are worth.

## Verification Without a Reference

Interrupts are the first feature in this project that has to be checked
without a second implementation to compare against. There are three ways to
get confidence, and only two are available.

Compare against a model: not possible for the reason above. It could be
made possible with `qemu-system-riscv32` and a CLINT, but the comparison
would then be between two machines whose timers advance on different
schedules, and making an interrupt arrive at the same instruction on both
would be most of the work.

Derive expected values from the specification: available, and what
`test_irq.c` does. Thirty-two checks over delivery, both mask gates,
priority, vectored mode for interrupts and for exceptions, `wfi`,
level-triggered re-entry, and the reservation an interrupt drops. The guest
programs are hand-encoded so the test owns the whole machine, and it needs
neither a cross toolchain nor QEMU.

Check that the tests would notice if the code were wrong: available, and
the substitute for the missing reference. Five deliberate defects were
introduced one at a time:

| Mutation | Caught |
|---|---|
| The old vectoring, applying to exceptions too | yes |
| Priority order reversed, timer before external | yes |
| Lines made edge-triggered, `mip` cleared on delivery | yes |
| The `mstatus.MIE` gate removed | yes |
| Interrupt taken after the instruction instead of before | yes |

A test suite that passes tells you nothing on its own. A test suite that
fails when you break the thing it is testing has said something, and for
code with no reference model that is the strongest statement available.

## What It Costs

The previous two articles measured interpreter overhead in wall-clock MIPS.
That was adequate for differences of several percent on an idle machine and
is not adequate here: the numbers below differ by fractions of a percent,
and repeated runs on a loaded host moved further than the effect being
measured.

So this article measures host instructions retired, counted by callgrind,
which is deterministic and identical from run to run. The workload is the
same benchmark guest, 612,030 guest instructions, using no interrupts at
all.

| Interpreter | Host instructions | vs published |
|---|---|---|
| Published, first article | 123,207,388 | -- |
| With Zba, Zbb, Zbs and Zcb | 123,972,534 | +0.62% |
| Interrupts, check unguarded | 126,621,137 | +2.77% |
| Interrupts, check guarded | 125,804,559 | **+2.11%** |

Per guest instruction that is 201 host instructions for the published
interpreter, and the interrupt check adds 4.3 of them placed naively or 3.0
behind a guard. The guard is the whole optimisation:

```c
    if ((cpu->mip & cpu->mie) != 0) {
```

Two words that are already in the struct, and a branch that is never taken
in a guest with no interrupt controller. It recovers about a quarter of the
check's cost by avoiding a call that computes the same answer.

This is the third time in three articles that the same shape of question
has come up, and the third different answer. For the bit-manipulation
extensions the fix was to move a decoder behind the base integer set; for
Zcb it was to move an expander behind the base compressed set; here neither
applies, because an interrupt check genuinely does have to happen on every
instruction. What was available instead was making the common case cheap
enough not to matter.

The table also says something about the two previous
articles. They reported the extension work costing about 2.4% of
throughput, measured in wall clock. Counted in host instructions it costs
0.62%. Both numbers are real and they measure different things: the
instruction count is work done, and the wall clock also contains the cache
and branch-prediction effects of a larger interpreter. Where they disagree,
the wall clock is what a user experiences and the instruction count is what
the code actually does.

## Where the Work Came From

The interrupt code in this article was not written for it. A separate
project adopted this interpreter, wanted a guest that could be interrupted
by a timer, and wrote the seam.

The core was adopted unmodified. The version it started from is byte for
byte the `rv32-orig.c` kept in this project's download for exactly this
kind of comparison, which made the delta trivial to identify: 72 lines in
`rv32.c` and 41 in `rv32.h`. Two commits in that project's history touch
the emulator core, one to adopt it and one to add interrupts, so there was
nothing to disentangle.

The delta applied to a core that had moved on. This project had added Zba,
Zbb, Zbs and Zcb in the meantime, growing `rv32.c` by 353 lines. None of it
conflicted, because interrupts live in `rv_trap` and `rv_step` while the
extensions live in the decoders. That is not luck so much as evidence that
the boundaries in a 2,400-line interpreter are in defensible places.

And the direction was the useful one. A separate project needed something,
built it against a stable core, and it came back as a patch rather than as
a fork. For a piece of code whose entire purpose is to be embedded in
someone else's program, that round trip is a better result than any
benchmark in this series.

## Conclusion

Interrupt delivery is 122 lines and turns a machine that could describe
interrupts into one that can take them. The design is level-sensitive lines
driven by the host, delivery between instructions, and `wfi` as a nop that
records what it was waiting for, all of which keeps the policy outside the
core where the memory bus and the environment call already are.

The bug matters more than the feature. Vectored `mtvec` was applied to
exceptions for as long as this emulator has existed, and the three
verification methods this project has been proud of were all structurally
incapable of finding it: the compliance suite never selects vectored mode,
the fuzzer never traps, and the reference model is a user-mode process with
no machine mode at all. A differential strategy can only reach as far as its
reference does, and this project had been standing on that limit for a month
without ever measuring where it fell.

Where no reference exists, the substitute is a specification for expected
values and mutation for confidence. Five deliberate defects, five caught.
That is a weaker guarantee than a hundred thousand instructions compared
against QEMU, and it is the strongest one available in a region QEMU cannot
enter.

The obvious next question is whether anything in the rig was pointing at
this bug and being ignored. Something was, and
[Coverage Told Us About the Bug and We Did Not Listen](/rv32-coverage/)
audits all four verification methods to find out what else they are not
seeing.

## Source

The interpreter, the interrupt tests and the mutation list are in the
[companion download for the first article](/rv32-emulator/rv32-emulator-source.zip),
which is the single copy of this code the whole project builds against. The
setup guide is in the demo
[README.md](/rv32-emulator/demo/README.md)
([HTML](/rv32-emulator/demo/README.html)).

```sh
make check          # every suite that needs no reference model, 443 checks
make test-irq       # the 32 interrupt checks on their own
make archtest       # 268 passing, nothing failing, nothing unbuilt
```
