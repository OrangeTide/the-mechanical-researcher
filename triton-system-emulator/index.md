---
title: "The Triton: System Emulator and Monitor ROM"
date: 2026-03-26
abstract: "Wrapping a ColdFire V4e CPU in a complete system emulator — memory-mapped peripherals, a cross-compiled monitor ROM, and the console startup that built a company's dream"
category: systems
---

## Introduction

[Part 1](../coldfire-emulator/) built a ColdFire V4e emulator in 2,221 lines of C. It executes instructions, passes tests, validates against QEMU — and cannot boot. A CPU without a system around it is like an engine on a test stand: it runs, but it goes nowhere.

The gap between "executes instructions" and "boots a program" is the system layer: a memory map that routes addresses to RAM, ROM, and peripherals; a boot ROM that initializes hardware and loads software; a display that shows pixels; a serial port that prints text. Every real computer has this layer. Every emulator needs it.

This article builds that layer. We take the ColdFire V4e CPU from Part 1 and embed it in the Triton — a fantasy game console from an alternate history, built by a company called Vertex Technologies. The system emulator implements the Triton's memory map, a monitor ROM cross-compiled for the ColdFire, UART output bridged to the host terminal, and an optional SDL3 display. The monitor ROM boots from NOR flash, parses an ELF binary staged in RAM, and jumps to the loaded program — all running on the emulated CPU, with no host-side assistance after reset.

## Abstract

We present a system emulator for the Triton fantasy game console. The emulator wraps the 2,221-line ColdFire V4e CPU from Part 1 in a memory-mapped address space covering 8 MB RAM, 8 MB VRAM, 4 MB NOR flash, and peripheral registers for GPU, audio, UART, SCSI, MMC, input, timer, and DMA — most as stubs, with the UART providing host-bridged serial output. A monitor ROM cross-compiled for the ColdFire boots from NOR flash, initializes hardware registers, parses an ELF binary staged in guest RAM, loads its segments, and jumps to the entry point. The system emulator adds approximately 800 lines of host-side C and 250 lines of guest-side ColdFire C. The monitor successfully boots a test program that prints to the UART and draws colored rectangles to the framebuffer, running 2.6 million emulated instructions to completion.

## Vertex Technologies

### The Pitch

In the spring of 1999, three engineers from 3Dfx Interactive lease office space on Fortran Drive in San Jose — down the street from 3Dfx's headquarters, where they had worked until the previous quarter's layoffs. With $8 million in first-round venture capital from a fund that would not survive the dot-com correction, they incorporate Vertex Technologies.

The pitch is disarmingly simple: take the best hardware from five years ago, sell it at today's prices, and bet everything on making it easy to program. No $10,000 development kits. No proprietary SDKs. No NDA-gated documentation. The dev kit *is* the console — plug in a keyboard and a serial cable, cross-compile with GCC, and you are developing. The target market is not Sony or Nintendo's audience. It is the thousands of hobbyist and small-studio developers who would make games for the hardware if only someone made it affordable to try.

By the summer of 2000, Vertex has about fifty engineers and a chip design that pairs two pieces of commodity silicon: a Motorola ColdFire V4e CPU core — the fastest member of the 68000 family, with hardware multiply, divide, remainder, and an IEEE-754 FPU — and a 3Dfx Voodoo Banshee GPU, whose integrated 2D/3D pipeline is exposed through the open Glide 3.0 API.

### The Market

The console market in 2001 has no room for a fourth contender. Everyone in the industry knows this. Vertex knows this too — but they interpret the same data as opportunity.

| Console | Launch | Price | CPU | GPU | Positioning |
|---|---|---|---|---|---|
| PlayStation 2 | Mar 2000 | $299 | MIPS R5900 | GS (custom) | Mass market |
| GameCube | Nov 2001 | $199 | PowerPC Gekko | ATI Flipper | Nintendo fans |
| Xbox | Nov 2001 | $299 | Intel PIII | Nvidia NV2A | PC gamers |
| Dreamcast | 1999 (dying) | $99 | Hitachi SH-4 | PowerVR CLX2 | Fire sale |
| **Triton** | **Nov 2001** | **$199** | **ColdFire V4e** | **Banshee** | **Developers** |

Vertex's argument: the PlayStation 2 is infamously hard to program (vector units, DMA chains, scratchpad memory). The Xbox has a familiar CPU but a $20,000 dev kit and Microsoft's platform fees. The GameCube is Nintendo — third parties have always been second-class citizens. And the Dreamcast is a cautionary tale, not a competitor.

The Triton can't match any of them on raw power. But it is the cheapest to develop for, the easiest to program, and the most open. Its pitch is the same one that made the original PlayStation succeed against the Saturn: *simple hardware that is easy to get triangles out of.*

### The Bet

Vertex makes three deliberate bets about what their console needs and does not need. Each one sounds reasonable in a 2000 boardroom. Each one is wrong in a way that matters.

**No FMV.** Vertex strips the Banshee's video overlay unit from the SoC to save die area. Their reasoning: the Sega CD debacle, Night Trap, Sewer Shark, and a generation of bad interactive movies proved that consumers want real-time 3D, not pre-rendered video. The market has spoken. What they miss: DVD playback — not FMV games — is the killer feature that sells PlayStation 2s to families who want a $299 DVD player that also plays games.

**Shutter glasses.** The Banshee rasterizer already supports stereo buffer rendering. Vertex adds a mini-DIN sync output and bundles LCD shutter glasses with a launch title. They are convinced that Nintendo's Virtual Boy failed because of bad hardware — monochrome, low resolution, neck pain — not because of a bad concept. A proper stereoscopic 3D experience at 640×480 in full color will be transformative. In 2001, consumers do not want to wear glasses any more than they did in 1995.

**The 68K.** Every Sega Genesis developer knows the 68000. Every Amiga developer knows the 68000. ColdFire V4e is the 68000's direct descendant, with a flat 32-bit address space and hardware floating-point. Vertex believes this familiarity will drive adoption. They are not wrong about the architecture's quality — Part 1 of this series demonstrated that ColdFire V4e is uniquely suited for embeddable emulation. But by 2001, the 68K ecosystem is a historical artifact. The developers who remember it are building middleware for PlayStation 2, not looking for a new console to adopt.

### The Timeline

| Date | Milestone |
|---|---|
| Q1 2000 | License ColdFire V4e core from Motorola |
| Q3 2000 | SoC design begins — V4e + Banshee + audio on one die |
| Oct 2000 | 3Dfx files for bankruptcy |
| Dec 2000 | Nvidia acquires 3Dfx IP; Vertex negotiates Banshee (SST-2) license |
| Q1 2001 | Tape-out custom SoC |
| Q2 2001 | Engineering samples, dev kits ship to studios |
| E3 2001 (May) | Public announcement, playable demos on show floor |
| Q3 2001 | Volume production |
| Nov 2001 | Launch at $199, bundled with 4 GB HDD and one controller |

The Nvidia deal is the hinge of the project. Vertex cannot build a console without a GPU, and 3Dfx's bankruptcy leaves the Banshee IP in Nvidia's hands. Jensen Huang signs the license for a flat fee — he is not speculating on the Triton's success. He is taking guaranteed money for IP that Nvidia has no plans to use. If the Triton succeeds, Nvidia collects royalties and has a console platform. If it fails, the NRE payment is already in the bank.

## The Triton Hardware

### Memory Map

The Triton uses a flat 32-bit address space. Every peripheral is memory-mapped — there are no I/O port instructions on ColdFire.

| Address Range | Size | Description |
|---|---|---|
| 0x00000000 – 0x007FFFFF | 8 MB | Main RAM |
| 0x00800000 – 0x00FFFFFF | 8 MB | VRAM (CPU-accessible linear framebuffer) |
| 0x01000000 – 0x0100FFFF | 64 KB | GPU registers (Glide state machine) |
| 0x01100000 – 0x011003FF | 1 KB | Audio registers (16 channels × 32 bytes) |
| 0x01110000 – 0x011100FF | 256 B | NCR 5380 SCSI controller |
| 0x01120000 – 0x011200FF | 256 B | MMC/SPI memory card |
| 0x01130000 – 0x011300FF | 256 B | Input ports (2× controller/keyboard/mouse) |
| 0x01140000 – 0x011400FF | 256 B | Timer / system control |
| 0x01150000 – 0x011500FF | 256 B | UART (debug serial port) |
| 0x01160000 – 0x011600FF | 256 B | DMA controller (2 channels) |
| 0x01200000 – 0x015FFFFF | 4 MB | System NOR flash (execute-in-place) |

The 4 MB NOR flash is where the monitor ROM lives. It is mapped as execute-in-place (XIP) — the ColdFire can run code directly from flash without copying to RAM first. This is how the Macintosh Classic, early Cisco routers, and most embedded systems of the era worked.

### NOR Flash Layout

```
0x01200000 - 0x0127FFFF  Recovery loader / monitor ROM (512 KB, write-protected)
0x01280000 - 0x0137FFFF  OS / BIOS (3 MB, updatable)
0x01380000 - 0x013FFFFF  Asset partition (512 KB, FAT16)
```

The recovery loader occupies the first 512 KB. Its write-protect pin is tied high on the PCB — a bad firmware update cannot brick the console. For Part 2, this region contains the monitor ROM: a minimal boot program that initializes hardware and loads an ELF from RAM.

### Boot Sequence

The ColdFire V4e follows the 68000 reset protocol: the CPU reads two 32-bit values from the vector base register (VBR) offset — the initial supervisor stack pointer from VBR+0 and the initial program counter from VBR+4. On the Triton, the SoC hardwires VBR to the NOR flash base (0x01200000) on reset, so the CPU boots directly into the monitor ROM.

```
1. Power-on / reset
2. ColdFire reads SSP from 0x01200000 → 0x00800000 (top of 8 MB RAM)
3. ColdFire reads PC  from 0x01200004 → _monitor_start (in NOR flash)
4. Monitor initializes GPU, silences audio
5. Monitor checks 0x00001000 for an ELF magic number
6. Monitor parses ELF headers, copies PT_LOAD segments to p_vaddr
7. Monitor jumps to e_entry
8. Guest program runs
```

In the system emulator, the host stages a raw ELF file into guest RAM at address 0x00001000 before releasing the CPU from reset. This simulates what will eventually be a CD-ROM or hard drive boot — the monitor loads the program from whatever storage is available. For Part 2, the host does the staging; in Part 5, a real SCSI driver will do it.

### The UART

Every system emulator should start with a serial port. A UART is the simplest peripheral that provides visible feedback: write a byte to a register, see a character on the screen. Until the GPU renders pixels, the UART is the only way to know that anything is happening.

The Triton UART is a minimal 4-register interface:

| Offset | Name | R/W | Description |
|---|---|---|---|
| 0x00 | TX_DATA | W | Write byte to transmit |
| 0x04 | TX_STATUS | R | Bit 0: ready to accept a byte |
| 0x08 | RX_DATA | R | Read received byte |
| 0x0C | RX_STATUS | R | Bit 0: byte available |

In the emulator, TX_DATA is bridged to the host's `putchar` — every byte the guest writes appears on the terminal. TX_STATUS always returns "ready" because the host can accept characters instantly. The receive side is stubbed for Part 2.

A real UART would have a baud rate generator, parity settings, and FIFOs. The Triton's UART is a debug port first and a communication channel second. It exists so that developers can `printf` from the first day they have a prototype board, before the GPU driver works, before the storage driver works, before anything else works. Vertex ships the dev kit with a serial cable in the box.

## The Monitor ROM

### What a Monitor Does

The first code that runs on a computer's CPU is the boot ROM. On the Macintosh, it was the Toolbox ROM — 256 KB of routines that drew windows and talked to disk drives. On the Amiga, it was Kickstart — loaded from floppy on early models, burned into ROM on later ones. On the Dreamcast, it was a 2 MB flash image that displayed the swirl logo, checked the GD-ROM, and loaded the boot sector.

The Triton's monitor ROM is simpler than any of these. It has one job: get a program running on the hardware. It initializes the minimum set of peripherals, finds an ELF binary in RAM, parses it, and jumps to it. Everything else — GPU rendering, audio mixing, storage access, the system shell — lives in the OS/BIOS region that Part 5 will implement. The monitor is the recovery loader: the code that runs when nothing else works.

### Vector Table

The first 1,024 bytes of the monitor ROM are the ColdFire vector table — 256 entries of 4 bytes each. The first two entries are special: vector 0 is the initial supervisor stack pointer, and vector 1 is the initial program counter. The remaining 254 entries are pointers to exception and interrupt handlers.

```c
__attribute__((section(".text.vectors")))
const unsigned int vector_table[256] = {
    0x00800000,                         /* 0: Initial SSP (top of RAM) */
    (unsigned int)_monitor_start,       /* 1: Initial PC */
    (unsigned int)_default_handler,     /* 2: access error */
    (unsigned int)_default_handler,     /* 3: address error */
    (unsigned int)_default_handler,     /* 4: illegal instruction */
    /* ... */
    (unsigned int)_trap0_handler,       /* 32: TRAP #0 = halt */
    /* 33-255: default handler */
};
```

The linker script places `.text.vectors` at exactly 0x01200000 — the first section in NOR flash. Getting this wrong means the CPU boots to garbage.

### Hardware Initialization

The monitor entry point writes a handful of registers to bring the hardware to a known state:

```c
void _monitor_start(void) {
    GPU_VID_PROC = 0x00000001;  /* enable display */
    GPU_FB_BASE  = 0x00000000;  /* framebuffer at start of VRAM */
    AUDIO_GLOBAL = 0x00000000;  /* silence all channels */

    mon_puts("Triton Monitor v0.1\r\n");
    mon_puts("Vertex Technologies, 2001\r\n");
    /* ... */
}
```

The GPU write sets the video processor configuration register — in Part 3, this will configure the 640×480 RGB565 display mode. The audio write silences all 16 channels. These are one-time initializations: the monitor puts the hardware into a usable state and never touches these registers again.

### Guest-Side ELF Loader

This is the most interesting part of the monitor ROM. In Part 1, the ELF loader ran on the host — it was a C function (`elf_load`) that called `fopen`, `fseek`, and `fread` to parse the ELF file and used a callback to write bytes into the emulated address space. In Part 2, the ELF loader runs on the *guest* CPU. There is no file I/O, no libc, no heap. Just pointer arithmetic into the flat address space.

The advantage of big-endian: ColdFire is natively big-endian, and ELF files for M68K targets are big-endian (ELFDATA2MSB). This means reading a 32-bit ELF header field is just a pointer cast and dereference:

```c
static unsigned int elf_u32(const unsigned char *p) {
    return *(const unsigned int *)p;
}
```

On the host (little-endian x86), the Part 1 ELF loader needed explicit byte-swap helpers. On the guest, endianness is free.

The loader walks the ELF in four steps:

**1. Validate.** Check the magic bytes (0x7F 'E' 'L' 'F'), ELF class (32-bit), encoding (big-endian), and machine type (EM_68K). If any check fails, print an error to the UART and halt.

**2. Extract header fields.** The entry point (`e_entry`), program header offset (`e_phoff`), program header entry size (`e_phentsize`), and program header count (`e_phnum`) — all read from fixed offsets in the ELF header.

**3. Load segments.** Walk the program header table. For each PT_LOAD segment, copy `p_filesz` bytes from the ELF data to the segment's virtual address (`p_vaddr`), then zero-fill any remaining bytes (`p_memsz - p_filesz`) for the BSS section.

**4. Jump.** Cast `e_entry` to a function pointer and call it.

```c
unsigned int entry = mon_load_elf();
((void (*)(void))entry)();
```

The loader is approximately 60 lines of C — the same logic as Part 1's host-side `elf_loader.c`, but running on the emulated ColdFire. Here is the monitor's UART output during a boot:

```
Triton Monitor v0.1
Vertex Technologies, 2001

ELF: entry=0x00010000 phnum=2
  LOAD: vaddr=0x00010000 filesz=0x00000190 memsz=0x00000190
Jumping to 0x00010000
```

### Exception Handlers

The default exception handler prints a register dump to the UART and halts. It reads the exception stack frame that the ColdFire pushes during exception processing:

```
[SP+0]  Format/vector word (16-bit)
[SP+2]  Status register (16-bit)
[SP+4]  Program counter (32-bit)
```

The format word contains the exception vector number, which identifies what went wrong — illegal instruction, address error, zero divide, privilege violation. Combined with the stacked PC, a developer can identify exactly which instruction caused the fault without a debugger.

TRAP #0 gets a dedicated handler that prints "TRAP #0: halting" and executes the ColdFire HALT instruction. Guest programs use `trap #0` as the conventional "exit" syscall — the same convention used in Part 1's test programs.

### Cross-Compilation

The monitor ROM is compiled with the same cross-compiler used for Part 1's test programs:

```
m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
    -T monitor_link.ld -o monitor.elf monitor.c
```

The flags mean: target ColdFire V4e (`-mcpu=5475`), no standard library (`-nostdlib`), bare-metal environment (`-ffreestanding`), use a custom linker script that places code at 0x01200000. The compiled monitor ROM is 2,327 bytes — it fits in the first 3 KB of the 512 KB recovery loader region.

The binary is extracted from the ELF with `objcopy -O binary`, then converted to a C header (`monitor_rom.h`) containing a byte array. The system emulator includes this header and memcpy's it into the NOR flash region at startup. The pre-generated header is committed to the repository, so the system emulator builds without a cross-compiler.

## System Emulator Architecture

### Bus Dispatch

The CPU emulator from Part 1 communicates with the outside world through six callbacks: `read8`, `read16`, `read32`, `write8`, `write16`, `write32`. Each callback receives an address and either returns a value (reads) or receives a value (writes). The system emulator's job is to implement these six functions with address decoding that routes each access to the correct region.

The dispatch is a cascade of address range checks:

```c
static uint32_t bus_read32(void *ctx, uint32_t addr) {
    triton_sys *sys = ctx;

    if (addr + 3 < TRITON_RAM_END)
        return rd32(sys->ram, addr);
    if (addr >= TRITON_VRAM_BASE && addr + 3 < TRITON_VRAM_END)
        return rd32(sys->vram, addr - TRITON_VRAM_BASE);
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END)
        return uart_read(sys, addr - TRITON_UART_BASE);
    if (addr >= TRITON_GPU_BASE && addr + 3 < TRITON_GPU_END)
        return gpu_read(sys, addr - TRITON_GPU_BASE);
    if (addr >= TRITON_FLASH_BASE && addr + 3 < TRITON_FLASH_END)
        return rd32(sys->flash, addr - TRITON_FLASH_BASE);
    return 0;
}
```

The pattern is the same for all six callbacks. RAM, VRAM, and NOR flash are backed by arrays in the system state struct. The UART handler bridges to the host terminal. The GPU stub stores register writes and returns status values. Everything else returns zero.

This is not a performance-optimized design — a production emulator would use a page table or shift-and-mask lookup to avoid the cascade of comparisons. But for a system that runs a monitor ROM and a test program, it is straightforward and correct. The entire address space is covered: accesses to unmapped regions return zero and do not crash.

### NOR Flash

The NOR flash region is read-only in the emulator. Writes to the flash address range are silently discarded — they do not crash, and they do not modify the contents. This matches the behavior of real NOR flash, which requires a specific erase/program command sequence to modify data.

At startup, the emulator copies the pre-compiled monitor ROM into the flash array:

```c
memcpy(sys.flash, monitor_rom_data, monitor_rom_size);
```

The monitor ROM then executes in place — the CPU's instruction fetches at addresses in the 0x012xxxxx range read directly from this array, just as a real ColdFire would execute from XIP flash.

### The VBR Trick

The critical detail that makes the boot sequence work is a single line of code:

```c
sys.cpu.vbr = TRITON_FLASH_BASE;    /* 0x01200000 */
cf_reset(&sys.cpu);
```

The ColdFire's vector base register (VBR) defaults to zero after `cf_init`. If we called `cf_reset` without setting VBR first, the CPU would try to read its initial stack pointer and program counter from addresses 0x00000000 and 0x00000004 — which are in RAM, and which contain zeros. The CPU would jump to address zero with a zero stack pointer and immediately fault.

Setting VBR to the NOR flash base before reset causes `cf_reset` to read the vector table from flash instead. Vector 0 at 0x01200000 contains 0x00800000 (top of RAM = stack pointer), and vector 1 at 0x01200004 contains the address of `_monitor_start`. The CPU begins executing monitor code. This is the Triton SoC's hardware behavior — on a real chip, the VBR reset value would be hardwired by the chip designer.

### SDL3 Display

The VRAM region (8 MB starting at 0x00800000) serves double duty: the GPU reads from it to display pixels, and the CPU can write to it directly for framebuffer effects, HUD rendering, or — in Part 2's test program — drawing colored rectangles.

When built with SDL3 (`-DTRITON_SDL3`), the emulator creates a 640×480 window and uploads the first 614,400 bytes of VRAM (640 × 480 × 2 bytes per pixel, RGB565) to an SDL texture once per frame:

```c
SDL_UpdateTexture(texture, NULL, sys.vram, 640 * 2);
SDL_RenderTexture(renderer, texture, NULL, NULL);
SDL_RenderPresent(renderer);
```

In headless mode (the default build), the emulator runs without a display. UART output goes to the terminal, and the CPU runs until it halts. The same code runs either way — only the main loop differs.

### System State

The system state struct contains all memory and peripheral state:

```c
typedef struct triton_sys {
    cf_cpu cpu;
    uint8_t ram[8 * 1024 * 1024];       /* 8 MB */
    uint8_t vram[8 * 1024 * 1024];      /* 8 MB */
    uint8_t flash[4 * 1024 * 1024];     /* 4 MB */
    uint8_t gpu_regs[64 * 1024];        /* 64 KB */
    uint8_t uart_rx_data;
    uint8_t uart_rx_ready;
    int running;
    int headless;
} triton_sys;
```

The struct is approximately 28 MB — almost entirely the three large arrays. It is allocated as a static global, not on the stack (which would overflow) or the heap (which would add unnecessary allocation). There are no pointers to free, no cleanup required, no memory leaks possible.

## The Test Program

### Hello from Triton

The test program is 90 lines of bare-metal ColdFire C. It does two things: prints "Hello from Triton!" to the UART, and draws colored rectangles to the framebuffer.

```c
void _start(void) {
    puts_uart("Hello from Triton!\r\n");

    fill_rect(0, 0, SCREEN_W, SCREEN_H, BLACK);
    fill_rect( 40,  40, 160, 120, RED);
    fill_rect(240, 180, 160, 120, GREEN);
    fill_rect(440, 320, 160, 120, BLUE);

    /* white border */
    fill_rect(0, 0, SCREEN_W, 2, WHITE);
    fill_rect(0, SCREEN_H - 2, SCREEN_W, 2, WHITE);
    fill_rect(0, 0, 2, SCREEN_H, WHITE);
    fill_rect(SCREEN_W - 2, 0, 2, SCREEN_H, WHITE);

    puts_uart("VRAM test complete\r\n");
    __asm__ volatile("trap #0");
}
```

The `fill_rect` function writes RGB565 pixels directly to VRAM. Each pixel is a 16-bit write to an address in the 0x00800000–0x00FFFFFF range — the same addresses that the GPU reads when rendering the display. Red is 0xF800 (5 bits red, 6 bits green, 5 bits blue), green is 0x07E0, blue is 0x001F.

The program runs 2.6 million emulated instructions — the overwhelming majority in `fill_rect`, which fills the entire 640×480 framebuffer with black (307,200 pixels) and then draws the rectangles and border. Each pixel write goes through the bus dispatch, which routes it to the VRAM array.

### Building and Running

The default build requires only a C compiler:

```
$ make
$ ./triton-headless

triton: monitor ROM installed (2327 bytes at 0x01200000)
triton: embedded hello program staged (9164 bytes at 0x00001000)
triton: CPU reset, PC=0x01200400 SP=0x00800000

Triton Monitor v0.1
Vertex Technologies, 2001

ELF: entry=0x00010000 phnum=2
  LOAD: vaddr=0x00010000 filesz=0x00000190 memsz=0x00000190
Jumping to 0x00010000

Hello from Triton!
VRAM test complete

TRAP #0: halting

triton: halted after 2605868 instructions
```

The monitor ROM and test program are pre-compiled and embedded as C byte arrays — no cross-compiler needed for the default build. Custom programs can be loaded from ELF files on the command line: `./triton-headless program.elf`.

The included `fetch-sdl3.sh` script downloads SDL3 3.4.2 and builds it as a static library — no system-wide installation required. After running the script, `make triton` builds a version that opens a 640×480 window showing the framebuffer contents — three colored rectangles on a black background with a white border. The resulting binary is self-contained: SDL3 is linked statically, so the only runtime dependencies are libc and libm.

## Conclusion

The system emulator adds approximately 800 lines of host-side C (`triton.c` + `triton.h`) and 250 lines of guest-side ColdFire C (`monitor.c`) to the 2,221-line CPU emulator from Part 1. The total system — CPU, bus dispatch, peripherals, monitor ROM — is under 3,300 lines of C.

What Part 2 proves: a CPU emulator becomes a system emulator when you add address decoding and a single working peripheral. The UART alone is enough to get output, debug problems, and develop software. Everything else — the GPU, audio, storage, input — can be stubbed and filled in later. The monitor ROM demonstrates that guest-side firmware works: ELF parsing runs on the emulated CPU, not the host, using the same memory bus that the loaded program will use. The boot chain is real.

What Part 2 stubs: the GPU registers accept writes but do not render. Audio registers are ignored. The SCSI controller, MMC interface, DMA engine, and timer exist as address ranges that return zero. These stubs ensure that monitor ROM code which initializes peripherals does not crash — it writes to the registers, the writes go into an array, and the monitor continues. Each stub is a placeholder for a future article.

Part 3 will fill in the largest stub: the GPU. A software rasterizer implementing a subset of the Glide 3.0 API — the same API that 3Dfx shipped with the Banshee in 1998 — will turn the VRAM framebuffer into a working 3D display. The pixel pipeline — triangle setup, rasterization, texturing, depth testing, alpha blending — is the piece that transforms the Triton from a serial terminal into a game console.

Vertex Technologies would not live to see it. But the hardware works.

### Sources

- *ColdFire Family Programmer's Reference Manual*, Rev. 3 (CFPRM), Freescale Semiconductor, 03/2005
- *MCF5475 Reference Manual* (MCF5475RM), Freescale Semiconductor, Rev. 4
- 3Dfx Interactive, *Voodoo Banshee Technical Reference Manual*, 1998
- 3Dfx Interactive, *Glide 3.0 Reference Manual*, July 1998
- [SDL3 Wiki](https://wiki.libsdl.org/SDL3/FrontPage), Simple DirectMedia Layer documentation
- NOR flash background: AMD Am29F032B datasheet, Intel 28F320 datasheet — representative 4 MB NOR flash ICs from the era
- NCR 5380 (Am5380) datasheet — 8-register SCSI controller used in Macintosh Plus/SE/Classic/II
- [ELF specification](https://refspecs.linuxfoundation.org/elf/elf.pdf), Tool Interface Standard (TIS) Executable and Linking Format
