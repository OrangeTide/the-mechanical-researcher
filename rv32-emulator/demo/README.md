# RV32IMAFC_Zicsr_Zifencei_Zba_Zbb_Zbs_Zcb_Zcmp Emulator

An embeddable 32-bit RISC-V interpreter in C, with the differential test
tools used to establish that it is correct.

The interpreter is `rv32.c` and `rv32.h`. Everything else in this
directory either tests it, measures it, or demonstrates embedding it.

`rv32-orig.c` is the interpreter as the first article described it, kept
unchanged so that the two versions can be built and measured side by side.
Nothing links it except the `bench_rv32_orig` comparison binary.

## What It Implements

| Extension | Instructions | Notes |
|---|---|---|
| RV32I | 40 | base integer set |
| M | 8 | multiply, divide, remainder |
| A | 11 | atomics, best effort -- see below |
| F | 26 | single precision, all five rounding modes |
| C | 30 encodings | compressed, expanded to their 32-bit forms |
| Zicsr | 6 | control and status registers |
| Zifencei | 1 | instruction fence |
| Zba | 3 | shifted add, for array indexing |
| Zbb | 18 | counting, rotates, min and max, sign extension |
| Zbs | 8 | single-bit set, clear, invert and extract |
| Zcb | 11 encodings | compressed byte and halfword forms |
| Zcmp | 6 | whole-frame push and pop |

Zba, Zbb and Zbs together are the ratified B extension, and `misa`
advertises the B bit when all three are enabled.

Extensions are taken here when a compiler emits them from ordinary C, which
is the only thing that matters to an engine whose job is to run compiler
output. The RP2350's Hazard3 cores supplied the shortlist to test that
against, and the overlap is deliberate so that code can be prototyped here
and then moved to real silicon. It is not a target to match: nothing
requires an RP2350 binary to run on this emulator. Zbkb is on the chip and
not here, because no compiler emits its five new instructions.

Zcb expands into instructions the rest of the emulator already has, so
three of its forms need Zbb and one needs M. That is the specification's
dependency, and it is enforced by the expansion rather than by a check:
turn `bitmanip` off and `c.sext.b` raises the illegal-instruction trap on
its own.

Machine mode only. There is no supervisor mode and no MMU, so this runs
bare-metal programs rather than an operating system.

### Interrupts

Three level-sensitive lines, driven by the host:

```c
rv_set_irq(&cpu, RV_IRQ_TIMER, 1);      /* raise the timer line */
uint32_t cause = rv_irq_pending(&cpu);  /* what would be taken next */
```

A timer or an interrupt controller lives in the embedding, not in here. An
interrupt is taken between instructions, so `mepc` names the instruction
that has not run and nothing retires on that step. Both gates apply,
`mstatus.MIE` and the bit in `mie`, and priority is external, then
software, then timer.

The lines are levels rather than edges. A handler that returns without
lowering the line or clearing `mip` is re-entered, so a guest that forgets
to acknowledge livelocks in its handler exactly as it would on hardware.

`wfi` is a nop, which the specification allows, and sets `cpu.waiting` so a
host driving the machine can jump its clock to the next interrupt instead
of stepping the wait out.

Two properties are worth knowing before embedding it:

- **No math library and no `<fenv.h>`.** Every floating-point result is
  produced by computing an exact intermediate in double precision and
  rounding it in software. Nothing depends on the host's rounding mode.
  Build with `-fno-math-errno` so the one square root stays a single
  instruction.
- **No heap and no host pointers reach the guest.** All memory access goes
  through six callbacks, and an optional access-check callback lets an
  embedding refuse anything outside the guest's sandbox.

### The A Extension Is a Convenience

The atomics are present so that ordinary code links, not because anything
here is concurrent. Without them a guest using C11 `_Atomic`, the
`__atomic_*` builtins or C++ `std::atomic` fails at link time with
`undefined reference to __atomic_fetch_add_4`, because no runtime library
for that is built for this target.

This is a single hart, and an interrupt is only ever taken between
instructions, so nothing can interleave with a read-modify-write, and the
acquire and release bits order accesses that no other agent can observe. Treat them as a way to run software that expects
atomics, not as a concurrency guarantee. What is genuinely enforced:

- Each operation's arithmetic, and the value it returns.
- The reservation carried by `lr.w` and `sc.w`, including that a store
  conditional fails without a matching reservation, fails against a
  different address, and fails on a second use.
- The reservation is dropped on any trap, so a handler cannot complete a
  store conditional begun by the code it interrupted. Now that interrupts
  exist this rule is reachable in the ordinary way rather than only through
  an exception, and `test_irq.c` covers it: a timer between `lr.w` and
  `sc.w` makes the store conditional fail and the guest retry.
- Natural alignment, enforced regardless of the misaligned-access setting,
  because the guarantee cannot be offered on an operand split across two
  words.

The evidence is `test_atomic.c` (51 tests), the compliance suite's nine
atomic tests, and lockstep against qemu over all eleven forms. The
instruction fuzzer does not reach them, because it excludes memory access
by design.

## Prerequisites

```sh
sudo apt-get install gcc make binutils-riscv64-linux-gnu \
                     gcc-riscv64-linux-gnu qemu-user qemu-system-misc
```

| Tool | Needed for |
|---|---|
| `gcc`, `make` | building the interpreter and the tools |
| `riscv64-linux-gnu-gcc` | cross-compiling the guest programs |
| `qemu-riscv32` | the reference model for lockstep and fuzzing |
| `qemu-system-riscv32` | the reference model for the compliance suite |
| `clang` 18+ | only for Zcmp guests and the WebAssembly build |
| `riscv64-unknown-elf-gcc`, picolibc | only for the Lua guest |

GNU `as` 2.42 cannot assemble the Zcmp instructions. Guests that use them
are assembled with clang and linked with GNU `ld`, which the Makefile does
for any `*_zcmp.elf` target.

## Quick Start

```sh
make            # build the interpreter, the tools and the guest programs
make check      # everything that needs no reference model
```

`make check` runs eight suites and should report no failures:

```
108 tests, 0 failures                      IEEE-754 conformance
30 tests, 0 failures                       memory access and sandboxing
73 tests, 0 failures                       whole-frame push and pop
80 tests, 0 failures                       Zba, Zbb and Zbs
69 tests, 0 failures                       Zcb
28 tests, 0 failures                       interrupt delivery
51 tests, 0 failures                       atomics
3432 instructions retired, exit code 0     a compiled guest program
all harness checks passed
```

## Running Against a Reference Model

The claim that this interpreter is correct rests on comparing it against
qemu, not on the tests above. Three tools do that.

### Instruction-by-instruction comparison

```sh
make lockstep-run       # plain RV32IMFC
make lockstep-zcmp      # a guest built with whole-frame push and pop
make lockstep-bitmanip  # a guest the compiler filled with Zba, Zbb and Zbs
make lockstep-zcb       # a guest full of compressed byte and halfword work
```

Both models load the same ELF and start from the same architectural state,
then advance one instruction at a time. After every step all 32 integer
registers, the program counter, the 32 floating-point registers and `fcsr`
are compared, and guest memory is compared once at the end.

```
guest memory identical over [00010000,00014690)
3433 instructions in lockstep, no divergence
exit code 0 on both models
```

A divergence reports the encoding that caused it rather than whatever the
program eventually printed.

This runs at roughly 1,500 instructions per second, because each step costs
two packets over the remote debugging protocol. It is for correctness, not
for long runs.

### Randomized instructions

```sh
make fuzz-run           # 50 rounds, about 25,000 instructions
./fuzz fuzz_target.elf -n 1200 -s 1     # a longer campaign
```

Compiled code only reaches the encodings and operand values a compiler
chooses to emit. The fuzzer fills a buffer with random valid encodings,
randomizes every register including deliberately awkward floating-point
values -- signalling NaNs, subnormals, exact midpoints, values either side
of a binade boundary -- and steps both models.

It deliberately generates no control flow and no memory access. Both are
covered elsewhere, and excluding them keeps the program counter advancing
so a random encoding cannot wander into unmapped memory.

This is what found the two real bugs in this interpreter.

### The compliance suite

```sh
git clone -b old-framework-3.x \
    https://github.com/riscv-non-isa/riscv-arch-test.git ../../riscv-arch-test
make archtest
```

Each test is assembled against the target port in `archtest-port/`, run on
the interpreter, and its signature region compared against the same binary
running on `qemu-system-riscv32`.

```
riscv-arch-test: 268 passed, 0 failed, 4 skipped, 0 did not build
```

Three of the four skipped tests are the carry-less multiplies, which live
in the `B` directory but belong to Zbc rather than to B. The fourth stores
`mstatus`, whose readable bits depend on which privilege modes exist;
qemu's board has supervisor and user mode where this has machine mode only,
so the two read back different legal values.

The 11 tests that did not build before Zcb was implemented were the Zcb
ones. They live in the `C` directory rather than in a suite of their own.

Use the `old-framework-3.x` branch. The current framework needs an
assembler that pads `.p2align` with nops while relaxation is disabled;
binutils 2.42 fills it with zeros instead, which makes those binaries hang
on any model, qemu included.

## Running a Real Application

CoreMark is a third-party benchmark that validates its own results, which
makes it a useful answer to "does this thing actually run software". It is
not vendored here:

```sh
git clone --depth 1 https://github.com/eembc/coremark.git ../../coremark
make coremark
```

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
Correct operation validated. See README.md for run and reporting rules.

31360564 instructions retired, 471 syscalls, exit code 0
```

Every CRC matches the same binary run under `qemu-riscv32`, and the three
workload CRCs are the values CoreMark documents as correct.

Two caveats. The iterations-per-second figure is **not** a CoreMark score:
the clock in `coremark-port/` reads the retired-instruction counter, so a
"second" here is a million instructions rather than a unit of time.
CoreMark also rejects a run shorter than ten of its seconds, which is why
the iteration count is set high enough to pass that rule.

The port in `coremark-port/` is about a hundred lines and supplies the four
things CoreMark's barebones target expects from a board: a clock, board
initialization, a character sink, and the seed variables.

### Lua

A benchmark is a gentle test. Lua is not: it allocates constantly, uses
`setjmp` and `longjmp` for error handling, formats floating-point numbers,
and can check its own answers.

```sh
sudo apt-get install gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf
git clone --depth 1 -b v5.4.7 https://github.com/lua/lua.git ../../lua
make lua
```

```
lua Lua 5.4  integer max 2147483647  1e300 -> 1e+300
ok   integer arithmetic     3
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

Twenty-seven checks over integer and float arithmetic, closures, tables and
sorting, string patterns and formatting, metatables, coroutines, error
handling, garbage collection and recursion. The same binary produces the
same output under `qemu-riscv32`.

Worth knowing about the numbers: this build gives Lua 32-bit integers and
IEEE **double** floats. RV32F is single precision only, so every Lua float
operation runs in software through libgcc. Lua therefore leans on the
integer core and the M extension far more than on the F extension.

The port in `lua-port/` is one C file and one assembly file. picolibc
expects a board to supply `sbrk`, the standard streams, and a handful of
POSIX calls; there is no filesystem or clock here, so `open`, `unlink`,
`rename` and the time calls refuse cleanly rather than being omitted, which
leaves `io.open` raising a catchable Lua error instead of failing to link.

`lua.ld` reserves the heap and stack as real sections rather than by moving
the location counter past the end of the image. That keeps them inside a
loadable segment. The flat memory model here would tolerate either, but a
model that maps only what the program headers declare would not, and
getting it right is what lets the same binary run under qemu for
comparison.

### What Will Not Run Yet

The remaining limit is the syscall layer in `machine.c`, which implements
`write`, `exit` and `exit_group` and returns `-ENOSYS` for everything else.
That is enough for a program that computes and prints. Anything wanting
real files, a clock, or processes needs more of them.

## Benchmarking

```sh
make bench              # this interpreter alone
make bench-compare      # against the ColdFire V4e emulator
```

`bench.c` is cross-compiled for both targets from one source, so the two
emulators run the same algorithms. Both print their results, which should
be identical.

### What the Bit-Manipulation Extensions Are Worth

```sh
make bitmanip-compare   # the same guests built with and without them
make bitmanip-overhead  # what the wider decoder costs every guest
```

`bitmanip-compare` builds four guests twice, differing only in `-march`, and
reports retired instructions and `.text` size for each pair. Any guest named
`foo_zb.elf` is `foo.elf` built for `rv32imafc_zba_zbb_zbs`; the recipe is
shared between the two, so the builds cannot drift apart. The last two rows
need `CM` and `LUA` pointing at checkouts.

```
guest                    base    Zba+Zbb+Zbs    change       text
test_program             3432           3432     0.00%      0.00%
bench                  612030         552029    -9.80%     -0.73%
coremark             31362052       30414094    -3.02%     -1.14%
lua                  15252142       14974101    -1.82%     -0.22%
```

`bitmanip-overhead` runs the same base-ISA guest on `bench_rv32_orig`, which
links the published `rv32-orig.c`, and on `bench_rv32`, which links the
current `rv32.c`. The difference is what a guest pays for a decoder that
knows about instructions it never executes.

### What Zcb Is Worth

```sh
make zcb-compare        # the same guests, with and without Zcb
```

Zcb is a code-size extension rather than an instruction-count one. Every
encoding stands for exactly one 32-bit instruction, so the same program
retires the same instructions from a smaller image. Both columns already
have Zba, Zbb and Zbs, so this isolates Zcb.

```
guest             +B text  +Zcb text    change        insns
test_program         1650       1646    -0.24%      +0.0000%
bench                1086       1086     0.00%      +0.0000%
coremark            11956      11680    -2.31%      +0.0003%
lua                205832     204776    -0.51%      +0.0010%
```

The instruction column is not quite zero for two reasons, neither of them
the extension. CoreMark prints its own `-march` string, which is four
characters longer, so it makes four more write syscalls. Lua's output is
identical, and its smaller image moves every heap address, which changes
what the allocator does by a hair.

## The WebAssembly Demo

```sh
make wasm
xdg-open wasm/index.html
```

Builds the interpreter as a freestanding `wasm32` module with no imports,
cross-compiles a guest particle simulation, and embeds both into a single
self-contained page. It opens from the filesystem with no server.

Every particle is moved by RISC-V machine code. The page never computes a
position; it only draws what the guest asks it to through `ecall`. Once
three quarters of the particles have settled on the floor the guest
scatters them again, so the picture never stops moving. A per-frame
instruction budget means a guest that loops forever is cut off at the end
of its slice instead of freezing the page.

## Coverage, and What It Is Worth

```sh
make coverage           # the aggregate line count
make coverage-methods   # what each method covers, and what it alone covers
make icov               # the same question in guest instructions
make mutants            # whether the rig notices when the code is wrong
```

`make coverage` reports the aggregate, about 89% of lines across the whole
rig. Treat that number with suspicion. An earlier version of this file
described the uncovered remainder as "defensive: double-fault handling,
vectored trap vectors, and range-extension branches", and one of those
vectored trap vectors was a real bug that had been shipping since the first
commit. It was uncovered, it was listed as uncovered, and the list was
summarised instead of read. `coverage-by-method.sh` writes the uncovered
lines to a file so there is something to read.

The other three targets exist because the aggregate answers so little:

- `coverage-methods` runs each verification method alone and reports what it
  covers that nothing else does. Lockstep and the compiled guest cover no
  line no other method reaches, which is a fact about the metric rather than
  about lockstep.
- `icov` asks which of the 171 guest instructions each method executed,
  which is the unit that suits an emulator. The union is 97.1%, and the
  handful that no method reaches is a list short enough to act on.
- `mutants` introduces one deliberate defect at a time and counts what the
  rig notices. The score is 70.7%, and 19 of the survivors sit on lines
  coverage calls covered.

## Files

| File | |
|---|---|
| `rv32.c`, `rv32.h` | the interpreter |
| `rv32-orig.c` | the interpreter as first published, for comparison |
| `machine.c` | flat memory and a small syscall layer |
| `elf_loader.c` | ELF32 loader and symbol lookup |
| `gdbclient.c` | remote debugging protocol client |
| `lockstep.c` | instruction-by-instruction comparison |
| `fuzz.c` | randomized instruction comparison |
| `archtest.c` | compliance suite runner |
| `test_fp.c` | IEEE-754 conformance |
| `test_mem.c` | memory access, alignment and sandboxing |
| `test_zcmp.c` | whole-frame push and pop |
| `test_atomic.c` | atomics, reservations and alignment |
| `test_bitmanip.c` | Zba, Zbb and Zbs, and the switch that disables them |
| `bitmanip_guest.c` | a compiled guest full of them, for lockstep |
| `test_zcb.c` | the Zcb expansion table, offsets and dependencies |
| `zcb_guest.c` | a compiled guest full of byte and halfword traffic |
| `test_irq.c` | interrupt delivery, checked by mutation |
| `coremark-port/` | board port for the CoreMark benchmark |
| `lua-port/` | board port and script for running Lua |
| `test_harness.c` | runs a compiled guest and checks its results |
| `bench.c` | shared benchmark, built for both architectures |
| `wasm/` | WebAssembly module, guest and browser page |
| `archtest-port/` | target port for the compliance suite |

## License

Public domain, or MIT-0 where a licence is required.
