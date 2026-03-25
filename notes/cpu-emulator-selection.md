# CPU Emulator Selection Notes

## Goals

- Small emulator implementation, easy to embed into a project for sandboxing
- A delight to develop for (users write C or assembly)
- Example use case: fantasy game console — emulating a 16-bit or 32-bit console that never existed, 80s/90s aesthetic
- Programs > 64K, ideally > 1MB address space
- Hardware multiply and divide
- Short implementation preferred over rich instruction set

## Article Direction

Light comparative analysis of real CPU architectures, leading to a decision based on requirements and biases, then a tutorial-style implementation.

## Requirements

| Requirement | Threshold |
|---|---|
| Address space | >64K, ideally >1MB |
| Arithmetic | Hardware multiply + divide |
| Implementation simplicity | ~1,500–3,000 LOC |
| Real architecture | Existing GCC/LLVM cross-compiler for C and assembly |
| Sandboxing | Easy to embed, isolate guest from host |
| Developer experience | Fun to write programs for |

## Candidates Evaluated

### Eliminated Early

| CPU | Why eliminated |
|---|---|
| 6809 | 8-bit, 64K address space only |
| TMS9900 | 64K address space; registers-in-memory is novel but not a plus for simplicity |
| 65816 | 24-bit addressing (good), but no hardware multiply or divide |
| AT&T Hobbit | Very little public documentation, no accessible toolchain |
| IBM ROMP | Obscure, poor documentation, no surviving toolchain |
| Alpha | 64-bit overkill, ~300 instructions, complex PALcode dependency |

### Viable but Complex

| CPU | Address | MUL/DIV | Instruction count | Notes |
|---|---|---|---|---|
| 80186 | 1MB (seg:off) | Yes | ~130+ | Variable-length encoding (1–6 bytes), segment logic is a pain, prefixes, string ops |
| PowerPC | 4GB | Yes | ~200+ | Clean RISC but large instruction set, complex condition register, SPRs |
| ARMv2/ARM7TDMI | 4GB | MUL yes, no DIV until ARMv7 | ~45 (ARMv2) | Conditional execution adds decoder complexity. Barrel shifter on every instruction. No divide |

### Strong Candidates

#### 1. RISC-V RV32IM
- Address space: 4GB (32-bit flat)
- MUL/DIV: Yes (M extension, 8 instructions)
- Base instructions: ~47 (RV32I) + 8 (M) = ~55
- Encoding: Fixed 32-bit, 4 formats (R/I/S/U + variants)
- Pros: Cleanest modern ISA, exceptional documentation, GCC/LLVM/Rust toolchains, QEMU reference. No legacy cruft.
- Cons: Might feel "too easy." No condition codes.
- Estimated LOC: 1,200–1,800

#### 2. MIPS32 (or MIPS I / R2000)
- Address space: 4GB (32-bit flat, with kernel/user segments)
- MUL/DIV: Yes (HI/LO register pair)
- Instructions: ~60 (MIPS I core)
- Encoding: Fixed 32-bit, 3 formats (R/I/J)
- Pros: Classic teaching ISA, Patterson & Hennessy textbook. Branch delay slots are interesting.
- Cons: Branch delay slots annoying to implement. Load delay slots (MIPS I). CP0 needed for exceptions.
- Estimated LOC: 1,500–2,200

#### 3. Motorola 68000
- Address space: 16MB (24-bit bus, 32-bit internal)
- MUL/DIV: Yes (MULS/MULU, DIVS/DIVU)
- Instructions: ~56 instruction types, many addressing modes
- Encoding: Variable-length (2–10 bytes), 16-bit aligned
- Pros: Elegantly orthogonal — 8 data + 8 address registers, rich addressing modes. Deep nostalgia (Mac, Amiga, Atari ST, Genesis).
- Cons: Effective addressing mode decode is the main complexity (~12 modes × 56 instructions). Variable-length encoding.
- Estimated LOC: 2,500–4,000

#### 4. PDP-11
- Address space: 64K base, 128K with I/D separation, 4MB with MMU (22-bit)
- MUL/DIV: Yes (EIS)
- Instructions: ~60 with EIS
- Encoding: Variable (2–6 bytes), 16-bit word-oriented, 8 addressing modes × 8 registers
- Pros: Most elegant minicomputer ISA. Direct ancestor of 68000. Historical significance (Unix born here).
- Cons: 16-bit — programs think in 16-bit addresses. Slightly fails >64K requirement at program level.
- Estimated LOC: 1,500–2,500

#### 5. SuperH SH-2
- Address space: 4GB (32-bit flat)
- MUL/DIV: Yes (MUL.L, DMULS, DMULU, DIV0S/DIV1 step division)
- Instructions: ~60
- Encoding: Fixed 16-bit (extremely compact)
- Pros: 32-bit RISC with 16-bit instruction encoding — best code density of any RISC. Used in Sega Saturn and Dreamcast (SH-4). Clean design.
- Cons: Division is multi-step (DIV1 loop). Delayed branches. Less well-known, fewer reference emulators.
- Estimated LOC: 1,500–2,200

### Other Mentions

- Z8000: 16-bit with segmented mode (8MB), MUL/DIV, ~110 instructions. Poor toolchain, limited docs.
- OpenRISC OR1K: Clean 32-bit RISC, ~200 instructions. Less historically interesting.
- SPARC V8: 32-bit RISC, ~70 instructions, MUL/DIV. Register windows (136 physical registers) add complexity.

## Historical Context

### 68000 Family in Game Consoles

| Console | Release | CPU | Clock |
|---|---|---|---|
| Sega Genesis / Mega Drive | 1988 JP, 1989 US | Motorola 68000 | 7.67 MHz |
| SNK Neo Geo (MVS/AES) | 1990 | Motorola 68HC000 | 12 MHz |
| Amiga CD32 | Sep 1993 EU | Motorola 68EC020 | 14.28 MHz |
| Atari Jaguar | Nov 1993 US | Motorola 68000 | 13.295 MHz |
| Sega Saturn | Nov 1994 JP | Motorola 68EC000 (sound CPU) | 11.3 MHz |

The 68000 powered or co-processed in five major gaming platforms from 1988–1994. Even the Saturn (which used dual SH-2 as main CPUs) kept a 68000 for sound processing — the lineage ran deep.

### ColdFire Timeline

| Date | Event |
|---|---|
| 1994 | ColdFire introduced (V2 core), low-cost embedded 68K successor |
| 1996 | First commercial V2 products (MCF5200 series) ship |
| Late 1998 | V4 core disclosed |
| Apr 2000 | MCF5407 (first V4 product) announced, 220 MHz, ~3× V3 performance |
| **Oct 11, 2000** | **V4e core announced at Microprocessor Forum** — adds FPU, MMU, EMAC |
| Oct 2002 | V5 core announced (333 MHz, 610 MIPS, superscalar) — but never shipped as standard product |
| **May 2004** | **MCF547x/MCF548x families announced by Freescale** (spun off from Motorola Jul 2004) |
| 2004–2007 | MCF5475 datasheets revised through Rev. 4 (Dec 2007) |
| 2015 | NXP acquires Freescale; ColdFire becomes legacy |

### MCF5475 Specifications (Top-of-line V4e)

- ColdFire V4e core, up to 266 MHz, 410 Dhrystone 2.1 MIPS
- 32 KB I-cache + 32 KB D-cache
- MMU, FPU (IEEE-754 double precision), EMAC (4-stage pipeline, four 48-bit accumulators)
- 32-bit DDR SDRAM controller
- Marketed for: factory automation, medical instrumentation, robotics, security, POS terminals
- MCF548x variants added dual Ethernet, CAN, USB, PCI, hardware crypto accelerator
- Linux (uClinux), RTEMS, Green Hills INTEGRITY RTOS support
- Dev boards: Freescale M5475EVB, LogicPD "Fire Engine" SOM (64 MB DDR, 16 MB Flash)

### Competing Game Console CPUs (1998–2003)

| Console | Release | CPU | Clock | Performance |
|---|---|---|---|---|
| Sega Dreamcast | Nov 1998 JP | Hitachi SH-4 | 200 MHz | 360 MIPS, 1.4 GFLOPS |
| PlayStation 2 | Mar 2000 JP | Emotion Engine (MIPS R5900) | 294 MHz | ~6.2 GFLOPS peak |
| Nintendo GameCube | Sep 2001 JP | IBM Gekko (PPC 750CXe) | 486 MHz | 1.9 GFLOPS sustained |
| Xbox | Nov 2001 US | Custom Intel PIII | 733 MHz | Desktop-class |

The V4e core (announced Oct 2000) was contemporaneous with the PS2 launch. A hypothetical V4e game console in 2001–2002 at 225–266 MHz would have been roughly Dreamcast-class in integer throughput (360–410 MIPS range). The real MCF547x silicon didn't ship until 2004, too late for that generation — but for a fantasy console, we posit a world where Sega chose ColdFire V4e over SH-4, maintaining 68K binary compatibility with their Genesis heritage.

### The Road Not Taken

The V4e was "100% synthesizable and highly configurable" — designed as a licensable core for custom SoCs, exactly how console CPUs work. Key properties that would have suited a game console:

- **68K backward compatibility**: Genesis developers' skills transfer directly
- **FPU**: IEEE-754 double precision, no 80-bit complexity
- **EMAC**: Multiply-accumulate perfect for audio mixing, simple DSP
- **MMU**: Process isolation for multitasking OS or sandboxing game code
- **Hardware multiprocessing support**: Dual-core was possible and ahead of its time

ColdFire was never used in a game console. Its markets stayed industrial (factory automation, medical, robotics, networking). The gaming world moved to purpose-built RISC (SH-4, MIPS, PowerPC, x86). But the alternate history is plausible — and makes a compelling fantasy console premise.

### Sources

- Motorola V4e announcement (Oct 2000): design-reuse.com/news/202503252
- Freescale MCF547x/548x announcement (May 2004): embedded.com
- MCF5475 Product Brief: nxp.com/docs/en/product-brief/MCF5475PB.pdf
- MCF5475 Datasheet Rev. 4: nxp.com/docs/en/data-sheet/MCF5475EC.pdf
- MCF547x Reference Manual: people.freebsd.org/~wpaul/MCF5475RM.pdf
- Console architectures: copetti.org/writings/consoles/
- ColdFire V5 announcement: design-reuse.com/news/4166
- MCF5407 (V4): edn.com "Motorola new version of ColdFire"

## ColdFire Analysis

ColdFire is Motorola/Freescale's simplified 68K — designed for cheaper silicon, which maps directly to simpler emulation.

### What ColdFire Removes from 68000

- BCD instructions (ABCD, SBCD, NBCD, PACK, UNPK)
- Rarely-used addressing modes (some indexed variants)
- Byte/word variants of many operations (long-only for many ops)
- MOVEP (peripheral byte-packing)
- MOVE from SR in user mode
- Some shift/rotate variants

### What ColdFire Adds

- MAC unit (multiply-accumulate) — great for audio/DSP
- Hardware REMU/DIVU.L (full 32÷32→32) — 68000 only does 32÷16→16
- MVS/MVZ (sign/zero extend), BITREV, BYTEREV, FF1
- Simplified variable-length encoding

### ColdFire Versions

| Version | Character | FPU | Notes |
|---|---|---|---|
| V1 | Ultra-minimal | No | Too stripped |
| V2 | Integer sweet spot | No | ~45 instruction types, has MUL but no DIV |
| V3 | V2 + extras | No | Minor additions |
| V4e | Full-featured | Yes | FPU is 68881 subset (~15 instructions), 64-bit double precision |

### ColdFire V4e FPU Details

- 8 registers (FP0–FP7), 64-bit double precision (not 80-bit)
- Instructions: FADD, FSUB, FMUL, FDIV, FNEG, FABS, FSQRT, FCMP, FTST, FMOV, FBcc, FScc, FINT, FINTRZ
- No transcendentals — no FSIN, FCOS, FLOG
- Uses `double` natively — no 80-bit headaches

### ColdFire LOC Estimates

| Component | ColdFire V2 | ColdFire V4e |
|---|---|---|
| CPU state | ~50 | ~60 |
| Decoder | 250–400 | 280–430 |
| EA engine | 200–300 | 200–300 |
| Integer handlers | 900–1,400 | 900–1,400 |
| Condition codes | 150–200 | 150–200 |
| FPU | — | 300–450 |
| MAC unit | 100–150 | 100–150 |
| Exceptions/interrupts | 100–180 | 100–180 |
| **Total** | **1,750–2,680** | **2,080–3,120** |

## FPU Comparison: 68881 vs ColdFire V4e vs SH-4

| | 68881 (full) | ColdFire V4e FPU | SH-4 FPU |
|---|---|---|---|
| Added LOC | 1,200–2,000 | 300–450 | 500–700 |
| Precision | 80-bit internal | 64-bit double | 32-bit single (double optional) |
| Registers | 8 (FP0–FP7) | 8 (FP0–FP7) | 16 (FR0–FR15) × 2 banks |
| Transcendentals | Yes (FSIN, FCOS, etc.) | No | No |
| Bank switching | No | No | Yes (FPSCR.FR bit) |
| Added as module | Coprocessor (F-line trap) | Integrated | Integrated |

## Toolchain Verification (GCC 13, Ubuntu 24.04)

### Available Toolchains

Both `gcc-m68k-linux-gnu` and `gcc-sh4-linux-gnu` available in Debian/Ubuntu repos.
Single m68k package targets all variants: `-m68000`, `-mcpu=5206` (CF V2), `-mcpu=5475` (CF V4e).
Free Pascal supports m68k (68000, 68020, ColdFire) but not SuperH.

QEMU user-mode emulators available: `qemu-m68k`, `qemu-sh4`, `qemu-system-m68k`, `qemu-system-sh4`.

### GCC Codegen Test Results

Test program: 32-bit add, multiply, divide, modulo, float add/mul/div.

| Operation | 68000 | ColdFire V2 | ColdFire V4e | SH-4 |
|---|---|---|---|---|
| 32-bit add | `add.l` | `add.l` | `add.l` | `add` |
| 32-bit multiply | **libgcc** | `muls.l` | `muls.l` | `mul.l` + `sts macl` |
| 32-bit divide | **libgcc** | **libgcc** | `divs.l` | **libgcc** (`__sdivsi3_i4i`) |
| 32-bit remainder | **libgcc** | **libgcc** | `rems.l` | mul+sub (after div call) |
| float add | **libgcc** | **libgcc** | `fsadd` | `fadd` |
| float multiply | **libgcc** | **libgcc** | `fsmul` | `fmul` |
| float divide | **libgcc** | **libgcc** | `fsdiv` | `fdiv` |

Key findings:
- **68000**: No 32-bit multiply, no 32-bit divide, no FPU — everything except add is a libgcc call
- **ColdFire V2**: Has 32-bit multiply but no hardware divide, no FPU
- **ColdFire V4e**: Everything is a single hardware instruction — cleanest codegen of all targets
- **SH-4**: Hardware FPU works but integer divide is still a libgcc call (`__sdivsi3_i4i` uses FPU reciprocal internally). Also has FPSCR bank-switching overhead and delay slots.

### SH-4 Emulator Complications Visible in Codegen

1. **Literal pools**: Every address/constant loaded via PC-relative from pool after function body (16-bit instructions can't hold large immediates)
2. **Delay slots**: Instructions after branch execute before branch takes effect — #1 source of emulator bugs
3. **MACL/MACH**: Multiply results go to special accumulator registers, not GPRs
4. **FPSCR toggling**: GCC emits `sts fpscr` / `xor` / `lds fpscr` around FP ops to switch single/double precision mode (bit 0x80000)

## Ranking (Updated — for Fantasy Console Use Case)

| Priority | CPU | Rationale |
|---|---|---|
| **Top pick** | **ColdFire V4e** | All arithmetic in hardware, ~2,080–3,120 LOC, FPU included, 68K lineage ("Genesis II that never was"), GCC + Free Pascal toolchains, clean codegen |
| Runner-up | RISC-V RV32IM+F | Cleanest implementation (~1,200–1,800 + FPU), modern relevance, but less nostalgic |
| Dark horse | SH-4 | Good FPU but software integer divide, delay slots, FPSCR bank-switching, literal pools add complexity. No Free Pascal support |
| Historical | 68000 (classic) | Nostalgia but fails requirements — no 32-bit multiply, no FPU without 68881 coprocessor (adds 1,200+ LOC) |
