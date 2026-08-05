---
title: "Coverage Told Us About the Bug and We Did Not Listen"
date: 2026-08-05
abstract: "Auditing four verification methods with line coverage, instruction coverage and mutation testing, and finding that the metrics disagree about which of them to keep"
category: systems
---

## Introduction

The [previous article](/rv32-interrupts/) found a defect in trap delivery
that had been in this emulator since its first commit. Vectored `mtvec` was
applied to exceptions as well as interrupts, so with vectored mode selected
an illegal instruction entered the handler at the wrong address. It survived
268 compliance tests, a hundred thousand fuzzed instructions compared
against QEMU, and every lockstep run the project had ever done.

That article explained why each method was structurally blind to it. This
one starts from a more uncomfortable question: was anything pointing at it?

Something was. Here is the coverage of that function, reconstructed from the
version of the interpreter that had the bug, under the test suite as it
stood at the time:

```
       59:  133:    if ((cpu->mtvec & 3) == 1)
branch  0 taken 0% (fallthrough)
branch  1 taken 100%
    #####:  134:        base += cause * 4;   /* vectored mode, exceptions use base */
```

The condition was evaluated 59 times and never once taken. The line under it
was never executed at all. `gcov` had been reporting that line as uncovered
for as long as the bug existed, in a list of 436 uncovered lines that nobody
read.

It is worse than that. The [first article in this
series](/rv32-emulator/) had already looked at that line and excused it by
name, in a sentence that has been published since the day the emulator was:

> Line coverage of the interpreter is 86.3%. The remainder is defensive:
> double-fault handling, **vectored trap vectors**, and range-extension
> branches in the math helpers that single-precision operands cannot reach.

Every word of that is true except the one doing the work. "Defensive" is a
claim about code that cannot be reached, and it was applied to code that
simply had not been. We wrote down the excuse, and the bug was inside the
excuse.

## Abstract

Three coverage measures are applied to the same interpreter and the same six
verification methods, to find out which of them is worth keeping. Line
coverage reaches 89.0% of 1,356 lines, and attributing it per method shows
that lockstep co-simulation against QEMU contributes no line no other method
reaches. Instruction coverage, a measure built for this article because line
coverage is the wrong unit for an emulator, reaches 97.1% of 171
instructions and says the same thing about lockstep, and identifies five
instructions that no method executes at all. Mutation testing contradicts
both: of 92 mutants, the unit suites kill 56, lockstep and the fuzzer kill 4
that nothing else kills, and the compliance suite kills 5 more, for a
mutation score of 70.7%. Twenty-seven mutants survive everything, and 19 of
them sit on lines that line coverage reports as covered. The
conclusion is that coverage identifies what was never tried, mutation
identifies what was tried without being checked, and the two disagree about
which methods earn their place strongly enough that acting on either alone
would remove a method the project cannot do without.

## The Series

The last of five on one small RV32 interpreter, built to be embedded in
another program. The first four added things to it; this one audits the way
all four of them were checked.

1. [A Compact RV32 Engine, Verified Against QEMU](/rv32-emulator/): the
   interpreter, and four independent ways of establishing it is correct
2. [What Bit Manipulation Buys an RV32 Interpreter](/rv32-bitmanip/): Zba,
   Zbb and Zbs, measured in retired instructions
3. [Zcb: The Same Instructions in Less Space](/rv32-zcb/): compressed byte
   and halfword forms, measured in code size
4. [Interrupts, and the Bug Three Test Methods Missed](/rv32-interrupts/):
   interrupt delivery, and a defect no reference model could catch
5. **Coverage Told Us About the Bug and We Did Not Listen**: auditing the
   rig with coverage and mutation testing

Every article measures the same interpreter at a different point in its
life. `rv32.c` is that interpreter, the single evolving copy the whole
project builds against, and
[`rv32-orig.c`](/rv32-emulator/demo/rv32-orig.c) beside it is the version
the first article described, kept unchanged so that its measurements stay
reproducible. Keeping that copy is what made the opening of this article
possible: the coverage below was reconstructed by instrumenting the frozen
file and running the suite that existed alongside it.

## The Four Measurements

Six methods verify this interpreter, and every one of them can be run alone:

| Method | What it is |
|---|---|
| `unit` | seven hand-written suites, 443 checks, no reference model |
| `program` | one compiled guest that validates its own results |
| `apps` | CoreMark and Lua, which validate their own results |
| `archtest` | 268 compliance tests, signatures compared against QEMU |
| `fuzz` | randomized encodings, compared against QEMU per instruction |
| `lockstep` | compiled guests, compared against QEMU per instruction |

Each was run against one instrumented build, with the counters cleared in
between, so that the sets are comparable and set arithmetic over them means
something.

### Line Coverage

```
method        lines   of 1356    unique cumulative
unit            929     68.5%       152        929
program         454     33.5%         0       1063
apps            394     29.1%         6       1128
archtest        976     72.0%        19       1202
fuzz            754     55.6%         5       1207
lockstep        674     49.7%         0       1207

union          1207     89.0%
```

The **unique** column is the one worth reading. It counts lines that a
method reaches and no other method reaches, which is the closest thing
coverage offers to "what would be lost if this were deleted".

By that measure the compiled guest and lockstep contribute nothing at all.
Every line they touch is touched by something else. If line coverage were
the criterion for keeping a test, both would go, and lockstep is the method
this project's entire claim to correctness rests on.

### Instruction Coverage

Line coverage asks which of `rv32.c` a method ran. For an emulator that is
the wrong unit: one switch arm serves eight instructions, so a method can
execute every line of the decoder while touching a fraction of the
architecture. The measure that fits is which *guest instructions* ran, and
nothing off the shelf reports it, so `icov.c` does: one hook in the fetch
path behind a build flag, and a classifier that turns an encoding into one
of the 171 instructions this emulator implements.

```
method         seen    of 171    unique cumulative
unit             98     57.3%         8         98
program          39     22.8%         0        129
apps             73     42.7%         0        156
archtest        155     90.6%         7        166
fuzz            101     59.1%         0        166
lockstep         91     53.2%         0        166

union           166     97.1%
```

The two measures rank the methods differently, which is the first sign that
neither is measuring what it appears to. The unit suites lead on lines
(68.5%) and trail badly on instructions (57.3%), because they exercise many
paths through the decoder with a narrow set of encodings. The compliance
suite is the reverse and dominates on the metric that suits the domain: 155
of 171 instructions, and 7 that nothing else reaches.

It also produces something line coverage never did, which is a short and
actionable list. Five instructions are executed by no method at all:

```
ebreak  c_ebreak  csrrc  csrrci  csrrsi
```

That is a finding a person can act on in an afternoon, and it is the same
information as "149 uncovered lines" in a form that does not invite being
summarised away.

### Mutation

Coverage of either kind says a line ran. It cannot say whether anything
would have complained had that line been wrong, which is the only question a
test suite exists to answer. `mutate.sh` answers it directly: introduce one
deliberate defect into `rv32.c`, rebuild, run the rig, and see whether
anything fails.

The rig is applied in stages, so what each stage adds is exactly what the
cheaper ones missed.

| Stage | Mutants killed | Cumulative score |
|---|---|---|
| The unit suites | 56 | 60.9% |
| Plus lockstep and the fuzzer | +4 | 65.2% |
| Plus the compliance suite | +5 | **70.7%** |
| Survived everything | 27 | |

Ninety-two mutants were judged, from 150 attempted: 51 did not compile and 7
produced no textual change.

Now put that beside the coverage tables. Lockstep contributes **no unique
line** and **no unique instruction**, and it kills four mutants that nothing
else kills. On the evidence of coverage it is redundant. On the evidence of
mutation it is the only thing standing between four specific defects and a
release.

## What the Two Measures Disagree About

The disagreement is not noise, and it has a mechanism.

Coverage is a measure of *reach*: did control flow arrive here. Mutation is a
measure of *sensitivity*: if the answer computed here were wrong, would
anything downstream notice. A method can be excellent at one and useless at
the other, and the two methods this project relies on most are precisely
those cases.

**The fuzzer adds almost no coverage and is the only method that has ever
found a real bug here.** It contributes 5 unique lines out of 1,356 and no
unique instructions. What it actually does is re-execute lines every other
method already covers, with operands nobody would choose: zero, all ones,
the sign bit, the exact midpoint between two floats. Its value is entirely
in the third dimension that neither coverage measure has, which is the
values flowing through a line rather than the fact that control reached it.

**Lockstep adds no coverage and supplies the only independent oracle.**
Everything else in the rig compares against expectations written by the same
person who wrote the interpreter. Lockstep compares against an
implementation written by strangers. Coverage cannot see the difference
between an assertion and an oracle, so it reports the method as redundant
with the tests whose correctness it exists to underwrite.

That is the general shape: coverage measures the test's contact with the
code, and says nothing about the quality of the judgement applied once
contact is made.

## Where Coverage Actively Misleads

Of the 27 mutants that survive everything, 25 are distinct source lines, and
**19 of them sit on lines that line coverage reports as covered.**

```
  226      if ((addr & (uint32_t)(size - 1)) == 0) {
  685          if (f32_sign(a) == f32_sign(b))
 1055             (rd << 7) | op;
 1342      else if (rlist <= 11)
 2176          if ((f3 & 3) != 1 && rs1 == 0) {
 ...
```

Line 226 is the alignment test in every guest memory access. It is executed
hundreds of thousands of times by the test suite. Invert it, and nothing in
443 unit checks, 268 compliance tests, the fuzzer or lockstep says a word.
Line 1342 is a frame-size boundary in the Zcmp stack adjustment, covered by
73 dedicated tests, and moving the boundary by one goes unnoticed.

This is the precise sense in which 89% coverage is not 89% of anything
useful. The covered set contains at least 19 places where the code could be
wrong today without a single test failing.

### The Survivors That Are Not Holes

Honesty requires the other half. Some survivors are equivalent mutants,
where the change cannot alter behaviour, and telling them apart from real
holes is manual work that mutation testing does not do for you.

The first survivor found was this, in the minimum and maximum helper:

```c
    return fa < fb ? a : b;         /* mutated to <= */
```

Changing `<` to `<=` alters the result only when `fa == fb`, and two
distinct bit patterns compare equal as floats only when they are `+0` and
`-0`, which three lines above are handled separately. The mutant is
equivalent, and establishing that took reading the enclosing function.

Four more survivors are inside the portable fallbacks of `clz32`, `ctz32`
and `cpop32`, which this compiler never selects because the builtins are
available. They are not holes either; they are code that this build does not
contain.

So of 25 survivor lines, a handful are equivalent or unbuildable and the
rest are real. Mutation testing does not hand you a defect list. It hands
you a list to read, which is the same demand coverage makes and the reason
both get ignored.

## What the Reference Model Cannot Reach

The last measurement is the structural version of the previous article's
argument. Lockstep compares against `qemu-riscv32`, which is user-mode QEMU:
a process, not a machine. There is no machine mode in it, no `mtvec` to
write, and no interrupts. Whatever part of this interpreter implements those
things cannot be checked against it, ever, no matter how many guests are run.

That is measurable rather than arguable:

| Function | Lines lockstep executes |
|---|---|
| `rv_trap` | **0 of 58** |
| `csr_read` | **0 of 42** |
| `csr_write` | **0 of 35** |
| `rv_irq_pending` | **0 of 26** |
| `exec_zcmp` | 25 of 72 |
| `exec` | 148 of 531 |

One hundred and sixty-one lines of trap handling, control registers and
interrupt delivery, in a method that has run millions of instructions
against an independent implementation, with a coverage count of zero. The
vectoring bug lived in the first row.

Those zeros are where the reference model's scope runs out, drawn as a
boundary through a specific file at specific line numbers. The previous
article argued that such a boundary exists. This is where it falls, and
nothing above it has ever been compared against anything.

## What This Changed

The measurements are only worth taking if they change what gets done, so:

**Five instructions no method executes.** `ebreak`, `c_ebreak`, `csrrc`,
`csrrci` and `csrrsi` are implemented, shipped, and have never run. That is
a concrete gap in the rig with a concrete fix.

**Nineteen covered lines whose correctness nothing checks.** The alignment
test, the Zcmp frame boundary, several immediate assemblies in the
compressed decoder. Each is a test that should exist and does not.

**The methods are keepers, and coverage was wrong about which.** Nothing
here argues for deleting lockstep or the fuzzer, the two methods coverage
called redundant. It argues for distrusting the metric that called them
that.

**The uncovered list gets read now, not summarised.** The specific failure
that produced the vectoring bug was compressing 436 lines into a percentage
and a sentence of excuses. `coverage-by-method.sh` writes the list to a file
for that reason.

## Conclusion

Line coverage is a floor, not a measure of quality. It answers exactly one
question well, which is what has never been tried at all, and this project
had that answer in hand for weeks with a bug sitting in it. The answer was
not acted on because 436 uncovered lines is not a finding, it is a chore,
and the chore was discharged with a sentence in a README that named the
buggy line as acceptable.

Instruction coverage is the same tool in the unit that suits the domain, and
it is better only because its list is short enough to read. Five instructions
that nothing executes is a finding. One hundred and forty-nine uncovered
lines is a document nobody opens.

Mutation testing measures the thing everyone assumes coverage measures, and
it is expensive, noisy with equivalent mutants, and the only one of the three
that would have told this project that its 89% covered code contains at least
19 places where being wrong costs nothing. A mutation score of 70.7% is a far
more honest description of this test suite than 89% coverage, and both are
more honest than the four green ticks that preceded them.

The disagreement between the metrics is the most useful result. Coverage
ranked lockstep and the fuzzer as redundant; mutation ranked them as the only
methods catching nine specific defects. Any project with one metric and a
deletion policy would have removed the tests that work. That is not an
argument for measuring more things. It is an argument for knowing which
question each number answers, and for the fact that neither of them answers
"is this correct".

## Source

The three measurement tools are in the
[companion download for the first article](/rv32-emulator/rv32-emulator-source.zip):
`coverage-by-method.sh` for line coverage per method, `icov.c` and
`icov-by-method.sh` for instruction coverage, and `mutate.sh` for mutation.
The setup guide is in the demo
[README.md](/rv32-emulator/demo/README.md)
([HTML](/rv32-emulator/demo/README.html)).

```sh
make coverage-methods   # what each method covers, and what it alone covers
make icov               # the same, in guest instructions
make mutants            # whether the rig notices when the code is wrong
```
