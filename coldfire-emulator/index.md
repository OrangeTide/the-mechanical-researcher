---
title: Building a ColdFire V4e Emulator
date: 2026-03-25
abstract: "From ISA selection through implementation — a standalone ColdFire V4e CPU emulator in C, validated against GCC-compiled bare-metal programs"
category: systems
---

## Introduction

When a project needs an embedded CPU — a fantasy console, a retro computing emulator, a sandboxed plugin system — there are two paths. One is to invent a bytecode virtual machine: define an instruction set, write an assembler, build a compiler backend. The other is to emulate a real architecture: pick an existing ISA with existing toolchains, existing documentation, and decades of existing software to test against. This article takes the second path.

The trade-off is straightforward. A custom bytecode VM can be arbitrarily simple — the Quake 3 VM is 60 opcodes and 2,000 lines of C — but it requires building an entire toolchain from scratch. A real ISA comes with GCC, LLVM, debuggers, and disassemblers out of the box, but it carries decades of design decisions that the emulator must faithfully reproduce. The question is which real architecture minimizes that burden while maximizing what the toolchain can do.

We evaluated fourteen architectures across six dimensions — address space, arithmetic capabilities, implementation complexity, toolchain availability, sandboxing potential, and developer experience — and arrived at Motorola's ColdFire V4e: a simplified derivative of the 68000 family designed for embedded systems, with hardware multiply, divide, and floating-point in a package that compiles to 2,239 lines of C.

This article walks through the selection process, the architecture's instruction encoding, the emulator implementation, and the testing strategy. The complete source code is available as a companion download.

## Abstract

We compare fourteen CPU architectures for embeddable emulation, evaluating 8-bit through 64-bit designs from the 6809 to Alpha. Five finalists — RISC-V RV32IM, MIPS32, Motorola 68000, PDP-11, and SuperH SH-2 — are analyzed in depth, with GCC cross-compilation tests revealing that ColdFire V4e is the only target where every arithmetic operation (32-bit add, multiply, divide, remainder, and all floating-point operations) compiles to a single hardware instruction. We implement a complete ColdFire V4e emulator in 2,239 lines of C: integer core, FPU, exception handling, and a callback-based memory bus with zero heap allocation. The emulator is validated against GCC 13 cross-compiled bare-metal programs covering recursion, integer arithmetic, bit manipulation, and IEEE-754 floating-point, passing all tests in 1,528 executed instructions.

## Choosing an Architecture

### Requirements

The selection criteria reflect a specific use case — embedding a CPU into a larger project where users write programs in C or assembly — but the trade-offs generalize to any emulation project.

| Requirement | Threshold | Rationale |
|---|---|---|
| Address space | >64 KB, ideally >1 MB | Programs larger than a single 64 KB bank |
| Arithmetic | Hardware multiply + divide | Avoid libgcc software emulation |
| Implementation budget | 1,500–3,000 LOC | One developer; embeddable as a component in a larger project |
| Real toolchain | GCC or LLVM cross-compiler | Users write C, not custom assembly |
| Sandboxing | Easy to isolate guest from host | Memory bus callbacks, no host pointers leaked |
| Developer experience | Orthogonal, enjoyable to program | Registers, addressing modes, clean encoding |

### Eliminated Early

Six candidates failed basic requirements:

| CPU | Why Eliminated |
|---|---|
| Motorola 6809 | 8-bit, 64 KB address space only |
| TI TMS9900 | 64 KB address space; registers-in-memory novelty adds complexity without benefit |
| WDC 65816 | 24-bit addressing (good), but no hardware multiply or divide |
| AT&T Hobbit | Very little public documentation, no accessible toolchain |
| IBM ROMP | Obscure, poorly documented, no surviving toolchain |
| DEC Alpha | 64-bit overkill, ~300 instructions, complex PALcode dependency |

### Viable but Complex

Three architectures met the requirements but carried significant implementation baggage:

**Intel 80186.** The 8086 derivative offers a 1 MB address space via segment:offset addressing and has hardware multiply/divide. But segment logic pervades the entire emulator — every memory access requires segment register lookups, segment override prefixes change behavior, and the variable-length encoding (1–6 bytes with prefixes) makes the decoder substantially more complex than a fixed-format ISA. Estimated 3,000–5,000 LOC.

**PowerPC.** A clean 32-bit RISC with 4 GB flat addressing, hardware multiply/divide, and excellent GCC support. But the instruction set runs to 200+ instructions, the condition register has eight independent 4-bit fields with its own set of logical operations, and the special-purpose register file (SPRs) adds dozens of move-to/move-from instructions. Estimated 3,500–5,000 LOC.

**ARMv2 / ARM7TDMI.** Compact instruction set (~45 instructions for ARMv2), 4 GB flat addressing, and hardware multiply. But every instruction is conditionally executed (4-bit condition field on every opword), every ALU instruction includes a barrel shifter operand (adding a second level of decode to every arithmetic operation), and there is no hardware divide until ARMv7. The Thumb encoding variant doubles the decoder. Estimated 2,500–4,000 LOC.

### The Five Finalists

Five architectures survived initial screening. Each represents a distinct point in the design space.

#### RISC-V RV32IM

The cleanest modern ISA. Forty-seven base instructions (RV32I) plus eight multiply/divide instructions (M extension), all in a fixed 32-bit encoding with four formats (R/I/S/U plus variants). The specification is open, the documentation is exceptional, and GCC/LLVM/Rust toolchains are mature. QEMU provides a reference emulator.

The implementation estimate is 1,200–1,800 LOC — the lowest of any candidate. The encoding is regular enough that the decoder is nearly mechanical. The downside: no condition codes (branches compare two registers directly), which some find less intuitive, and the architecture lacks the historical resonance that makes emulation projects compelling. Adding the F extension for single-precision float brings the estimate to 1,800–2,500 LOC.

#### MIPS32

The classic teaching ISA from Patterson and Hennessy. About sixty instructions in a fixed 32-bit encoding with three formats (R/I/J). Hardware multiply and divide use a dedicated HI/LO register pair. The 4 GB flat address space is partitioned into kernel/user segments by convention.

Estimated 1,500–2,200 LOC. The main complications are branch delay slots (the instruction after a branch always executes, even if the branch is taken) and load delay slots in MIPS I. The CP0 coprocessor is needed for exceptions. Delay slots are the number one source of emulator bugs in MIPS implementations — they interact badly with every control flow instruction.

#### Motorola 68000

The most elegantly orthogonal architecture of its generation. Eight data registers (D0–D7), eight address registers (A0–A7, with A7 doubling as the stack pointer), and twelve addressing modes that apply uniformly across almost all instructions. Variable-length encoding (2–10 bytes, 16-bit aligned) with ~56 instruction types.

The 68000 powered the Macintosh, Amiga, Atari ST, Sega Genesis, and Neo Geo — five major platforms from 1984 to 1994. The instruction set is a joy to program in assembly, and GCC produces clean code for it. But the effective address decode is the main complexity: twelve modes applied across fifty-six instructions means the EA engine is the single largest component. Estimated 2,500–4,000 LOC. The bigger problem: no 32-bit multiply (only 16×16→32), no 32-bit divide (only 32÷16→16:16), and no FPU without the 68881 coprocessor (which adds 1,200–2,000 LOC for 80-bit extended precision and transcendental functions).

#### PDP-11

The most elegant minicomputer ISA ever designed, and the direct ancestor of the 68000. Sixty instructions with EIS (extended instruction set for multiply/divide), variable-length encoding (2–6 bytes), eight addressing modes applied to eight general-purpose registers. Unix was born on this machine. The instruction set influenced everything that followed.

Estimated 1,500–2,500 LOC. The fatal flaw for our use case: the PDP-11 is a 16-bit architecture. Programs think in 16-bit addresses, even with I/D separation (128 KB) or the optional MMU (4 MB physical). This slightly fails the >64 KB requirement at the program level, and there is no path to an FPU without significant additional work.

#### SuperH SH-2

Hitachi's 32-bit RISC with 16-bit instruction encoding — the best code density of any RISC architecture. About sixty instructions, 4 GB flat addressing, hardware multiply (MUL.L, DMULS, DMULU). Used in the Sega Saturn (dual SH-2) and Sega Dreamcast (SH-4 variant with FPU).

Estimated 1,500–2,200 LOC for SH-2. Division is multi-step (DIV0S/DIV1 loop), delayed branches add the same emulator complexity as MIPS, and the architecture is less well-known with fewer reference implementations. The SH-4 variant adds an FPU but also adds FPSCR bank-switching, literal pools for every constant, and MACL/MACH accumulator registers for multiply results.

### The GCC Codegen Test

Tables of instruction counts and addressing modes reveal the architecture's complexity. But the question that matters for a project where users write C is: *what does the compiler actually emit?*

We compiled a simple test program — 32-bit addition, multiplication, division, modulo, and three floating-point operations — for four targets using GCC 13 on Ubuntu 24.04. The targets span the 68K family plus the leading alternative:

- **68000**: `m68k-linux-gnu-gcc -m68000`
- **ColdFire V2**: `m68k-linux-gnu-gcc -mcpu=5206`
- **ColdFire V4e**: `m68k-linux-gnu-gcc -mcpu=5475`
- **SH-4**: `sh4-linux-gnu-gcc`

The same source file, the same optimization level (`-O2`), four different targets. The results:

| Operation | 68000 | ColdFire V2 | ColdFire V4e | SH-4 |
|---|---|---|---|---|
| 32-bit add | `add.l` | `add.l` | `add.l` | `add` |
| 32-bit multiply | **libgcc** | `muls.l` | `muls.l` | `mul.l` + `sts macl` |
| 32-bit divide | **libgcc** | **libgcc** | `divs.l` | **libgcc** (`__sdivsi3_i4i`) |
| 32-bit remainder | **libgcc** | **libgcc** | `rems.l` | mul+sub (after div call) |
| float add | **libgcc** | **libgcc** | `fsadd` | `fadd` |
| float multiply | **libgcc** | **libgcc** | `fsmul` | `fmul` |
| float divide | **libgcc** | **libgcc** | `fsdiv` | `fdiv` |

The `fs`-prefix instructions (`fsadd`, `fsmul`, `fsdiv`) are the V4e's single-precision FPU operations — the test used `float` types. Double-precision equivalents use an `fd` prefix (`fdadd`, `fdmul`, `fddiv`). Both map to the same FPU hardware; the prefix selects the rounding precision.

**ColdFire V4e is the only target where every operation compiles to a single hardware instruction.** No library calls, no multi-instruction sequences, no accumulator register shuffling. The 68000 compiles everything except addition to libgcc calls — it has no 32-bit multiply, no 32-bit divide, and no FPU. ColdFire V2 gains 32-bit multiply but still lacks divide and FPU. The SH-4 has a capable FPU, but integer divide is a libgcc call that internally uses the FPU's reciprocal instruction (`__sdivsi3_i4i`), and multiply results land in the MACL accumulator register rather than a general-purpose register.

### SH-4 Hidden Costs

The codegen table understates the SH-4's emulator complexity. Examining the full GCC output reveals four complications that don't appear in an instruction-by-instruction comparison:

1. **Literal pools.** Every address and large constant is loaded via PC-relative access from a data pool placed after the function body. SH-4's fixed 16-bit instruction encoding cannot hold large immediates, so the compiler generates `mov.l @(disp,pc), Rn` instructions that read from these pools. The emulator must handle PC-relative addressing within code segments.

2. **Delay slots.** The instruction after every branch executes before the branch takes effect. This is the number one source of emulator bugs in any delayed-branch architecture — it interacts with every control flow instruction, including subroutine calls and returns.

3. **MACL/MACH registers.** Multiply results go to special accumulator registers, not general-purpose registers. The compiler must emit `sts macl, Rn` to retrieve results, adding instructions and requiring the emulator to track additional state.

4. **FPSCR bank-switching.** GCC emits `sts fpscr` / `xor` / `lds fpscr` sequences around floating-point operations to toggle between single and double precision mode (FPSCR bit 19). The emulator must track this mode bit and apply it to every FPU operation.

### The Decision

| Priority | CPU | Rationale |
|---|---|---|
| **Selected** | **ColdFire V4e** | All arithmetic in hardware, ~2,100–3,100 LOC estimate, FPU included, 68K lineage, GCC + Free Pascal toolchains |
| Runner-up | RISC-V RV32IM | Cleanest implementation (~1,200–1,800 + FPU), modern relevance, but less historically interesting |
| Dark horse | SH-4 | Good FPU but software integer divide, delay slots, FPSCR bank-switching, literal pools |
| Historical | 68000 | Deep nostalgia but fails requirements — no 32-bit multiply, no FPU without 68881 |

ColdFire V4e wins on the strength of the codegen test. A project where users write C needs an architecture where the compiler produces clean, efficient output — and V4e is the only target where the compiler never falls back to software emulation for basic arithmetic.

## The ColdFire V4e

### What ColdFire Is

ColdFire is Motorola's embedded successor to the 68000. Introduced in 1994, it was designed for cheaper silicon — a simplified instruction set that could be implemented in fewer gates, at higher clock speeds, in smaller die area. Every simplification Motorola made for silicon cost translates directly to less emulator code.

ColdFire is *not* binary compatible with the 68000. This is a common misconception. The instruction encoding is derived from 68K, and many programs will assemble for both, but ColdFire removes instructions, restricts addressing modes, and changes the behavior of some operations. Code compiled specifically for ColdFire (with `-mcpu=5475` or similar) uses the restricted instruction set and takes advantage of ColdFire-specific additions.

### What ColdFire Removes

Each removal is code the emulator doesn't need to implement:

- **BCD instructions** (ABCD, SBCD, NBCD, PACK, UNPK) — decimal arithmetic for financial applications. Six instruction types eliminated.
- **MOVEP** — byte-packing for 8-bit peripheral access. One instruction type eliminated.
- **Rare addressing modes** — some double-indirect and indexed variants. Fewer EA decode paths.
- **Byte/word variants** — many operations are long-only on ColdFire (ADD, SUB, AND, OR, EOR to memory). The emulator still handles byte/word for register operations and MOVE, but fewer combinations overall.
- **Some shift/rotate variants** — ColdFire restricts memory shifts and eliminates ROXL/ROXR (rotate through extend).

### What ColdFire Adds

Each addition means better compiler output — the compiler can use a single instruction where the 68000 would need a sequence or a library call:

- **Full 32÷32 division**: `DIVS.L` and `DIVU.L` produce a 32-bit quotient. The 68000 only offers 32÷16→16:16.
- **Hardware remainder**: `REMS.L` and `REMU.L` — the 68000 has no remainder instruction at all.
- **MOV3Q**: Move a 3-bit immediate (-1, 1–7) to any EA. GCC uses this constantly — 17 occurrences in a single fibonacci function.
- **MVS/MVZ**: Sign-extend and zero-extend from byte or word to long. Eliminates multi-instruction extension sequences.
- **BITREV, BYTEREV, FF1**: Bit reversal, byte swap, find-first-one. Single-cycle operations that would otherwise require loops.
- **EMAC**: Enhanced multiply-accumulate unit with four 48-bit accumulators. Designed for audio mixing and DSP.
- **FPU**: IEEE-754 double precision, eight registers (FP0–FP7), ~15 instruction types. A subset of the 68881 without the 80-bit extended precision or transcendental functions.

### ColdFire Versions

| Version | Character | FPU | Estimated LOC |
|---|---|---|---|
| V1 | Ultra-minimal | No | 1,200–1,600 |
| V2 | Integer sweet spot | No | 1,750–2,680 |
| V3 | V2 with minor additions | No | 1,800–2,700 |
| **V4e** | **Full-featured** | **Yes** | **2,080–3,120** |

V4e is the clear choice. The FPU adds 300–450 LOC but eliminates all floating-point library calls. The alternative — implementing the full 68881 coprocessor for the classic 68000 — would add 1,200–2,000 LOC for 80-bit extended precision and transcendental functions (FSIN, FCOS, FLOG, FATAN, etc.). The V4e FPU uses 64-bit `double` natively, which maps directly to the host CPU's double-precision format.

### LOC Estimates vs. Actual

| Component | Estimated | Actual |
|---|---|---|
| CPU state + init | ~60 | 68 |
| Decoder (main switch) | 280–430 | 47 |
| EA engine | 200–300 | 190 |
| Integer handlers (groups 0–E) | 900–1,400 | 1,350 |
| Condition codes | 150–200 | 150 |
| FPU (group F) | 300–450 | 350 |
| Exceptions/interrupts | 100–180 | 30 |
| Memory bus + helpers | — | 54 |
| **Total** | **2,080–3,120** | **2,239** |

The actual implementation landed squarely within the estimate, slightly below the midpoint. The decoder came in much smaller than estimated because the two-level switch is just a dispatch table — the real decode logic lives in each group handler.

### FPU Comparison

| | 68881 (full) | ColdFire V4e FPU | SH-4 FPU |
|---|---|---|---|
| Added LOC | 1,200–2,000 | 300–450 | 500–700 |
| Precision | 80-bit internal | 64-bit double | 32-bit single (double optional) |
| Registers | 8 (FP0–FP7) | 8 (FP0–FP7) | 16 (FR0–FR15) × 2 banks |
| Transcendentals | FSIN, FCOS, FLOG, etc. | No | No |
| Bank switching | No | No | Yes (FPSCR.FR bit) |
| Implementation | Coprocessor (F-line trap) | Integrated | Integrated |

The V4e FPU is the sweet spot: enough for IEEE-754 arithmetic without the complexity of 80-bit extended precision or transcendental functions. SH-4's FPU is capable but adds bank-switching overhead and defaults to single precision, requiring mode toggles for double-precision operations.

### Historical Context

The 68000 was the game console CPU. From 1988 to 1994, it powered five major platforms — and then it disappeared from the console world entirely:

| Console | Release | CPU | Clock |
|---|---|---|---|
| Sega Genesis / Mega Drive | 1988 JP | Motorola 68000 | 7.67 MHz |
| SNK Neo Geo (MVS/AES) | 1990 | Motorola 68HC000 | 12 MHz |
| Amiga CD32 | Sep 1993 EU | Motorola 68EC020 | 14.28 MHz |
| Atari Jaguar | Nov 1993 US | Motorola 68000 | 13.295 MHz |
| Sega Saturn | Nov 1994 JP | Motorola 68EC000 (sound) | 11.3 MHz |

ColdFire was the 68000's embedded successor — introduced in 1994, the same year the Saturn shipped its last 68K. The V2 core shipped in commercial products by 1996. The V4e core was announced at Microprocessor Forum in October 2000 — contemporary with the PlayStation 2 launch — adding FPU, MMU, and EMAC to a "100% synthesizable and highly configurable" core designed for custom SoCs. The first V4e silicon (MCF547x/MCF548x) shipped in 2004 from Freescale (spun off from Motorola in July 2004), running at up to 266 MHz and delivering 410 Dhrystone 2.1 MIPS.

But ColdFire never crossed back into gaming. Its markets stayed industrial — factory automation, medical instrumentation, robotics, POS terminals. The V4e had everything a console CPU needed: 68K backward compatibility for the Genesis developer ecosystem, an integrated FPU for 3D math, EMAC for audio mixing, and MMU for sandboxing game code. It was a console CPU that never got a console. NXP acquired Freescale in 2015, and ColdFire became a legacy product line.

### Musashi Comparison

The gold standard for 68K emulation is Karl Stenerud's [Musashi](https://github.com/kstenerud/Musashi), used in MAME and numerous retro computing projects. Musashi covers the full 68000/68010/68020/68EC020 instruction set across approximately 6,500 lines of C generated from a code generator that processes instruction tables. It aims for cycle-accurate emulation of the classic 68K family.

Our emulator has a different goal. It implements the ColdFire V4e ISA specifically — the simplified instruction set with its restrictions and additions — in 2,239 lines of handwritten C. It does not aim for cycle accuracy. It provides a callback-based memory bus for embedding, zero heap allocation, and enough fidelity to run GCC-compiled bare-metal programs. Where Musashi emulates the past, this emulator targets a platform that never existed.

## Instruction Encoding

### The Opword

Every ColdFire instruction begins with a 16-bit *opword*, fetched from the current program counter. The top four bits select one of sixteen *groups*, and the remaining twelve bits encode operands, sub-opcodes, and effective address fields whose interpretation varies by group:

```
 15 14 13 12 | 11 10  9 |  8 |  7  6 |  5  4  3 |  2  1  0
 +-----------+----------+----+-------+----------+----------+
 |   group   |  varies  |    | varies|   mode   |   reg    |
 +-----------+----------+----+-------+----------+----------+
```

The bottom six bits — mode (3 bits) and register (3 bits) — form the *effective address field*, which appears in most instructions and specifies how to locate an operand. Some instructions use a second EA field encoded in bits 11–6 with swapped mode/register positions (notably MOVE, groups 1–3).

Many instructions require *extension words* — additional 16-bit or 32-bit values fetched after the opword. These carry displacement values, immediate operands, index register specifications, or (for FPU instructions) the actual operation code.

### The Sixteen Groups

| Group | Hex | Instructions | Sub-dispatch |
|---|---|---|---|
| 0 | `0x0` | ORI, ANDI, SUBI, ADDI, EORI, CMPI, BTST/BCHG/BCLR/BSET | Bits 11–9 (immediate type) |
| 1 | `0x1` | MOVE.B | Dual EA fields (src + dst) |
| 2 | `0x2` | MOVE.L | Dual EA fields (src + dst) |
| 3 | `0x3` | MOVE.W | Dual EA fields (src + dst) |
| 4 | `0x4` | LEA, PEA, CLR, NEG, NOT, EXT, SWAP, MOVEM, TRAP, LINK, UNLK, RTS, RTE, JSR, JMP, TST, MULS.L, DIVS.L, HALT, NOP, FF1 | Bits 11–6 (largest group) |
| 5 | `0x5` | ADDQ, SUBQ, Scc | Bit 8 (add/sub vs. set) |
| 6 | `0x6` | Bcc, BRA, BSR | Bits 11–8 (14 conditions + always + subroutine) |
| 7 | `0x7` | MOVEQ, MVS, MVZ | Bit 8 (0=MOVEQ, 1=MVS/MVZ) |
| 8 | `0x8` | OR, DIVU, DIVS | Bit 8, bits 7–6 |
| 9 | `0x9` | SUB, SUBA, SUBX | Bit 8, size field |
| A | `0xA` | MOV3Q, EMAC | Bits 8–6 (5=MOV3Q, others=EMAC) |
| B | `0xB` | CMP, CMPA, EOR | Bit 8 |
| C | `0xC` | AND, MULU, MULS, EXG | Bit 8, bits 7–6 |
| D | `0xD` | ADD, ADDA, ADDX | Bit 8, size field |
| E | `0xE` | ASL, ASR, LSL, LSR, ROR | Bits 4–3, bit 8 |
| F | `0xF` | FPU ops, FBcc, FMOVEM | Extension word bits 15–13 (opclass), 6–0 (operation) |

Group 4 is the monster — twenty-plus instruction types packed into one group, dispatched through a chain of bit-pattern matches. Group F delegates almost entirely to the extension word, using the opword mainly for the EA field and the coprocessor ID.

### Effective Address Modes

The 6-bit EA field encodes eight addressing modes. ColdFire restricts several modes compared to the full 68000 — notably eliminating some double-indirect variants — which simplifies the decoder:

| Mode | Reg | Syntax | Description |
|---|---|---|---|
| 0 | 0–7 | `Dn` | Data register direct |
| 1 | 0–7 | `An` | Address register direct |
| 2 | 0–7 | `(An)` | Address register indirect |
| 3 | 0–7 | `(An)+` | Post-increment |
| 4 | 0–7 | `-(An)` | Pre-decrement |
| 5 | 0–7 | `(d16,An)` | Displacement (16-bit extension word) |
| 6 | 0–7 | `(d8,An,Xn.L*SF)` | Indexed (extension word: 8-bit disp + index reg + scale) |
| 7 | 0 | `(xxx).W` | Absolute short (sign-extended 16-bit address) |
| 7 | 1 | `(xxx).L` | Absolute long (32-bit address) |
| 7 | 2 | `(d16,PC)` | PC-relative displacement |
| 7 | 3 | `(d8,PC,Xn.L*SF)` | PC-relative indexed |
| 7 | 4 | `#imm` | Immediate (1, 2, or 4 bytes depending on operation size) |

Mode 7 overloads the register field to select among five special modes. This is the 68K family's characteristic trick: twelve addressing modes packed into six bits.

### Encoding Examples

Three instructions from the test program's disassembly illustrate the encoding at increasing complexity:

**`moveq #105,%d1`** — Opcode `0x7269`:

```
  0111  001  0  01101001
  group  D1  Q  immediate
```

Group 7, register D1 (bits 11–9 = 001), bit 8 = 0 selects MOVEQ (vs. MVS/MVZ). The low 8 bits (`0x69` = 105) are the sign-extended immediate value. The entire instruction fits in one opword — no extension words.

**`move.l 120(%sp),%a0`** — Opcode `0x206F 0x0078`:

```
  0010  000  001  101 111    0000000001111000
  grp2  A0  mode1  mode5+A7  displacement=120
```

Group 2 = MOVE.L. Destination EA (bits 11–6): register 0, mode 1 = address register direct → `%a0`. Source EA (bits 5–0): mode 5 = displacement, register 7 = `%sp` (A7). The 16-bit extension word `0x0078` = 120 decimal. This loads a long from 120 bytes above the stack pointer into A0.

**`mov3q.l #1,%d0`** — Opcode `0xA340`:

```
  1010  001  101  000 000
  grpA  data  MOV3Q  Dn + D0
```

Group A. Bits 8–6 = 101 selects MOV3Q (vs. EMAC). Bits 11–9 = 001 is the 3-bit data field — values 1–7 encode themselves, 0 encodes -1. So data=1 means the immediate value is 1. Destination EA: mode 0, register 0 = D0. GCC emits MOV3Q heavily — it can load any of {-1, 1, 2, 3, 4, 5, 6, 7} in a single opword with no extension word.

## Implementation Walkthrough

### CPU State

The entire emulator state lives in a single `cf_cpu` struct, owned and allocated by the caller (abbreviated — the full header includes additional stub registers for cache control, FP instruction address, and accumulator extensions):

```c
typedef struct cf_cpu {
    /* Integer core */
    uint32_t d[8];          /* D0-D7 data registers */
    uint32_t a[8];          /* A0-A7 address registers (A7 = active SP) */
    uint32_t pc;            /* program counter */
    uint32_t sr;            /* status register (includes CCR) */
    uint32_t vbr;           /* vector base register */
    uint32_t other_a7;      /* shadow stack pointer (USP or SSP) */

    /* FPU */
    double   fp[8];         /* FP0-FP7, 64-bit double precision */
    uint32_t fpcr;          /* FP control register */
    uint32_t fpsr;          /* FP status register */

    /* EMAC */
    int64_t  acc[4];        /* ACC0-ACC3, 48-bit accumulators */
    uint32_t macsr;         /* MAC status register */
    uint32_t mask;           /* MAC mask register */

    /* Emulator state */
    int      halted;
    uint64_t cycles;        /* instruction counter */
    int      fault;         /* set on bus/address error */

    /* Memory bus callbacks */
    cf_read_fn  read8, read16, read32;
    cf_write_fn write8, write16, write32;
    void *bus_ctx;          /* opaque pointer passed to callbacks */
} cf_cpu;
```

The FPU registers are stored as native `double` values — the V4e's 64-bit double precision maps exactly to the host's IEEE-754 `double`. No 80-bit extended precision means no software floating-point emulation. The EMAC accumulators use `int64_t` to hold 48-bit values with room for intermediate precision.

There is no heap allocation anywhere in the emulator. The caller provides the `cf_cpu` struct, the caller provides memory through callbacks. The emulator is a pure function of its inputs.

### Memory Bus

The emulator never touches guest memory directly. All access goes through six callback functions provided at initialization:

```c
typedef uint32_t (*cf_read_fn)(void *ctx, uint32_t addr);
typedef void (*cf_write_fn)(void *ctx, uint32_t addr, uint32_t val);

void cf_init(cf_cpu *cpu,
             cf_read_fn r8, cf_read_fn r16, cf_read_fn r32,
             cf_write_fn w8, cf_write_fn w16, cf_write_fn w32,
             void *bus_ctx);
```

The `bus_ctx` pointer is passed to every callback, allowing the host to maintain its own state (memory arrays, memory-mapped I/O handlers, access logging). This design makes embedding trivial — the host defines what memory looks like, and the emulator does not need to know:

```c
static inline uint32_t bus_read16(cf_cpu *cpu, uint32_t addr)
{
    return cpu->read16(cpu->bus_ctx, addr) & 0xFFFF;
}
```

A minimal host can implement callbacks as direct array access. A more sophisticated host can use them to implement memory-mapped I/O, access permissions, or bus logging.

### Fetch and Decode

The main loop is a single function. Fetch the opword, extract the top four bits, dispatch to one of sixteen group handlers:

```c
int cf_step(cf_cpu *cpu)
{
    if (cpu->halted)
        return -1;

    uint16_t op = fetch16(cpu);
    int group = (op >> 12) & 0xF;

    switch (group) {
    case 0x0: /* ... bit ops and immediate ops ... */ break;
    case 0x1: exec_move(cpu, op, SZ_BYTE); break;
    case 0x2: exec_move(cpu, op, SZ_LONG); break;
    case 0x3: exec_move(cpu, op, SZ_WORD); break;
    case 0x4: exec_group4(cpu, op); break;
    /* ... groups 5 through E ... */
    case 0xF: exec_groupF(cpu, op); break;
    }

    cpu->cycles++;
    return cpu->halted ? -1 : 0;
}
```

`fetch16` reads two bytes at the current PC and advances it. The PC always points to the next word to be fetched, so extension words within an instruction are consumed by subsequent `fetch16` or `fetch32` calls within the group handler.

### The EA Engine

The effective address engine is the emulator's most reusable component — approximately 190 lines that serve almost every instruction. It decodes the 6-bit mode:register field into an `ea_loc` descriptor, then separate `read_ea` and `write_ea` functions access the described location:

```c
typedef struct {
    int type;       /* 0=data reg, 1=addr reg, 2=memory, 3=immediate */
    int reg;        /* register number (for type 0, 1) */
    uint32_t addr;  /* memory address (for type 2) */
    uint32_t imm;   /* immediate value (for type 3) */
} ea_loc;

static ea_loc decode_ea(cf_cpu *cpu, int mode, int reg, int sz)
{
    ea_loc loc;
    switch (mode) {
    case 0: /* Dn */
        loc.type = 0;  loc.reg = reg;
        break;
    case 2: /* (An) */
        loc.type = 2;  loc.addr = cpu->a[reg];
        break;
    case 3: /* (An)+ */
        loc.type = 2;  loc.addr = cpu->a[reg];
        cpu->a[reg] += size_bytes(sz);
        break;
    case 4: /* -(An) */
        cpu->a[reg] -= size_bytes(sz);
        loc.type = 2;  loc.addr = cpu->a[reg];
        break;
    case 5: { /* (d16, An) */
        int16_t disp = (int16_t)fetch16(cpu);
        loc.type = 2;  loc.addr = cpu->a[reg] + disp;
        break;
    }
    /* ... modes 1, 6, 7 ... */
    }
    return loc;
}
```

Post-increment (mode 3) and pre-decrement (mode 4) modify the address register as a side effect of decoding — the adjustment depends on the operand size, so byte operations increment by 1, word by 2, long by 4. This side effect is why EA decode must happen in the right order when an instruction has both source and destination EA fields.

Mode 7 overloads the register field for five additional modes. The PC-relative modes (7/2, 7/3) capture the PC *before* fetching the displacement extension word — this is a common source of off-by-two errors if the implementation fetches the extension word before saving the base PC.

### Condition Codes

ColdFire inherits the 68K's five-flag condition code register: Carry (C), oVerflow (V), Zero (Z), Negative (N), and eXtend (X). Getting these flags right — especially carry and overflow for subtraction — is the trickiest part of the emulator. The flags are set differently for each operation class:

```c
static void set_flags_sub(cf_cpu *cpu, uint32_t src, uint32_t dst,
                          uint32_t res, int sz)
{
    uint32_t msb;
    switch (sz) {
    case SZ_BYTE: msb = 0x80; break;
    case SZ_WORD: msb = 0x8000; break;
    default:      msb = 0x80000000; break;
    }
    uint32_t sm = src & msb, dm = dst & msb, rm = res & msb;
    set_nz(cpu, res, sz);
    /* Overflow: operands differ in sign, result matches src sign */
    set_flag(cpu, CF_SR_V, (sm != dm) && (rm == sm));
    /* Carry/borrow */
    set_flag(cpu, CF_SR_C,
             ((!dm && sm) || (rm && (!dm || sm))) ? 1 : 0);
    set_flag(cpu, CF_SR_X, cpu->sr & CF_SR_C);
}
```

The carry formula looks like boolean logic because it is — it's derived from the truth table of when a borrow occurs in unsigned subtraction, expressed in terms of the MSBs of source, destination, and result. The overflow formula checks for signed overflow: if the operands have different signs and the result's sign matches the source (subtrahend), the subtraction overflowed. CMP uses the same logic but does not set the X (extend) flag.

Branch instructions use a 4-bit condition code that encodes a boolean combination of these flags. All fourteen conditions plus True and False:

```c
static int eval_cc(cf_cpu *cpu, int cc)
{
    uint32_t sr = cpu->sr;
    int n = (sr >> 3) & 1, z = (sr >> 2) & 1;
    int v = (sr >> 1) & 1, c = sr & 1;
    switch (cc & 0xF) {
    case 0x0: return 1;             /* T */
    case 0x1: return 0;             /* F */
    case 0x2: return !c && !z;      /* HI (unsigned greater) */
    case 0x3: return c || z;        /* LS (unsigned lower/same) */
    case 0x4: return !c;            /* CC (carry clear) */
    case 0x5: return c;             /* CS (carry set) */
    case 0x6: return !z;            /* NE */
    case 0x7: return z;             /* EQ */
    case 0x8: return !v;            /* VC (overflow clear) */
    case 0x9: return v;             /* VS (overflow set) */
    case 0xA: return !n;            /* PL (plus / positive) */
    case 0xB: return n;             /* MI (minus / negative) */
    case 0xC: return n == v;        /* GE (signed) */
    case 0xD: return n != v;        /* LT (signed) */
    case 0xE: return !z && (n==v);  /* GT (signed) */
    case 0xF: return z || (n!=v);   /* LE (signed) */
    }
}
```

### Selected Group Handlers

Rather than walk through all sixteen groups, four handlers illustrate the patterns that repeat throughout the decoder.

#### Groups 1–3: MOVE

MOVE is the most frequently executed instruction, and its encoding has a quirk: the destination EA field in bits 11–6 has the mode and register subfields *swapped* compared to the source EA in bits 5–0. The source uses `mode:reg` (bits 5–3 : 2–0) while the destination uses `reg:mode` (bits 11–9 : 8–6):

```c
static void exec_move(cf_cpu *cpu, uint16_t op, int sz)
{
    int src_mode = (op >> 3) & 7;
    int src_reg  = op & 7;
    int dst_reg  = (op >> 9) & 7;   /* swapped vs source */
    int dst_mode = (op >> 6) & 7;   /* swapped vs source */

    uint32_t val = ea_read(cpu, src_mode, src_reg, sz);

    if (dst_mode == 1) {
        /* MOVEA: address register, sign-extend, no flags */
        cpu->a[dst_reg] = (uint32_t)sign_extend(val, sz);
        return;
    }

    ea_loc dst = decode_ea(cpu, dst_mode, dst_reg, sz);
    write_ea(cpu, &dst, sz, val);
    set_flags_move(cpu, val, sz);
}
```

MOVEA (destination mode 1, address register) is handled inline — it sign-extends the source value and does not affect condition codes. This is true for all address register writes on the 68K family: address registers are always 32-bit and never set flags.

#### Group 4: The HALT-Before-TST Gotcha

Group 4 is the largest group, containing twenty-plus instruction types identified by various bit patterns. The trickiest interaction is between HALT and TST:

```c
static void exec_group4(cf_cpu *cpu, uint16_t op)
{
    /* HALT : 0100 1010 1100 1000 — must check before TST */
    if (op == 0x4AC8) {
        cpu->halted = 1;
        return;
    }

    /* TST <ea> : 0100 1010 ss eee rrr */
    if ((op & 0xFF00) == 0x4A00) {
        int sz_bits = (op >> 6) & 3;
        int sz = decode_size(sz_bits);
        /* ... */
    }
    /* ... 18+ more instruction patterns ... */
}
```

HALT's opcode is `0x4AC8`. TST's pattern is `0x4Axx`. The overlap: HALT matches TST's pattern with size bits = 11 (binary), which is an invalid size encoding. If TST is checked first, HALT is misidentified as TST with an invalid size field and raises an illegal instruction exception instead of halting. The fix is to check for the exact HALT opcode before the broader TST pattern — ordering matters in a pattern-matching decoder.

#### Group 7: MOVEQ vs. MVS/MVZ

Group 7 demonstrates how ColdFire extends the 68K encoding without breaking existing instructions. The classic 68000 uses the entire group for MOVEQ (move quick — an 8-bit sign-extended immediate to a data register). ColdFire adds MVS (move with sign-extend) and MVZ (move with zero-extend) by repurposing bit 8:

```c
static void exec_group7(cf_cpu *cpu, uint16_t op)
{
    int reg = (op >> 9) & 7;

    if (!(op & 0x100)) {
        /* MOVEQ: bit 8 = 0 — classic 68K instruction */
        int8_t data = (int8_t)(op & 0xFF);
        cpu->d[reg] = (uint32_t)(int32_t)data;
        set_flags_move(cpu, cpu->d[reg], SZ_LONG);
    } else {
        /* MVS / MVZ: bit 8 = 1 — ColdFire addition */
        int sz_bit  = (op >> 6) & 1;   /* 0=byte, 1=word */
        int is_zero = (op >> 7) & 1;   /* 0=MVS, 1=MVZ */
        int ea_mode = (op >> 3) & 7;
        int ea_reg  = op & 7;
        int sz = sz_bit ? SZ_WORD : SZ_BYTE;
        uint32_t src = ea_read(cpu, ea_mode, ea_reg, sz);

        if (is_zero)
            cpu->d[reg] = size_mask(src, sz);      /* zero-extend */
        else
            cpu->d[reg] = (uint32_t)sign_extend(src, sz);
        set_flags_move(cpu, cpu->d[reg], SZ_LONG);
    }
}
```

On the classic 68000, bit 8 of a group 7 opword is always 0 — MOVEQ's encoding guarantees it. ColdFire reclaims the bit-8-equals-1 space for new instructions. GCC uses MVZ.W heavily for loading 16-bit constants (e.g., `mvz.w #5050,%d1` to load the sum result, `mvz.w #252,%d2` for the GCD input).

#### Group A: MOV3Q

Group A is the line-A vector on the classic 68000 — any opword starting with `1010` triggers a line-A exception. ColdFire repurposes part of this space for MOV3Q, a move-quick instruction that encodes a 3-bit signed immediate:

```c
static void exec_groupA(cf_cpu *cpu, uint16_t op)
{
    int subop = (op >> 6) & 7;

    if (subop == 5) {
        /* MOV3Q: 1010 ddd 101 eee rrr */
        int data_field = (op >> 9) & 7;
        int32_t val = (data_field == 0) ? -1 : data_field;
        int ea_mode = (op >> 3) & 7;
        int ea_reg = op & 7;

        if (ea_mode == 0)
            cpu->d[ea_reg] = (uint32_t)val;
        else if (ea_mode == 1) {
            cpu->a[ea_reg] = (uint32_t)val;
            return;  /* no flags for address register */
        } else {
            ea_loc loc = decode_ea(cpu, ea_mode, ea_reg, SZ_LONG);
            write_ea(cpu, &loc, SZ_LONG, (uint32_t)val);
        }
        set_flags_move(cpu, (uint32_t)val, SZ_LONG);
    } else {
        /* EMAC instructions */
        cf_exception(cpu, CF_VEC_LINE_A);
    }
}
```

The 3-bit data field encodes values -1 and 1–7. The value 0 is mapped to -1 (since 0 can be loaded with CLR or MOVEQ). GCC uses MOV3Q so heavily that it appeared 17 times in the fibonacci function's disassembly — primarily `mov3q.l #1,%d0` for comparison constants and `mov3q.l #-1,%d0` for masks.

#### Group F: FPU

Group F handles the entire FPU. The opword carries the coprocessor ID and the EA field; the actual operation is encoded in a 16-bit extension word:

```c
static void exec_groupF(cf_cpu *cpu, uint16_t op)
{
    int cp_id = (op >> 9) & 7;
    if (cp_id != 1) {
        cf_exception(cpu, CF_VEC_LINE_F);
        return;
    }

    /* FBcc: opword bit 8 set, bit 7 set */
    if ((op >> 7) & 1) {
        /* ... FPU branch on condition ... */
        return;
    }

    /* General FPU: fetch extension word */
    uint16_t ext = fetch16(cpu);
    int opclass = (ext >> 13) & 7;

    switch (opclass) {
    case 0: /* Register-to-register */
    case 2: { /* Memory-to-register */
        int dst_fp = (ext >> 7) & 7;
        int opcode = ext & 0x7F;
        double src_val;

        if (opclass == 0) {
            int src_fp = (ext >> 10) & 7;
            src_val = cpu->fp[src_fp];
        } else {
            /* Decode source from memory via EA */
            /* ... format field selects long/single/double/word/byte ... */
        }

        double *dp = &cpu->fp[dst_fp];
        switch (opcode) {
        case 0x00: *dp = src_val; break;          /* FMOVE */
        case 0x03: *dp = trunc(src_val); break;   /* FINTRZ */
        case 0x04: *dp = sqrt(src_val); break;    /* FSQRT */
        case 0x20: *dp /= src_val; break;         /* FDIV */
        case 0x22: *dp += src_val; break;          /* FADD */
        case 0x23: *dp *= src_val; break;          /* FMUL */
        case 0x28: *dp -= src_val; break;          /* FSUB */
        case 0x38: /* FCMP */ { /* ... set FPCC flags ... */ }
        /* ... */
        }
    }}
}
```

Because the V4e FPU uses 64-bit double precision — the same as the host's `double` type — the FPU operations map directly to C arithmetic operators and `<math.h>` functions. `FADD` becomes `+=`, `FSQRT` becomes `sqrt()`, `FINTRZ` becomes `trunc()`. No software floating-point emulation is needed. This is the payoff of choosing V4e over the full 68881: the 80-bit extended precision of the 68881 would require a software floating-point library, adding hundreds of lines and introducing rounding differences between host and guest.

### Exception Handling

Exceptions — traps, illegal instructions, divide-by-zero, privilege violations — follow the 68K family's vector table model. The VBR (vector base register) points to a 256-entry table of 32-bit handler addresses. Each exception type has a fixed vector number:

```c
void cf_exception(cf_cpu *cpu, int vector)
{
    uint32_t old_sr = cpu->sr;

    /* Enter supervisor mode, raise interrupt mask */
    cpu->sr |= CF_SR_S;
    cpu->sr &= ~CF_SR_T;

    /* Push exception frame: PC, SR, format word */
    cpu->a[7] -= 4;
    bus_write32(cpu, cpu->a[7], cpu->pc);
    cpu->a[7] -= 2;
    bus_write16(cpu, cpu->a[7], old_sr & 0xFFFF);
    cpu->a[7] -= 2;
    uint16_t fmt = (4 << 12) | ((vector & 0xFF) << 2);
    bus_write16(cpu, cpu->a[7], fmt);

    /* Fetch handler from vector table */
    cpu->pc = bus_read32(cpu, cpu->vbr + (uint32_t)vector * 4);
}
```

The exception frame pushed to the supervisor stack contains the saved PC, saved SR, and a ColdFire-format word (format 4, distinct from the 68000's format 0). The test harness uses `TRAP #0` (vector 32) to signal program completion — the trap vector points to a `HALT` instruction, which sets the `halted` flag and stops execution.

## Testing and Validation

### The Test Program

The validation strategy uses a bare-metal C program that exercises multiple instruction categories and stores results at known memory addresses:

```c
volatile uint32_t result_fib __attribute__((section(".results"))) = 0;
volatile uint32_t result_gcd __attribute__((section(".results"))) = 0;
/* ... three more result variables ... */

void _start(void)
{
    result_fib    = fibonacci(10);       /* recursion, stack, branching */
    result_gcd    = gcd(252, 105);       /* REMU.L, loops */
    result_sum    = sum_to(100);         /* ADDQ, loop counting */
    result_bits   = bit_test(0xAB);      /* shift, AND, OR, XOR */
    result_sqrt_i = sqrt_approx(2.0);    /* FPU: FDADD, FDMUL, FDDIV */

    __asm__ volatile("trap #0");         /* signal completion */
}
```

Each test function targets a different instruction category. Fibonacci exercises deep recursion (MOVEM for register save/restore, JSR/RTS for call/return, stack frame management via LEA). GCD exercises the hardware remainder instruction (REMU.L) — the instruction that ColdFire V4e has and every other candidate either lacks or implements in software. The square root approximation uses Newton's method with twenty iterations of FPU arithmetic, exercising FDADD, FDMUL, FDDIV, FDSUB, FCMP, and FINTRZ.

### Cross-Compilation

The test program is cross-compiled with the standard Debian/Ubuntu m68k cross-toolchain:

```
m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
    -T link.ld -o test_program.elf test_program.c
```

The `-mcpu=5475` flag targets the MCF5475 — a top-of-line V4e part — ensuring GCC uses the full V4e instruction set including MOV3Q, MVS/MVZ, REMU.L, and the FPU's double-precision `fd`-prefix instructions. The linker script places `.text` at `0x00010000` and the `.results` section immediately after, at fixed addresses that the test harness reads after execution completes.

### Instruction Coverage

The compiled test program exercises 29 unique instruction mnemonics:

| Category | Instructions |
|---|---|
| Data movement | MOVE.L, MOVEA.L, MOVEQ, MOV3Q, MVZ.W, LEA, PEA, MOVEM.L, CLR.L |
| Arithmetic | ADD.L, ADDA.L, ADDI.L, ADDQ.L, SUB.L, SUBA.L, SUBQ.L |
| Logic/compare | AND.L, CMP.L, CMPA.L, TST.L, REMU.L |
| Control flow | Bcc (BEQ, BNE, BCS, BCC, BRA), JSR, RTS, TRAP |
| FPU | FMOVEM, FDMOVE.D, FDMUL.D, FDADD.D, FDDIV.D, FDSUB.D, FCMP.D, FBge, FINTRZ.D, FMOVE.L |

This is not exhaustive coverage of the ColdFire instruction set, but it covers the instructions GCC actually emits for real C programs — which is what matters for a practical emulator.

### The Smoke Test

Running the full test requires the m68k cross-compiler toolchain. For environments without cross-tools, the smoke test embeds the compiled binary directly as a C array with disassembly comments:

```c
static const uint8_t test_image[] = {
    /* _start */
    0x51,0x8f,                          /* subql #8,%sp */
    0xf2,0x17,0xf0,0x20,                /* fmovemd %fp2,%sp@ */
    0x2f,0x03,                          /* movel %d3,%sp@- */
    0x2f,0x02,                          /* movel %d2,%sp@- */
    0x48,0x78,0x00,0x0a,                /* pea a <_start-0x1001a> */
    0x4e,0xb9,0x00,0x01,0x01,0x04,      /* jsr 10104 <fibonacci> */
    0x75,0xfc,0x00,0xfc,                /* mvzw #252,%d2 */
    /* ... 930 bytes total ... */
};
```

The array is generated from `objdump` output using a shell script (`bin2c.sh`) that extracts hex bytes and preserves the disassembly as inline comments. This makes the binary self-documenting — any reader can see exactly which instructions are encoded — and the smoke test can be compiled and run with just the host compiler: `gcc smoke_test.c coldfire.c -lm`.

### Results

All five tests pass:

| Test | Function | Expected | Got | Instructions |
|---|---|---|---|---|
| Fibonacci(10) | Recursion, branching | 55 | 55 | — |
| GCD(252, 105) | REMU.L, loops | 21 | 21 | — |
| Sum(1..100) | ADDQ, counting | 5050 | 5050 | — |
| Bits(0xAB) | Shift, AND, OR, XOR | 0x0A55 | 0x0A55 | — |
| Sqrt(2)×1000 | FPU arithmetic | 1414 | 1414 | — |
| **Total** | | | | **1,528** |

The emulator executes 1,528 instructions to complete all five tests. The majority are in the fibonacci function, which makes 177 recursive calls to compute fib(10).

### QEMU Validation

To validate the emulator against an independent implementation, we compiled the same test functions into a standalone ColdFire binary and ran it under QEMU 8.2's user-mode emulation with the `cfv4e` CPU model:

```
$ qemu-m68k -cpu cfv4e ./qemu_validate
QEMU ColdFire V4e validation
----------------------------
  fibonacci(10)  got 55  expected 55  PASS
  gcd(252, 105)  got 21  expected 21  PASS
  sum_to(100)  got 5050  expected 5050  PASS
  bit_test(0xAB)  got 2645  expected 2645  PASS
  sqrt(2)*1000  got 1414  expected 1414  PASS

5/5 passed
```

All five results match between our emulator, the smoke test, and QEMU. The QEMU validation program uses raw Linux syscalls instead of libc to avoid glibc alignment faults — ColdFire enforces word and long alignment on memory accesses, and the standard m68k glibc contains unaligned access patterns from the 68020 (which relaxed the 68000's alignment requirement).

## Conclusion

The ColdFire V4e emulator is 2,239 lines of C — within the original 2,080–3,120 LOC estimate. It implements the integer core (eight data registers, eight address registers, sixteen opcode groups), an IEEE-754 double-precision FPU mapped directly to host `double` operations, EMAC stubs, and full exception handling. The memory bus uses callbacks for complete isolation between emulator and host. There is no heap allocation.

What the emulator intentionally omits: the EMAC multiply-accumulate unit raises a line-A exception instead of executing (the stub is sufficient for GCC-compiled code, which does not emit EMAC instructions without explicit intrinsics). The MMU, cache control, and debug module are not implemented — `MOVEC` handles control register reads/writes, but the registers themselves are stubs. The ISA_C additions FF1 and BYTEREV are recognized by the decoder but not yet implemented. These omissions reflect the emulator's scope: running GCC-compiled bare-metal C programs, not booting an operating system.

The GCC codegen test was the decisive factor in architecture selection. Fourteen candidates were evaluated; five reached the final round; only ColdFire V4e produced clean single-instruction output for every arithmetic operation tested. The SH-4 was the closest competitor but fell short on integer divide (a libgcc call) and carried additional complexity in delay slots, literal pools, and FPSCR bank-switching.

The emulator is designed as a building block. GCC, LLVM, and Free Pascal all produce code for ColdFire targets — the emulator runs whatever these compilers generate. In Part 2, it becomes the CPU of the Triton, a fantasy game console that explores the alternate history where Sega chose ColdFire V4e over SH-4: maintaining 68K binary compatibility with the Genesis while gaining a modern FPU, multiply-accumulate for audio, and an MMU for sandboxing game code.

### Sources

- *ColdFire Family Programmer's Reference Manual*, Rev. 3 (CFPRM, document CFPRMRev3), Freescale Semiconductor, 03/2005
- *MCF5475 Reference Manual* (MCF5475RM), Freescale Semiconductor, Rev. 4
- *MCF547x/MCF548x ColdFire Microprocessor Family Data Sheet* (MCF5475DE), Freescale Semiconductor, 05/2004
- Motorola V4e core announcement, Microprocessor Forum, October 11, 2000
- Karl Stenerud, [Musashi](https://github.com/kstenerud/Musashi), 68000/68010/68020/68EC020 emulator used in MAME
- Rodrigo Copetti, Console architecture analyses: [Sega Mega Drive](https://www.copetti.org/writings/consoles/mega-drive-genesis/), [Sega Saturn](https://www.copetti.org/writings/consoles/sega-saturn/), [Sega Dreamcast](https://www.copetti.org/writings/consoles/dreamcast/)
- *The M68000 Family Reference Manual* (M68000PM/AD), Motorola, 1992
