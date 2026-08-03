# ColdFire V4e Emulator Demo

A standalone ColdFire V4e CPU emulator in 2,550 lines of C, validated against
GCC-compiled bare-metal programs and QEMU.

## Quick Start (Smoke Test)

The smoke test embeds the compiled test binary directly — no cross-compiler
needed. Any system with a C compiler and `make` can run it:

```sh
make smoke
```

It runs the five compute tests and then 67 single-instruction checks across
every opcode group, ending with:

```
ok   fibonacci(10)
ok   gcd(252, 105)
ok   sum_to(100)
ok   bit_test(0xAB)
ok   sqrt(2)*1000
...
72 passed, 0 failed
```

## Full Test (Requires Cross-Compiler)

The full test cross-compiles a bare-metal C program for ColdFire V4e, loads the
ELF binary into the emulator, and checks results:

```sh
make test
```

```
--- Test Results ---
  fibonacci(10)        @ 0x000103c2: got 55         expected 55         PASS
  gcd(252, 105)        @ 0x000103be: got 21         expected 21         PASS
  sum_to(100)          @ 0x000103ba: got 5050       expected 5050       PASS
  bit_test(0xAB)       @ 0x000103b6: got 2645       expected 2645       PASS
  sqrt(2)*1000         @ 0x000103b2: got 1414       expected 1414       PASS

5/5 tests passed
```

That program executes 1,528 instructions.

## QEMU Validation (Requires Cross-Compiler + QEMU)

Validates emulator results against QEMU's independent ColdFire V4e implementation:

```sh
make validate
```

## Prerequisites

### Linux (Ubuntu / Debian)

```sh
sudo apt install gcc-m68k-linux-gnu binutils-m68k-linux-gnu   # cross-compiler
sudo apt install qemu-user                                     # qemu-m68k
```

### Linux (Fedora)

```sh
sudo dnf install gcc-m68k-linux-gnu binutils-m68k-linux-gnu   # cross-compiler
sudo dnf install qemu-user-static                              # qemu-m68k
```

### Linux (Arch)

Arch does not package `m68k-linux-gnu-gcc`. The AUR has `m68k-elf-gcc` (bare-metal
target with newlib), which works for this demo's `-nostdlib -ffreestanding` build.
Change `M68K_CC` in the Makefile to `m68k-elf-gcc` if using this package.

```sh
# From AUR (e.g. via yay):
yay -S m68k-elf-gcc m68k-elf-binutils

# QEMU user-mode:
sudo pacman -S qemu-user
```

### macOS

There are no Homebrew or MacPorts packages for `m68k-linux-gnu-gcc`. Two options:

**Option A — Docker (recommended).** Run the full toolchain inside a Debian container:

```sh
docker run --rm -v "$PWD":/work -w /work debian:bookworm bash -c \
  "apt-get update && apt-get install -y make gcc gcc-m68k-linux-gnu qemu-user && make test && make validate"
```

**Option B — Build from source.** Use [crosstool-ng](https://crosstool-ng.github.io/)
to build a cross-toolchain targeting `m68k-linux-gnu` or `m68k-elf`:

```sh
brew install crosstool-ng
ct-ng m68k-unknown-elf
ct-ng build
```

Note: `qemu-m68k` (user-mode emulation) is Linux-only. It is not available on macOS
even with `brew install qemu`. The `make validate` target requires Linux or Docker.

The **smoke test works natively on macOS** with no extra tools — just `make smoke`.

### Windows

**WSL2 (recommended).** Install Ubuntu under WSL2, then follow the Ubuntu instructions:

```sh
sudo apt install gcc-m68k-linux-gnu binutils-m68k-linux-gnu qemu-user make gcc
make test
make validate
```

**Docker Desktop** also works — use the same Docker command as macOS above.

Note: `qemu-m68k` user-mode emulation is Linux-only. Native Windows builds of QEMU
only include `qemu-system-m68k`, not the user-mode `qemu-m68k`. WSL2 or Docker is
required for `make validate`.

The **smoke test works in any environment** with a C compiler (MSYS2, Visual Studio
Developer Command Prompt, or WSL):

```sh
make smoke
```

## What Each Target Does

| Target | Cross-compiler | QEMU | Description |
|---|---|---|---|
| `make smoke` | No | No | Runs embedded binary through emulator, 72 checks |
| `make test` | Yes | No | Cross-compiles test program, runs through emulator |
| `make validate` | Yes | Yes | Runs same tests under QEMU for comparison |
| `make instrtest` | Yes | No | Instruction-level suite, 162 checks |
| `make instrtest-qemu` | Yes | Yes | The same suite under QEMU, 138 checks |
| `make emactest` | Yes | Yes | EMAC differential test, 12 records against QEMU |
| `make coverage` | Yes | No | gcov line coverage of the emulator |
| `make valgrind` | Yes | No | Memory-checks the emulator with valgrind |
| `make disasm` | Yes | No | Disassembles the test program ELF |
| `make sections` | Yes | No | Shows ELF section headers |

### A note on the EMAC MASK register

`make instrtest` used to report 155/162, with every failure in the EMAC group:
any check that read an accumulator back after a `mac.l` found zero instead of a
product. The cause was worth recording.

The emulator was ANDing the MASK register into both register operands of a
multiply-accumulate. MASK does not do that. It is ANDed with an operand
*address*, to keep a pointer inside a circular buffer addressed with `(Ay)+`,
and the memory-referencing MAC forms that would use it are not implemented
here. So a program that wrote a restrictive MASK — as the test does, two checks
earlier, and never restores — made every subsequent multiply return zero.

It stayed hidden because `cf_reset` leaves MASK all-ones, so compiled code never
tripped it, and because `make instrtest-qemu` cannot see the EMAC group at all:
that target builds the same source with `-DQEMU_USERMODE`, which compiles out
the EMAC, ISA_C and legacy tests as hand-assembled opcodes QEMU may reject. The
EMAC path had never been compared against an independent implementation.

Three things settled it. `m68k-linux-gnu-as -mcpu=5475` assembles `mac.l` and
encodes it as `a602 0800`, so the canonical encoding is available rather than
guessed. Running that encoding on the emulator returns the correct product.
And `qemu-m68k -cpu cfv4e` computes the full product with MASK set to
`0xFFFF0000`, confirming MASK must not reach a register operand.

A smoke test had asserted the old behaviour, so it was locking the bug in place;
it now checks that a `mac.l` result does not depend on MASK.

### Comparing the EMAC against QEMU

The MASK bug survived because EMAC expectations were hand-written against a
hand-written implementation. `make emactest` closes that loop. It builds one
source, `test_emac.c`, twice: once bare-metal for the emulator and once for
`qemu-m68k`. Both print one line per recorded state, and the two are diffed.
Nothing is hand-assembled, since GNU as accepts every EMAC mnemonic for
`-mcpu=5475`.

QEMU can only arbitrate part of it. Measured against `-cpu cfv4e`:

| Behaviour | QEMU | Required |
|---|---|---|
| `mac.l` accumulate, 3x5 then 4x6 | 39 | 39 |
| `msac.l` from a zeroed accumulator | +15 | -15 |
| A product of 2^46, read back from `accext01` | 0 | 0x4000 |

So QEMU's cfv4e EMAC gets `mac.l` right, treats `msac.l` as an add, and does
not carry a product past 32 bits into the 48-bit accumulator. The target
therefore compares only the subset QEMU models, 12 records, and runs the
saturation cases separately as an emulator-only regression baseline. Matching
QEMU on the rest would mean adopting its gaps.

The 12 comparable records agree exactly. Getting there took four fixes, all
confirmed against the ColdFire Family Programmer's Reference Manual, Rev. 3:

- **`movclr.l` did not clear.** The instruction is `move.l ACCn,Dn` with bit 6
  set, and bit 6 was ignored, so the accumulator was read but never zeroed.
  The manual gives the operation as `0 -> ACCx, ACCextx, MACSR[PAVx]`. QEMU
  caught this one directly.
- **MACSR[S/U] was inverted.** The bit reads 0 for signed and 1 for unsigned,
  which is the opposite of what the name suggests. The manual's pseudocode is
  explicit: signed integer mode is `MACSR[S/U,F/I] == 00`. Reset leaves MACSR
  zero, so the emulator had been giving compiled code unsigned multiplies by
  default. Two tests had encoded the same misreading and were locking it in.
- **The accumulator extensions were 8 bits wide instead of 16, and were dead
  state.** `ACCext01` packs bits 47:32 of two accumulators into one longword,
  the lower-numbered accumulator in the low half. They were held in a separate
  array that `mac.l` never updated, so reading `accext01` after accumulating
  returned zero however large the result. They are now derived from the
  accumulators, and the separate array is gone.
- **Results were not confined to 48 bits, and the overflow flags were
  conflated.** Without OMC the value must wrap rather than grow without bound.
  The three flags mean different things: `PAVx` is sticky, `V` is `PAVx` plus
  whatever the current operation did, and `EV` is narrower than either, asking
  whether the value has outgrown the low 32 bits of the accumulator (40 in
  fractional mode).

## Files

| File | Description |
|---|---|
| `coldfire.c` | ColdFire V4e emulator (2,550 lines) |
| `coldfire.h` | Public API (250 lines) |
| `test_coldfire.c` | Self-contained test with embedded binary, 72 checks |
| `test_instructions.c` | Instruction-level suite, cross-compiled |
| `test_emac.c` | EMAC differential test, built for both the emulator and QEMU |
| `emac_dump.c` | Runs `test_emac.elf` on the emulator and prints its results |
| `test_harness.c` | ELF-loading test runner |
| `test_program.c` | Bare-metal test program (cross-compiled) |
| `test_program.s` | Assembly listing of the test program |
| `test_program.dis` | Disassembly of the test program |
| `qemu_validate.c` | QEMU validation program (cross-compiled) |
| `elf_loader.c/h` | Minimal ELF32 big-endian loader |
| `link.ld` | Linker script for bare-metal programs |
| `bin2c.sh` | Converts ELF disassembly to C array for smoke test |
| `Makefile` | Build system |
