# Triton System Emulator

System emulator for the Triton fantasy game console. Wraps the ColdFire V4e
CPU emulator from [Part 1](../../coldfire-emulator/) in a complete system with
memory-mapped peripherals, a monitor ROM, and an optional SDL3 display.

## Quick Start

The default build requires only a C compiler:

```sh
make
./triton-headless
```

Expected output:

```
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
```

## Loading Custom Programs

Pass an ELF file as a command-line argument:

```sh
./triton-headless path/to/program.elf
```

The ELF must be a big-endian 32-bit executable for M68K (`m68k-linux-gnu-gcc
-mcpu=5475`). The monitor ROM loads it and jumps to the entry point.

## SDL3 Display (Optional)

The included `fetch-sdl3.sh` script downloads and builds SDL3 3.4.2 as a
static library. No system-wide installation needed:

```sh
sh fetch-sdl3.sh
make triton
./triton
```

The script requires `cmake` and standard X11/Wayland development libraries:

```sh
# Debian/Ubuntu
apt install cmake libx11-dev libxext-dev libwayland-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libasound2-dev libpulse-dev

# Fedora
dnf install cmake libX11-devel libXext-devel wayland-devel \
    libxkbcommon-devel libdrm-devel mesa-libgbm-devel alsa-lib-devel \
    pulseaudio-libs-devel

# macOS
brew install cmake
```

SDL3 is linked statically — the resulting `triton` binary has no SDL3
runtime dependency. The `sdl3/` directory is gitignored.

If SDL3 is already installed system-wide (via `libsdl3-dev` or similar),
`make triton` will find it via pkg-config without running `fetch-sdl3.sh`.

The display shows VRAM as a raw 640x480 RGB565 framebuffer.

## Cross-Compiling Guest Programs

The monitor ROM and test programs are pre-compiled and embedded as C headers.
To rebuild them from source, install the M68K cross-compiler:

```sh
# Debian/Ubuntu
apt install gcc-m68k-linux-gnu

# Then regenerate headers
make regen
```

### Writing a Triton Program

A minimal bare-metal ColdFire program:

```c
#define UART_TX_DATA   (*(volatile unsigned char *)0x01150000)
#define UART_TX_STATUS (*(volatile unsigned int  *)0x01150004)

static void putc(char c) {
    while (!(UART_TX_STATUS & 1)) ;
    UART_TX_DATA = c;
}

void _start(void) __attribute__((section(".text.entry")));
void _start(void) {
    putc('H'); putc('i'); putc('\r'); putc('\n');
    __asm__ volatile("trap #0");
    for (;;) ;
}
```

Compile and run:

```sh
m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
    -T hello_link.ld -o myprog.elf myprog.c
./triton-headless myprog.elf
```

## Memory Map

| Address Range | Size | Description |
|---|---|---|
| 0x00000000 - 0x007FFFFF | 8 MB | Main RAM |
| 0x00800000 - 0x00FFFFFF | 8 MB | VRAM (framebuffer at offset 0) |
| 0x01000000 - 0x0100FFFF | 64 KB | GPU registers |
| 0x01100000 - 0x011003FF | 1 KB | Audio registers |
| 0x01110000 - 0x011100FF | 256 B | SCSI registers (stub) |
| 0x01120000 - 0x011200FF | 256 B | MMC/SPI registers (stub) |
| 0x01130000 - 0x011300FF | 256 B | Input port registers (stub) |
| 0x01140000 - 0x011400FF | 256 B | Timer registers (stub) |
| 0x01150000 - 0x011500FF | 256 B | UART (TX bridged to host stdout) |
| 0x01160000 - 0x011600FF | 256 B | DMA registers (stub) |
| 0x01200000 - 0x015FFFFF | 4 MB | NOR flash (monitor ROM, read-only) |

## Build Targets

| Target | Description |
|---|---|
| `make` | Build headless emulator (default) |
| `make triton` | Build with SDL3 display |
| `make fetch-sdl3` | Download and build SDL3 locally |
| `make run` | Build and run with SDL3 |
| `make run-headless` | Build and run headless |
| `make regen` | Rebuild ROM headers (needs cross-compiler) |
| `make valgrind` | Run under valgrind |
| `make disasm-monitor` | Disassemble monitor ROM |
| `make disasm-hello` | Disassemble test program |
| `make clean` | Remove build artifacts |
| `make distclean` | Remove build artifacts + local SDL3 |

## Files

| File | Description |
|---|---|
| `triton.c` | System emulator (bus dispatch, UART, GPU stub, SDL3, main) |
| `triton.h` | Memory map constants and system state struct |
| `coldfire.c` | ColdFire V4e CPU emulator (from Part 1) |
| `coldfire.h` | CPU emulator API |
| `monitor.c` | Monitor ROM source (cross-compiled for ColdFire) |
| `monitor_link.ld` | Linker script for monitor ROM |
| `monitor_rom.h` | Pre-compiled monitor ROM as C array |
| `hello.c` | Test program: UART output + VRAM rectangles |
| `hello_link.ld` | Linker script for test programs |
| `hello_program.h` | Pre-compiled test program as C array |
| `fetch-sdl3.sh` | Download and build SDL3 locally |
| `gen_rom.sh` | Binary-to-C-header converter |
| `bin2c.sh` | ELF disassembly to C array (from Part 1) |
