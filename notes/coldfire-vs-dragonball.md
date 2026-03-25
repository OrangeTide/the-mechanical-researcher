# ColdFire vs DragonBall: 68K Family Comparison

## Origins and Lineage

Both ColdFire and DragonBall descend from the Motorola 68000 but took fundamentally different approaches:

- **DragonBall** (1995): Wrapped an unmodified **68EC000 core** (the cost-reduced 68000 without the M6800 peripheral bus) with integrated SoC peripherals — LCD controller, UART, SPI, timers, RTC. The CPU core is literally a static 68EC000. Full 68000 instruction set, no modifications, no reductions. The "EC" designation means no M6800 bus and MOVE from SR is privileged (supervisor-only), matching the 68010+ behavior. This is the only ISA difference from the original 68000.

- **ColdFire** (1994): A clean-sheet **RISC-influenced redesign** that retained the 68K programmer's model (sixteen 32-bit registers, supervisor/user modes, similar exception model) but aggressively pruned the ISA for pipeline efficiency. Not binary compatible with 68000.

Both were developed by Motorola's semiconductor division (later Freescale, now NXP).

## ISA Comparison

### DragonBall ISA

Complete MC68000 instruction set. All addressing modes. All data sizes (byte, word, long). No instructions removed, no instructions added. The 68EC000 core supports the full 1,000+ instruction encodings of the 68000. Binary-compatible with any 68000 code (with the trivial MOVE-from-SR caveat in user mode).

### ColdFire ISA — What Was Removed

ColdFire removed instructions that were infrequently compiler-generated or expensive to implement in hardware:

| Category | Removed Instructions |
|----------|---------------------|
| BCD arithmetic | ABCD, SBCD, NBCD, PACK, UNPK |
| Bit field ops | BFCHG, BFCLR, BFEXTS, BFEXTU, BFFFO, BFINS, BFSET, BFTST |
| Rotates | ROL, ROR, ROXL, ROXR |
| Loop/test | DBcc, CHK, CHK2, CMP2, CAS, CAS2 |
| Misc | EXG, RTR, RTD, CMPM, MOVEP, MOVES, TRAPV, TRAPcc |
| System | CALLM, RTM, BKPT, BGND, LPSTOP, RESET |
| CCR/SR bit ops | ORI/EORI/ANDI to CCR |
| TAS | Removed in V1-V3, restored in V4 |

**Byte/word restrictions**: Most arithmetic and logical instructions (ADD, SUB, AND, OR, EOR, NEG, NOT, CMP, ASL, ASR, LSL, LSR) operate on **long words only**. Only CLR, MOVE, and TST support byte/word/long. V4 restored byte/word CMP.

**Addressing mode restrictions**: Memory-indirect modes removed. Indexed modes limited to 8-bit displacement, no suppressed registers, index treated as long only, scale factors limited to 1/2/4 (not 8).

**Memory operand restrictions**: ADDI, ADDX, ANDI, CMPI, shifts, NEG, NEGX, NOT, EORI, ORI, SUBI, SUBX, Scc cannot operate on memory — register-only.

**Flag changes**: MULU/MULS do not set the overflow bit. ASL/ASR do not set the overflow bit.

**Instruction encoding**: All ColdFire instructions are exactly 2, 4, or 6 bytes (regularized for pipeline).

### ColdFire ISA — What Was Added (ISA B / V4+)

- **MOV3Q** — move 3-bit immediate (-1 or 1..7) to register
- **MVS** — move with sign-extend (byte/word to long)
- **MVZ** — move with zero-extend (byte/word to long)
- **SATS** — saturate on overflow
- **INTOUCH** — instruction cache touch (prefetch hint)
- **CPUSHL** — push/invalidate cache line
- **WDDATA/WDEBUG** — debug module access
- 32-bit branch displacements (BSR, Bcc, BRA) restored in V4
- Hardware divide restored in later versions
- MAC/EMAC instructions (multiply-accumulate, not in base 68K)
- FPU instructions in V4e (IEEE-754 double-precision, but 64-bit intermediate vs 68881's 80-bit extended)

## Pipeline and Microarchitecture

### DragonBall

All DragonBall variants use the **static 68EC000 core** — the same basic two-stage prefetch/execute microarchitecture as the original 68000. No caches. 16-bit data bus. Microcoded execution. The "static" designation means it can halt the clock entirely for power savings (critical for PDAs).

| Variant | Clock | MIPS | Bus |
|---------|-------|------|-----|
| MC68328 | 16.58 MHz | 2.7 | 16-bit data, 24-bit addr |
| MC68EZ328 | 16.58 MHz | 2.7 | 16-bit data, 24-bit addr |
| MC68VZ328 | 33 MHz | 5.4 | 16-bit data, 24-bit addr |
| MC68SZ328 | 66 MHz | 10.8 | 16-bit data, 32-bit addr |

### ColdFire

| Core | Pipeline | Issue | Clock (max) | DMIPS/MHz |
|------|----------|-------|-------------|-----------|
| V1 | 2-stage (simplified) | Single | 50 MHz | ~0.8 |
| V2 | 4-stage fetch + 5-stage execute | Single | 166 MHz | ~0.8 |
| V3 | Enhanced w/ branch prediction | Single | 240 MHz | ~0.88 |
| V4/V4e | Limited superscalar | Dual (limited) | 266 MHz | ~1.54 |
| V5 | 4-stage fetch + 2x 5-stage execute | Dual (full superscalar) | 300 MHz | ~1.83 |

ColdFire V2+ has Harvard architecture (separate instruction and data buses). V4+ adds instruction and data caches (up to 32 KB each in MCF547x).

## FPU, MMU, and MAC

| Feature | DragonBall | ColdFire V1 | ColdFire V2 | ColdFire V3 | ColdFire V4/V4e | ColdFire V5 |
|---------|-----------|-------------|-------------|-------------|-----------------|-------------|
| FPU | None | None | None | None | V4e only (IEEE-754 double) | None (base) |
| MMU | None | None | None | None | V4e only (TLB-based) | None (base) |
| MAC | None | None | Optional | Optional | Enhanced MAC (EMAC) | EMAC |
| Cache | None | None | Small | Small | Up to 32KB I + 32KB D | Yes |

The V4e FPU uses 64-bit intermediate precision, not 80-bit extended like the 68881/68882. This means some numerical results differ from classic 68K FPU code.

The V4e MMU uses software-loaded TLBs (no hardware page-table walker), unlike the 68030/68040 MMUs.

## Key Chip Examples

### DragonBall Family

| Chip | Year | Core | Clock | Key Peripherals | Used In |
|------|------|------|-------|----------------|---------|
| MC68328 | 1995 | 68EC000 | 16 MHz | LCD (1024x512 mono), UART, SPI, RTC | PalmPilot 1000/5000 |
| MC68EZ328 | 1998 | 68EC000 | 16 MHz | LCD (grayscale/8-bit color), improved power | Palm III, Palm V, IIIc, Sony CLIE S300 |
| MC68VZ328 | 1999 | 68EC000 | 33 MHz | LCD (640x480, 16 grayscale), software 65K colors | Palm m500, Symbol SPT 1800, Sony CLIE N710C |
| MC68SZ328 | 2000 | 68EC000 | 66 MHz | LCD (TFT 64K color), USB 1.1, MMC/SD, 100KB eSRAM | Later Sony CLIE models |
| MC9328MX1 | 2001 | **ARM920T** | 200 MHz | MPEG-4 DCT, LCD, USB, BT | Renamed i.MX1 in 2003 |

### ColdFire Family (Selected)

| Chip | Core | Clock | Key Features | Target |
|------|------|-------|-------------|--------|
| MCF5206 | V2 | 40 MHz | Basic MCU | Entry-level embedded |
| MCF5272 | V2 | 66 MHz | Ethernet MAC, USB | Networking |
| MCF523x | V2 | 150 MHz | 10/100 Ethernet, CAN | Industrial |
| MCF5307 | V3 | 90 MHz | DRAM controller, DMA | General embedded |
| MCF5407 | V4 | 220 MHz | Superscalar | Performance embedded |
| MCF547x | V4e | 266 MHz | FPU, MMU, 32KB caches, PCI | Linux-capable, networking |
| MCF548x | V4e | 200 MHz | FPU, MMU, Crypto, CAN | Secure networking |
| MCF5441x | V4e | 250 MHz | FPU, dual Ethernet, CAN | Industrial automation |
| MCF51xx (Flexis) | V1 | 50 MHz | Pin-compatible with S08 | 8-to-32-bit migration |

## Binary Compatibility

**DragonBall vs 68000**: Fully binary compatible. The 68EC000 core runs unmodified 68000 object code. The only caveat is MOVE from SR, which traps in user mode (matching 68010+ behavior). Palm OS and all DragonBall software ran native 68K binaries.

**ColdFire vs 68000**: **Not binary compatible**. ColdFire requires recompilation from source. Even at the assembly level, many instructions and addressing modes are missing. Tools like MicroAPL's PortASM/68K can automate assembly translation, but it is not drop-in.

**ColdFire vs DragonBall**: Not binary compatible, since ColdFire is not binary compatible with 68000 and DragonBall runs full 68000 code.

**ColdFire V1 note**: V1 was reportedly closest to 68K compatibility and could be made to behave somewhat like a 68K-compatible CPU, but V2+ diverged further. V3 re-added some addressing modes and can trap on unsupported opcodes for software emulation.

## DragonBall Variant Evolution

1. **MC68328** (1995) — Original. 16 MHz static 68EC000. First 68K SoC with integrated LCD controller. 3.3V, 144-pin TQFP. Powered the original PalmPilot.

2. **MC68EZ328** (1998) — "EZ" = cost-reduced. Same 16 MHz core. Improved power management, smaller package options (100-pin TQFP). Added 8-bit color LCD support. Dominated mid-era Palm devices.

3. **MC68VZ328** (1999) — "VZ" = doubled clock to 33 MHz. Enhanced LCD controller (640x480, 16 grayscale palettes, software-rendered 16-bit color). Still 16-bit data bus, 24-bit address.

4. **MC68SZ328** (2000) — "Super VZ" = doubled again to 66 MHz. Major upgrade: 32-bit address bus, 100 KB embedded SRAM, USB 1.1 device controller, MMC/SD host, TFT LCD support up to 64K colors. Pinnacle of the 68K DragonBall line.

5. **MC9328MX1** (2001) — "MX" = **ARM920T core** at 200 MHz. Complete architecture break from 68K. Kept the DragonBall brand name. Later renamed i.MX1, beginning the i.MX line that is still active today.

Throughout the 68K variants (328 through SZ328), the CPU core remained an unmodified 68EC000 — only the clock speed and peripheral integration changed. No ISA changes across the entire 68K DragonBall line.

## Fate of Both Lines

### DragonBall
- 68K-based variants (EZ/VZ/Super VZ) discontinued ~2005 by Freescale
- MX series transitioned to ARM, renamed i.MX (2003)
- i.MX line lives on today under NXP as a major ARM application processor family (i.MX 6, 7, 8, 9 series)
- The DragonBall brand name was dropped after the i.MX rename

### ColdFire
- Never merged with DragonBall — completely separate product lines
- Peaked with V4e (MCF547x/548x) which could run full Linux with MMU
- NXP stopped recommending ColdFire for new designs by early 2020s
- Most parts reached end-of-life (MCF5272 EOL October 2021)
- NXP recommends i.MX RT (Cortex-M) as the migration path
- A radiation-hardened variant (RH-CF5208) was used in NASA's MMS mission
- ColdFire core was released as open-source IP for FPGAs at one point

### Convergence
Both lines were effectively replaced by ARM:
- DragonBall -> i.MX (ARM application processors)
- ColdFire -> i.MX RT (ARM Cortex-M crossover processors)

Neither line merged with the other. They served different markets (PDA vs industrial) and had incompatible ISAs despite shared 68K heritage.

## Sources

- [MicroAPL: Differences between ColdFire & 68K](https://microapl.com/Porting/ColdFire/cf_68k_diffs.html)
- [NXP ColdFire — Grokipedia](https://grokipedia.com/page/NXP_ColdFire)
- [Freescale DragonBall — Grokipedia](https://grokipedia.com/page/Freescale_DragonBall)
- [NXP ColdFire Family Programmer's Reference Manual](https://www.nxp.com/docs/en/reference-manual/CFPRM.pdf)
- [MC68328 User's Manual](https://www.nxp.com/docs/en/reference-manual/MC68328UM.pdf)
- [MCF547x Product Brief](https://www.nxp.com/docs/en/product-brief/MCF5475PB.pdf)
- [Motorola 68000 series — Wikipedia](https://en.wikipedia.org/wiki/Motorola_68000_series)
