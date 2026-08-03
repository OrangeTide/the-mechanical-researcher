# ColdFire V4e Emulator Demo

A standalone ColdFire V4e CPU emulator in 2,544 lines of C, validated against
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
| `make instrtest` | Yes | No | Instruction-level suite, 162 checks (see below) |
| `make instrtest-qemu` | Yes | Yes | The same suite under QEMU, 138 checks |
| `make coverage` | Yes | No | gcov line coverage of the emulator |
| `make valgrind` | Yes | No | Memory-checks the emulator with valgrind |
| `make disasm` | Yes | No | Disassembles the test program ELF |
| `make sections` | Yes | No | Shows ELF section headers |

### Known failure: `make instrtest`

`make instrtest` currently reports **155/162**. All seven failures are in the
EMAC group, and all of them are checks that read an accumulator back after a
`mac.l`: the accumulator reads as zero where the test expects a product. The
MACSR and MASK registers read and write correctly, so the register interface
works and the multiply-accumulate itself does not.

This is not covered by `make instrtest-qemu`. That target builds the same
source with `-DQEMU_USERMODE`, which compiles out the EMAC, ISA_C and legacy
groups because they are hand-assembled opcodes QEMU may reject. The EMAC tests
have therefore never been checked against an independent implementation, and
whether the emulator or the test is wrong is still open. Settling it needs the
CFPRM's MAC.L encoding rather than another test run.

The other targets are unaffected: the five compute tests, the 72-check smoke
suite and the QEMU validation all pass.

## Files

| File | Description |
|---|---|
| `coldfire.c` | ColdFire V4e emulator (2,544 lines) |
| `coldfire.h` | Public API (250 lines) |
| `test_coldfire.c` | Self-contained test with embedded binary, 72 checks |
| `test_instructions.c` | Instruction-level suite, cross-compiled |
| `test_harness.c` | ELF-loading test runner |
| `test_program.c` | Bare-metal test program (cross-compiled) |
| `test_program.s` | Assembly listing of the test program |
| `test_program.dis` | Disassembly of the test program |
| `qemu_validate.c` | QEMU validation program (cross-compiled) |
| `elf_loader.c/h` | Minimal ELF32 big-endian loader |
| `link.ld` | Linker script for bare-metal programs |
| `bin2c.sh` | Converts ELF disassembly to C array for smoke test |
| `Makefile` | Build system |
