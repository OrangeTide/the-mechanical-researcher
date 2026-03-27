# Triton GPU: Glide 3.0 Software Rasterizer

Software rasterizer implementing a Glide 3.0 API subset for the Triton fantasy
game console. Builds on the ColdFire V4e CPU emulator from
[Part 1](../../coldfire-emulator/) and the system emulator from
[Part 2](../../triton-system-emulator/), adding a complete pixel pipeline
exposed to guest programs via LINE_A hypercalls.

## Quick Start

The default build requires only a C compiler:

```sh
make
./triton-headless
```

This boots the embedded hello program (from Part 2). To run the cube demo:

```sh
# Requires m68k cross-compiler (see "Cross-Compiling" below)
cd examples && make && cd ..
./triton-headless examples/cube.elf
```

Expected output:

```
triton: monitor ROM installed (2327 bytes at 0x01200000)
triton: loaded 11532 bytes from examples/cube.elf at 0x00001000
triton: CPU reset, PC=0x01200400 SP=0x00800000

Triton Monitor v0.1
Vertex Technologies, 2001

ELF: entry=0x00010206 phnum=2
  LOAD: vaddr=0x00010000 filesz=0x000009CE memsz=0x000029D0
Jumping to 0x00010206

cube: starting Glide demo
[glide] grGlideInit
[glide] grSstWinOpen: 640x480 RGB565
cube: 300 frames rendered
[glide] grSstWinClose
[glide] grGlideShutdown

triton: halted after 3372349 instructions
```

## SDL3 Display (Optional)

The included `fetch-sdl3.sh` script downloads and builds SDL3 as a static
library. No system-wide installation needed:

```sh
sh fetch-sdl3.sh
make triton
./triton examples/cube.elf
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

If SDL3 is already installed system-wide (`libsdl3-dev`), `make triton` will
find it via pkg-config without running `fetch-sdl3.sh`.

The display shows the front buffer from VRAM as a 640x480 RGB565 framebuffer.
Buffer swaps in the Glide API update which VRAM region the display reads from.

## Cross-Compiling Guest Programs

Example programs (including the cube demo) require the M68K cross-compiler:

```sh
# Debian/Ubuntu
apt install gcc-m68k-linux-gnu

# Build all examples
cd examples
make
```

### Writing a Glide Program

A minimal Glide program for the Triton:

```c
#include "common.h"
#include "glide3x.h"

void _start(void)
{
    grGlideInit();
    grSstSelect(0);
    grSstWinOpen();

    grVertexLayout(GR_PARAM_XY,  0, GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_RGB, 16, GR_PARAM_ENABLE);
    grCoordinateSpace(GR_WINDOW_COORDS);

    grColorCombine(GR_COMBINE_FUNCTION_LOCAL,
                   GR_COMBINE_FACTOR_ZERO,
                   GR_COMBINE_LOCAL_ITERATED,
                   GR_COMBINE_OTHER_ITERATED, 0);

    grBufferClear(0xFF000000, 0, 0xFFFF);

    GrVertex v0 = { .x = 320, .y = 100, .r = 255, .g = 0, .b = 0, .a = 255 };
    GrVertex v1 = { .x = 200, .y = 380, .r = 0, .g = 255, .b = 0, .a = 255 };
    GrVertex v2 = { .x = 440, .y = 380, .r = 0, .g = 0, .b = 255, .a = 255 };
    grDrawTriangle(&v0, &v1, &v2);

    grBufferSwap(1);

    grSstWinClose();
    grGlideShutdown();

    __asm__ volatile("halt");
    for (;;) ;
}
```

Compile and run:

```sh
cd examples
m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
    -T app_link.ld -o mytriangle.elf mytriangle.c common.o
cd ..
./triton-headless examples/mytriangle.elf
```

## Memory Map

| Address Range | Size | Description |
|---|---|---|
| 0x00000000 - 0x007FFFFF | 8 MB | Main RAM |
| 0x00800000 - 0x00FFFFFF | 8 MB | VRAM (layout below) |
| 0x01000000 - 0x0100FFFF | 64 KB | GPU registers |
| 0x01150000 - 0x011500FF | 256 B | UART (TX bridged to host stdout) |
| 0x01200000 - 0x015FFFFF | 4 MB | NOR flash (monitor ROM, read-only) |

### VRAM Layout

| Offset | Size | Purpose |
|---|---|---|
| 0x000000 | 614,400 | Front buffer (640x480 RGB565) |
| 0x096000 | 614,400 | Back buffer (640x480 RGB565) |
| 0x12C000 | 614,400 | Z-buffer (640x480 uint16) |
| 0x1C2000 | ~6.1 MB | Texture memory |

## Glide API Coverage

### Tier 1 — Implemented (45+ functions)

| Category | Functions |
|---|---|
| Lifecycle | `grGlideInit`, `grGlideShutdown`, `grSstSelect`, `grSstWinOpen`, `grSstWinClose` |
| Buffers | `grBufferClear`, `grBufferSwap`, `grRenderBuffer`, `grClipWindow`, `grSstOrigin` |
| Drawing | `grDrawTriangle`, `grDrawLine`, `grDrawPoint`, `grDrawVertexArray`, `grDrawVertexArrayContiguous` |
| Vertex | `grVertexLayout`, `grCoordinateSpace` |
| Color | `grColorCombine`, `grConstantColorValue`, `grColorMask`, `grDitherMode` |
| Alpha | `grAlphaCombine`, `grAlphaBlendFunction`, `grAlphaTestFunction`, `grAlphaTestReferenceValue` |
| Depth | `grDepthBufferMode`, `grDepthBufferFunction`, `grDepthMask`, `grDepthBiasLevel` |
| Fog | `grFogMode`, `grFogColorValue`, `grFogTable` |
| Texture | `grTexSource`, `grTexCombine`, `grTexClampMode`, `grTexFilterMode`, `grTexMipMapMode`, `grTexDownloadMipMap`, `grTexDownloadMipMapLevel`, `grTexCalcMemRequired`, `grTexMinAddress`, `grTexMaxAddress` |
| Culling | `grCullMode` |
| Chroma-key | `grChromakeyMode`, `grChromakeyValue` |
| LFB | `grLfbLock`, `grLfbUnlock`, `grLfbWriteRegion` |
| Query | `grGet`, `grGetString` |
| Sync | `grFinish`, `grFlush` |

### Tier 2 — Stubbed

Functions in the 0x100–0x1FF hypercall range return success silently.

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
| `make clean` | Remove build artifacts |
| `make distclean` | Remove build artifacts + local SDL3 |

## Files

| File | Lines | Description |
|---|---|---|
| `glide_raster.c` | 1,541 | Software rasterizer: pixel pipeline, all Glide handlers |
| `glide_raster.h` | 378 | Rasterizer state, VRAM layout, Glide enums, hypercall IDs |
| `triton.c` | 491 | System emulator (bus, UART, GPU stub, SDL3, hypercall glue) |
| `triton.h` | 108 | Memory map constants and system state struct |
| `coldfire.c` | — | ColdFire V4e CPU emulator (from Part 1) |
| `coldfire.h` | — | CPU emulator API |
| `examples/glide3x.h` | 598 | Guest-side Glide header: types, enums, LINE_A wrappers |
| `examples/cube.c` | 381 | Spinning textured cube demo |
| `examples/white.c` | — | Minimal test: fill screen white |
| `examples/common.c` | — | UART and framebuffer helpers |
