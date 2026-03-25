# ColdFire V4e Emulator Demo

A standalone ColdFire V4e CPU emulator in 2,239 lines of C, validated against
GCC-compiled bare-metal programs and QEMU.

## Quick Start (Smoke Test)

The smoke test embeds the compiled test binary directly — no cross-compiler
needed. Any system with a C compiler and `make` can run it:

```sh
make smoke
```

Expected output:

```
  fibonacci(10)        got 55         expected 55         PASS
  gcd(252, 105)        got 21         expected 21         PASS
  sum_to(100)          got 5050       expected 5050       PASS
  bit_test(0xAB)       got 2645       expected 2645       PASS
  sqrt(2)*1000         got 1414       expected 1414       PASS
5/5 smoke tests passed (1528 instructions)
```

## Full Test (Requires Cross-Compiler)

The full test cross-compiles a bare-metal C program for ColdFire V4e, loads the
ELF binary into the emulator, and checks results:

```sh
make test
```

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
| `make smoke` | No | No | Runs embedded binary through emulator |
| `make test` | Yes | No | Cross-compiles test program, runs through emulator |
| `make validate` | Yes | Yes | Runs same tests under QEMU for comparison |
| `make valgrind` | Yes | No | Memory-checks the emulator with valgrind |
| `make disasm` | Yes | No | Disassembles the test program ELF |

## Files

| File | Description |
|---|---|
| `coldfire.c` | ColdFire V4e emulator (2,239 LOC) |
| `coldfire.h` | Public API (117 lines) |
| `smoke_test.c` | Self-contained test with embedded binary |
| `test_harness.c` | ELF-loading test runner |
| `test_program.c` | Bare-metal test program (cross-compiled) |
| `qemu_validate.c` | QEMU validation program (cross-compiled) |
| `elf_loader.c/h` | Minimal ELF32 big-endian loader |
| `link.ld` | Linker script for bare-metal programs |
| `bin2c.sh` | Converts ELF disassembly to C array for smoke test |
| `Makefile` | Build system |
