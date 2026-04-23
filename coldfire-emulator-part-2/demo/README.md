# ColdFire V4e Multi-Language Bare-Metal Demo

Cross-compiled test programs in seven programming languages, all targeting
the ColdFire V4e (`-mcpu=5475`) in bare-metal mode.

## Prerequisites

Ubuntu cross-compiler packages:

```
sudo apt-get install \
    gcc-m68k-linux-gnu \
    g++-m68k-linux-gnu \
    gobjc++-m68k-linux-gnu \
    gobjc-9-m68k-linux-gnu \
    gm2-m68k-linux-gnu \
    gfortran-m68k-linux-gnu \
    gdc-9-m68k-linux-gnu \
    gnat-10-m68k-linux-gnu
```

A native C compiler (GCC or Clang) is needed for the host-side test harness.

## Building

```
make          # build test harness + all 7 language ELFs
make test     # build and run all tests
make clean    # remove all generated files
```

## Test Programs

Each program implements five algorithms: recursive Fibonacci, Euclidean GCD,
summation, bitwise manipulation, and Newton's method square root approximation.
Results are stored in ELF symbols that the test harness checks after execution.

| File | Language | Standalone | Notes |
|---|---|---|---|
| `test_cpp.cpp` | C++ | Yes | `-fno-exceptions -fno-rtti` |
| `test_objc.m` | Objective-C | Yes | No ObjC runtime features |
| `test_objcpp.mm` | Objective-C++ | Yes | No ObjC/C++ runtime features |
| `test_d.d` | D | Yes | Needs `object.d` stub; integer-only sqrt |
| `test_fortran.f90` | Fortran | No | C shim: `shim_fortran.c` |
| `test_ada.adb` | Ada | No | C shim: `shim_ada.c`; `-gnatp` |
| `test_modula2.mod` | Modula-2 | No | C shim: `shim_modula2.c`; M2RTS stubs |

## Expected Output

```
$ make test
=== test_cpp.elf ===
...
5/5 tests passed

=== test_objc.elf ===
...
5/5 tests passed

(... all 7 languages ...)

=== Summary: 7 passed, 0 failed out of 7 ===
```

## File Layout

- `coldfire.c`, `coldfire.h` — ColdFire V4e emulator core (from Part 1)
- `elf_loader.c`, `elf_loader.h` — ELF binary loader (from Part 1)
- `test_harness.c` — Host test runner with symbol-based result checking
- `link.ld` — Bare-metal linker script (RAM at 0x10000, 1 MB)
- `object.d` — Minimal D runtime stub for GDC 9
- `Makefile` — Builds all targets

## Disassembly

To disassemble any test binary:

```
make disasm-test_fortran    # or any test name
make sections-test_ada      # show ELF section layout
```
