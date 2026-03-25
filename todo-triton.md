# Triton Game System by Vertex

## Part 1: ColdFire Emulator Article

- [x] Hypercalls: LINE_A intercept mechanism for guest→host calls (cf_set_hypercall API, 12-bit function ID, zero exception overhead)
- [x] coldfire-emulator/index.md — the article
- [x] Compare to Musashi — brief paragraph in article
- [x] demo/README.md — setup guide (prerequisites, build, run)
- [x] QEMU validation — `qemu-m68k -cpu cfv4e ./qemu_validate`, 5/5 match
- [x] Valgrind clean — 0 errors, 0 leaks
- [x] Clean up coldfire.c warnings (unused variables)
- [ ] Update top-level README.md with link to new topic
- [ ] Add to published.txt

## Part 2: The Triton — System Emulator + Monitor ROM

### Monitor ROM (NOR flash at 0x00E00000)

Bare-bones boot monitor, cross-compiled for ColdFire:
- Vector table (reset SP, reset PC, exception handlers)
- Minimal hardware init (GPU, audio silence)
- ELF loader: parse ELF header at 0x00001000, load segments to p_vaddr, JMP to e_entry
- Default exception handlers (register dump via UART, HALT)

Host puts raw ELF file bytes into RAM at 0x00001000 before reset (simulates CD-ROM/MMC read).
Monitor does real ELF parsing on the guest CPU. Reference: existing elf_loader.c.

### System Emulator

- Memory bus: wire CPU callbacks to memory map (RAM, VRAM, GPU, audio, peripherals, NOR flash)
- Peripheral stubs: GPU registers, audio registers, input, timer, UART
- NOR flash: read-only region with monitor ROM image
- SDL or similar for display/input/audio output
- Boot sequence: host loads ELF → RAM, CPU reset → monitor → ELF load → JMP

### Article

- How the CPU emulator from Part 1 plugs into the Triton's memory bus
- Boot sequence and monitor ROM design
- Vertex Technologies and the alternate history

## Part 3: Triton GPU — Software Rasterizer

- Glide API subset (~45 functions)
- Pixel pipeline: triangle setup, rasterization, texturing, depth, blending
- Texture memory management, mipmap support
- SDL display output
- question: Is 320x240 appropriate for 2001? why is this level of support for Z-buffer ; HDTVs were just becoming affordable on the market, although most people still use 640x480 DVDs at that time. GeForce 256 was out (and far more capable than Voodoo 1) and showing good 2D performance at 1600x1200 and excellent 3D performance at 800x600 and higher.
- what API for the emulator, now that we will need graphics. Targets platforms for the emulator should be Linux, Windows, and web(WASM/javascript), with a stretch goal of macOS. We could handle glide with OpenGL/WebGL, Vulkan, Metal, etc. which means we would want a graphics library that handles at least OpenGL/WebGL. Or we could implement a software rasterizer for Glide. which would have less call overhead for WASM but take a little more processing power, but if the resolution is relatively low (under 640x480 and 16bpp) then it is perhaps feasible. some possible game/graphics libraries:
  - for accelerated (OpenGL/WebGL, etc): SDL3, raylib, GLFW, simple custom wrapper for each OS (see /home/jon/DEVEL/screen-2/src/initgl/ ), and other suggestions?
  - for software. we could really use any readily available library or make a custom wrapper for each OS. the WASM would only need enough to deliver framebuffer pixels to the canvas on a present/swapbuffers.

## Part 4: Triton Audio Engine

- 16-channel hardware PCM mixer
- Wavetable synth mode with ADSR envelopes
- ADPCM decompression
- SDL audio output

## Part 5+: Storage, Input, System Firmware, Demo Game

### Real Firmware (replaces monitor ROM)

- SCSI driver, CD-ROM ISO 9660 filesystem
- MMC/SPI driver, FAT16 filesystem
- System shell (game browser, memory card manager, CD player, settings)
- Glide runtime library (callable via TRAP interface)
- ELF loading code carries forward from monitor

### Other

- NCR 5380 SCSI emulation (8 registers, PIO + DMA)
- MMC/SPI memory card
- Input devices (gamepad, mouse, keyboard)
- .BIN/.CUE and .ISO CD-ROM formats
- A small demo game to tie it all together

## Propaganda

- Press release for the founding of Vertex (1999)
- Industry publication article about the unreleased Triton console
- Include Vertex and Triton in a 2001 E3 report
- Period-correct HTML styling; include text body for quoting

## Design

- Look at Magic Cap (https://en.wikipedia.org/wiki/Magic_Cap) for UI ideas
- Console specification document for third-party implementers
- Validation test suite for CPU and graphics (smoke test is a start)
- FreePascal cross-compilation for 68K targets
  (https://downloads.freepascal.org/fpc/snapshot/trunk/m68k-linux/)

## OS API Requirements (Growing List)

See `notes/fantasy-console-spec.md` → "System Firmware — OS API Requirements" for full details.

Key design decisions:
- TRAP #1 syscall interface (function in D0, args in D1-D4/A0-A1)
- NOR flash split: 1 MB firmware + 3 MB FAT16 asset partition
- Asset partition is patchable by emulator (controller graphics substitution)
- 3D low-poly system menu environment (not flat 2D — tech demo for GPU)
- Controller abstraction: games query OS for button glyphs/names, never hardcode

### API categories to flesh out
- [ ] Controller: button metadata, glyphs, diagrams, remapping
- [ ] Assets: FAT16 read access to asset partition
- [ ] Save data: memory card and HDD save file management
- [ ] Display: system fonts, text rendering
- [ ] Audio: system sound effects
- [ ] System: region, return-to-menu, console info

## Open Questions

- OS style — custom and minimal, in NOR flash:
  - Custom like Human68k on X68000? (currently leaning this way)
  - Custom but functionally equivalent to OS-9?
  - High-level abstraction like Palm OS?
  - Simple Unix clone like Coherent? (https://en.wikipedia.org/wiki/Coherent_%28operating_system%29)
    Probably overkill — drop multi-user, protection, virtual memory.
- System menu 3D environment: which concept? (virtual room, abstract space, workshop/garage)
- Asset partition format: plain FAT16, or custom read-only filesystem?
- How do games discover available OS API version? (version number in fixed address?)
