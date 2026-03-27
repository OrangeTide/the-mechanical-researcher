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

### Monitor ROM (NOR flash at 0x01200000)

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
- SDL3 for display (framebuffer blit), input, and audio output
- Boot sequence: host loads ELF → RAM, CPU reset → monitor → ELF load → JMP

### Article

- How the CPU emulator from Part 1 plugs into the Triton's memory bus
- Boot sequence and monitor ROM design
- Vertex Technologies and the alternate history

## Part 3: Triton GPU — Software Rasterizer

- [x] Glide API subset (~45 Tier 1 functions, ~20 Tier 2 stubs)
- [x] Pixel pipeline: triangle setup, rasterization, texturing, depth, blending
- [x] Texture memory management, mipmap support (5 formats, non-square aspect)
- [x] Color/alpha combine unit matching Glide combine equation
- [x] Fog (logarithmic table indexing), scissor, cull, alpha test, depth bias
- [x] SDL3 framebuffer blit (software-rendered RGB565 → SDL_Texture)
- [x] grGetString with Vertex Technologies branding
- [x] Host-side test suite (21 tests, 60 assertions, valgrind clean)
- [x] Article: triton-gpu/index.md
- [x] Demo: examples/cube.c — textured spinning cube
- [ ] Update top-level README.md with link to new topic
- [ ] Add to published.txt

## Part 4: Triton Audio Engine

- 16-channel hardware PCM mixer with stereo panning
- ADPCM decompression (IMA/DVI 4-bit)
- SDL3 audio output (callback-based mixer)
- Audio register file at 0x01100000 (16 channels × 32 bytes + global regs)
- Guest demo program exercising the mixer

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
