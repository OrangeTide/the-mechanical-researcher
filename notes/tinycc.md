# TinyCC (tcc)

## Overview

TinyCC (tcc) is a small, fast C compiler created by Fabrice Bellard. It compiles
C source to native machine code roughly 10x faster than `gcc -O0`, self-hosts
(compiles itself), and fits in roughly 100KB as a standalone executable including
preprocessor, compiler, assembler, and linker. Its standout feature is `-run`
mode, which compiles and executes C source directly — enabling C as a scripting
language via `#!/usr/local/bin/tcc -run` shebangs.

- **Version:** 0.9.28rc (no stable release since 0.9.27 in December 2017)
- **License:** LGPL v2+, with partial MIT relicensing in progress
- **Source:** ~64,000 lines of C90 across ~30 files
- **Repository:** repo.or.cz/tinycc.git (official), GitHub mirror at TinyCC/tinycc

## History and Origins

### OTCC (2001)

TCC descends from OTCC (Obfuscated Tiny C Compiler), which Bellard submitted to
the 2001 International Obfuscated C Code Contest, winning "Best abuse of the
rules." OTCC was a complete C subset compiler in 2,048 bytes of C source
(excluding semicolons, braces, and whitespace). It generated i386 machine code
directly in memory — no bytecode, no intermediate files. Despite its size, it was
self-hosting. A variant called OTCCELF generated dynamically linked i386 ELF
executables without requiring binutils.

### Evolution to TCC (2001–2005)

Bellard expanded and deobfuscated OTCC into TCC, hosted on Savannah (nongnu.org)
starting in 2002. Key milestones:

| Date | Event |
|------|-------|
| 2001 | OTCC wins IOCCC |
| 2002 | TCC project formalized on Savannah |
| 2004 | TCC Boot Loader: compiles and boots Linux kernel from source in <15 seconds |
| 2005 | v0.9.23: Windows support added |
| 2009 | v0.9.25: x86-64 support (Shinichiro Hamaji); source split into tcc.h/libtcc.c/tccpp.c/tccgen.c/tcc.c |
| 2013 | v0.9.26: ARM hardfloat, VLAs, libtcc API improvements |
| 2017 | v0.9.27: ARM64 target (Edmund Grimley Evans), improved ARM/x86-64 ABI, partial MIT relicensing |
| 2024 | v0.9.28rc: RISC-V target (Michael Matz), native macOS support, DWARF debug info |

### Post-Bellard Maintenance

Bellard stepped away from active TCC development around 2012. The project is now
community-maintained, primarily through grischka's "mob" branch on repo.or.cz.
Key post-Bellard contributors include grischka (PE support, Android, backtraces),
Herman ten Brugge (bounds checker, DWARF), Michael Matz (RISC-V, macOS, shared
libs), Danny Milosavljevic (ARM/RISC-V assemblers), Edmund Grimley Evans (ARM64),
and Thomas Preud'homme (ARM improvements).

### Relationship to Other Bellard Projects

TCC shares Bellard's design philosophy of minimal footprint and self-containment
with his other projects: FFmpeg (2000), QEMU (2003), TinyGL (2002). The TCC Boot
Loader demonstrated practical synergy — using TCC to compile a Linux kernel at
boot time. QEMU's dynamic binary translation and TCC's direct code generation
reflect similar interests in fast native code emission.

## Compiler Architecture

### One-Pass, No IR

TCC has **no intermediate representation**. It performs single-pass compilation:
the parser directly drives code generation as it processes the token stream. There
is no AST construction, no SSA form, no optimization passes. The compilation
pipeline is:

```
Source → Preprocessor (tccpp.c) → Parser+CodeGen (tccgen.c) → Backend (*-gen.c) → Output (tccelf.c/tccpe.c/tccmacho.c)
```

The entry point is `tccgen_compile()`, which calls `decl(VT_CONST)` to parse
top-level declarations. Functions are compiled as they are encountered — the
parser calls code generation functions inline during parsing.

### The Value Stack (SValue / vtop)

The central data structure connecting parsing to code generation is the **value
stack** (`vstack`/`vtop`), a stack of `SValue` entries (max 512 deep). Each
`SValue` represents an expression result and tracks:

```c
typedef struct SValue {
    CType type;           /* C type of the value */
    unsigned short r;     /* register + flags (VT_CONST, VT_LOCAL, VT_LVAL, etc.) */
    unsigned short r2;    /* second register for long long */
    union {
        struct { int jtrue, jfalse; };  /* forward jump targets */
        CValue c;                       /* constant value */
    };
    union {
        struct { unsigned short cmp_op, cmp_r; };  /* comparison operation */
        struct Sym *sym;                            /* symbol reference */
    };
} SValue;
```

The `r` field encodes where the value lives using `VT_*` flags:
- `VT_CONST` (0x30): compile-time constant (value in `c`)
- `VT_LOCAL` (0x32): stack-relative offset
- `VT_LLOCAL` (0x31): lvalue on stack (pointer to the value is on stack)
- `VT_CMP` (0x33): result of a comparison, in CPU flags
- `VT_JMP`/`VT_JMPI` (0x34/0x35): boolean result as conditional jump
- Register number (0–N): value is in that hardware register
- `VT_LVAL` (0x100): value is an lvalue (needs dereference)
- `VT_SYM` (0x200): has an associated symbol

Operations on the value stack (`vpush`, `vpop`, `vswap`, `vrotb`, `vrott`,
`vdup`) manipulate operands. The parser pushes operands onto the vstack, then
calls `gen_op()` which pops operands and pushes the result.

### Expression Parsing

Expressions use recursive descent with separate functions per precedence level:

```
gexpr() → expr_eq() → expr_cond() → expr_lor() → expr_land()
  → expr_or() → ... → expr_cmp() → expr_shift() → expr_sum()
  → expr_prod() → unary()
```

Each level calls the next-higher-precedence function, then loops on its own
operators, calling `gen_op()` to emit code after each binary operator. This is
standard operator-precedence parsing via recursive descent. `unary()` handles
literals, identifiers, prefix operators, casts, sizeof, function calls, etc. — it
is the largest single function in TCC at ~800 lines.

### Statement Parsing

`block()` handles statements: `if`, `while`, `for`, `do`, `switch`, `return`,
`goto`, labels, compound blocks (`{}`), and expression statements. Control flow
is compiled by emitting forward jumps (via `gjmp()`) and patching them later
(via `gsym()`). This is the standard backpatching technique for single-pass
compilation.

### Register Allocation

TCC uses a simple, greedy register allocator driven by the value stack:

1. `gv(rc)` — "get value": ensures the top-of-stack value is in a register
   matching register class `rc`. If already in a suitable register, no-op. If a
   constant, lvalue, or wrong-class register, emits a `load()` into a free
   register.

2. `get_reg(rc)` — finds a free register of class `rc` by scanning the vstack.
   If none free, spills the bottom-most vstack entry using that register class
   (important: bottom-first to avoid spilling values needed by the current
   operation).

3. `save_reg(r)` — spills a register to a stack temporary by emitting a `store()`
   and updating the vstack entry to `VT_LOCAL`.

There is no liveness analysis, no interference graph, no graph coloring. The
"allocation" is purely demand-driven: when an operation needs a register, it gets
one, possibly spilling. The vstack acts as a virtual register file.

### Backend Interface

Each target architecture implements a `-gen.c` file providing these functions:

| Function | Purpose |
|----------|---------|
| `load(r, sv)` | Load SValue into register r |
| `store(r, sv)` | Store register r to SValue location |
| `gen_opi(op)` | Integer binary operation (vtop[-1] op vtop) |
| `gen_opf(op)` | Float binary operation |
| `gen_cvt_itof(t)` | Convert integer to float |
| `gen_cvt_ftoi(t)` | Convert float to integer |
| `gen_cvt_ftof(t)` | Convert between float types |
| `gfunc_call(nb_args)` | Emit function call |
| `gfunc_prolog(sym)` | Emit function prologue |
| `gfunc_epilog()` | Emit function epilogue |
| `gjmp(t)` | Emit unconditional jump |
| `gjmp_cond(op, t)` | Emit conditional jump |
| `gfunc_sret(...)` | Struct return convention |

Each backend also defines `NB_REGS`, register classes (`RC_INT`, `RC_FLOAT`,
etc.), and the `reg_classes[]` array mapping register numbers to their classes.

Code is emitted directly into a `Section` buffer via `g()` (emit byte), `o()`
(emit dword), `gen_le32()`, etc. The `ind` variable tracks the current output
position.

### The Preprocessor

`tccpp.c` (4,026 lines) implements a full C preprocessor with:
- `#include` with search paths and `#include_next`
- `#define` with function-like macros and variadic `__VA_ARGS__`
- `#if`/`#ifdef`/`#elif`/`#else`/`#endif` conditional compilation
- `#pragma once`, `#pragma pack`, `#pragma comment(lib,...)`
- Stringification (`#`) and token pasting (`##`)
- Predefined macros (`__FILE__`, `__LINE__`, `__DATE__`, `__COUNTER__`, etc.)

The tokenizer and preprocessor are integrated — `next()` returns the next
preprocessed token, expanding macros on the fly.

### Built-in Assembler

`tccasm.c` (1,466 lines) implements a GAS-compatible assembler supporting inline
`asm()` statements and standalone `.S` files. Architecture-specific opcode tables
and encoding live in `*-asm.c` / `*-asm.h` files:
- i386/x86-64: 1,757 / 2,628 lines (full x86 instruction set including SSE)
- ARM: 3,092 lines
- ARM64: 94 lines (minimal)
- RISC-V: 2,628 lines

### Built-in Linker

TCC includes its own linker, supporting three output formats:
- **ELF** (tccelf.c, 4,116 lines): Linux/BSD executables, shared libraries, object files
- **PE/COFF** (tccpe.c, 2,114 lines): Windows executables and DLLs (by grischka)
- **Mach-O** (tccmacho.c, 2,476 lines): macOS executables

The linker handles relocations, symbol resolution, GOT/PLT generation, dynamic
linking, and debug section generation. For `-run` mode, `tccrun.c` (1,556 lines)
allocates executable memory, relocates code in-place, and calls `main()`.

### Debug Info

`tccdbg.c` (2,676 lines) generates both STABS and DWARF debug information,
selectable via `-gstabs` or `-gdwarf`. Also supports `-bt` for runtime backtrace
generation and test coverage instrumentation (`-ftest-coverage`).

## Source File Map

| File | Lines | Purpose |
|------|------:|---------|
| tccgen.c | 8,920 | Parser and code generator (the core) |
| tccelf.c | 4,116 | ELF linker |
| tccpp.c | 4,026 | Preprocessor and tokenizer |
| elf.h | 3,324 | ELF format definitions |
| arm-asm.c | 3,092 | ARM assembler |
| tccdbg.c | 2,676 | Debug info generation (STABS/DWARF) |
| riscv64-asm.c | 2,628 | RISC-V assembler |
| c67-gen.c | 2,543 | TMS320C67xx DSP code generator |
| tccmacho.c | 2,476 | Mach-O linker |
| arm-gen.c | 2,385 | ARM code generator |
| x86_64-gen.c | 2,314 | x86-64 code generator |
| libtcc.c | 2,272 | Library API, option parsing, file handling |
| arm64-gen.c | 2,209 | ARM64 code generator |
| tccpe.c | 2,114 | PE/COFF linker (Windows) |
| tcc.h | 2,016 | Main header (all types, flags, prototypes) |
| i386-asm.c | 1,757 | x86 assembler |
| tccrun.c | 1,556 | In-memory execution (`-run` mode) |
| tccasm.c | 1,466 | GAS-compatible assembler core |
| riscv64-gen.c | 1,434 | RISC-V code generator |
| i386-gen.c | 1,306 | i386 code generator |
| dwarf.h | 1,046 | DWARF format definitions |
| tcccoff.c | 951 | COFF output (for C67xx DSP) |
| il-gen.c | 657 | CIL/.NET code generator (bit-rotted since 2003) |
| tcctools.c | 651 | Built-in ar and impdef tools |
| tcc.c | 428 | Main program (command-line driver) |

## CPU Architecture Support

### In-Tree Targets

| Target | Files | Registers | Added By | Status |
|--------|-------|----------:|----------|--------|
| i386 | i386-gen.c, i386-asm.c, i386-link.c | 5 (eax,ecx,edx,ebx,st0) | Bellard (original) | Mature |
| x86-64 | x86_64-gen.c, x86_64-asm.h, x86_64-link.c | ~16 | Shinichiro Hamaji (v0.9.25) | Mature |
| ARM | arm-gen.c, arm-asm.c, arm-link.c | — | Bellard + Daniel Glöckner | Mature |
| ARM64 | arm64-gen.c, arm64-asm.c, arm64-link.c | — | Edmund Grimley Evans (v0.9.27) | Working |
| RISC-V 64 | riscv64-gen.c, riscv64-asm.c, riscv64-link.c | — | Michael Matz (v0.9.28) | Working |
| TMS320C67xx | c67-gen.c, c67-link.c | 24 | Bellard (2001) | Experimental, mob branch only |
| CIL/.NET | il-gen.c | 3 (stack) | Bellard (2002) | Dead (`#error` since 2003) |

### Notable Forks

| Target | Fork / Author | Notes |
|--------|---------------|-------|
| ARM Thumb | Erlend Sveen | tinycc fork |
| RISC-V 32 | Sam Ellicott | tcc-riscv32 |
| Transputer | David Smith (agentdavo) | tinycc-transputer |
| 65816 (SNES) | nArnoSNES | tcc-65816, based on v0.9.23 |
| PE-UEFI ARM64 | Andrei Warkentin | UEFI targets for x86_64, ARM64, IA32, ARM |
| MIPS | lhzhang | tinycc-mips |
| Global RegAlloc | Sebastian Falbesoner | TCCLS proof of concept |
| Softfloat | Giovanni Mascellani | Cross-compilation without host FP |
| Optimized i386 | Jason Hood | Peephole optimizations |

### Cross-Compilation

TCC supports cross-compilation via `--targetcpu` and `--cross-prefix` configure
options, and `-m32`/`-m64` flags for x86. A known limitation: TCC performs target
floating-point arithmetic using the host's FP hardware, which can produce
incorrect results when cross-compiling between architectures with different FP
semantics.

## libtcc API

TCC is usable as a library (`libtcc`) for JIT compilation and embedding:

```c
TCCState *s = tcc_new();
tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
tcc_compile_string(s, "int add(int a, int b) { return a + b; }");
tcc_relocate(s);
int (*add)(int, int) = tcc_get_symbol(s, "add");
printf("%d\n", add(3, 4));  // prints 7
tcc_delete(s);
```

Key API functions: `tcc_new`, `tcc_delete`, `tcc_compile_string`, `tcc_add_file`,
`tcc_relocate`, `tcc_get_symbol`, `tcc_add_symbol`, `tcc_run`, `tcc_set_options`.

## Known Users

Notable software that builds with TCC: GMP, GNU Make, GNU AWK, MPFR, MPC,
SQLite, zlib, mpv, st (suckless terminal), GNU Mes (bootstrapping). TCC can also
compile itself.

## Key Design Tradeoffs

1. **Speed over optimization**: No IR means no optimization passes, but
   compilation is extremely fast. Generated code is roughly equivalent to
   `gcc -O0`.

2. **Simplicity over completeness**: The single-pass model means some C features
   (complex types, some C11/C23) are difficult to add. Forward references work
   through backpatching rather than multi-pass resolution.

3. **Self-containment over reuse**: TCC includes its own preprocessor, assembler,
   linker, and runtime — no external tools needed. This enables `-run` mode and
   standalone use on minimal systems.

4. **Value stack over SSA/register file**: The vstack approach avoids the
   complexity of an IR but makes cross-architecture optimization essentially
   impossible. Each backend generates code in isolation.

## References

- Bellard's TCC page: https://bellard.org/tcc/
- OTCC: https://bellard.org/otcc/
- Official git: https://repo.or.cz/tinycc.git
- GitHub mirror: https://github.com/TinyCC/tinycc
- Savannah project: https://savannah.nongnu.org/projects/tinycc
- IOCCC 2001 entry: https://www.ioccc.org/2001/bellard/index.html
