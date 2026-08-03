# RV32IMAFC_Zicsr_Zifencei_Zcmp Emulator

An embeddable 32-bit RISC-V interpreter in C, with the differential test
tools used to establish that it is correct.

The interpreter is `rv32.c` and `rv32.h`. Everything else in this
directory either tests it, measures it, or demonstrates embedding it.

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
| Zcmp | 6 | whole-frame push and pop |

Machine mode only. There is no supervisor mode and no MMU, so this runs
bare-metal programs rather than an operating system.

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

This is a single hart with no interrupts, so nothing can interleave with a
read-modify-write, and the acquire and release bits order accesses that no
other agent can observe. Treat them as a way to run software that expects
atomics, not as a concurrency guarantee. What is genuinely enforced:

- Each operation's arithmetic, and the value it returns.
- The reservation carried by `lr.w` and `sc.w`, including that a store
  conditional fails without a matching reservation, fails against a
  different address, and fails on a second use.
- The reservation is dropped on any trap, so a handler cannot complete a
  store conditional begun by the code it interrupted.
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

`make check` runs five suites and should report no failures:

```
108 tests, 0 failures                      IEEE-754 conformance
30 tests, 0 failures                       memory access and sandboxing
73 tests, 0 failures                       whole-frame push and pop
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
riscv-arch-test: 228 passed, 0 failed, 1 skipped, 11 did not build
```

The 11 that do not build are Zcb instructions, which this interpreter does
not implement. The one skipped test stores `mstatus`, whose readable bits
depend on which privilege modes exist; qemu's board has supervisor and user
mode where this has machine mode only, so the two read back different legal
values.

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

## Coverage

```sh
make coverage
```

Reports about 75% of lines in `rv32.c` on its own, and about 86% when the
compliance binaries are run through the instrumented build as well. The
remainder is defensive:
double-fault handling, vectored trap vectors, and range-extension branches
in the math helpers that single-precision operands cannot reach.

## Files

| File | |
|---|---|
| `rv32.c`, `rv32.h` | the interpreter |
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
| `coremark-port/` | board port for the CoreMark benchmark |
| `lua-port/` | board port and script for running Lua |
| `test_harness.c` | runs a compiled guest and checks its results |
| `bench.c` | shared benchmark, built for both architectures |
| `wasm/` | WebAssembly module, guest and browser page |
| `archtest-port/` | target port for the compliance suite |

## License

Public domain, or MIT-0 where a licence is required.
