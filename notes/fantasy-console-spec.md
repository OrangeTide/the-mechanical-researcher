# Triton — Technical Specification

## The Triton

**"Join the Revolution."** — Vertex Triton launch tagline, E3 2001

*Brought to you by Vertex Technologies — the little company that could.*

### Branding and Tone

Vertex Technologies leans hard into the underdog narrative. Their marketing is tongue-in-cheek, self-aware, and a little scrappy. They know they can't outspend Sony or out-engineer Nintendo, so they out-charm them instead.

**Taglines / ad copy:**
- "Join the Revolution." (main tagline — on the box, on the booth, on the t-shirts)
- "They have armies. We have ideas."
- "The console that ships with a compiler."
- "640×480 pixels of pure uprising."
- "No $20,000 dev kit required. You're welcome."
- "Built from the ashes of giants." (a not-so-subtle nod to 3Dfx and the 68K)

**Voice:** Cheeky but not obnoxious. They're the garage band playing next to the stadium acts. The marketing winks at developers — "we know you know we're the scrappy option, and that's exactly why you should bet on us."

**Logo concept:** A stylized trident (Triton = Greek sea god, son of Poseidon). Clean, geometric, looks good embossed on black plastic. The three prongs subtly reference the three incumbents they're challenging.

### Concept

The Triton is a fantasy game console that never existed — built by Vertex Technologies, a scrappy Silicon Valley startup that bet on commodity hardware and off-the-shelf 1990s inventory to undercut Sony, Nintendo, and Sega. Launched for Christmas 2001 at $199, the Triton pairs a Motorola ColdFire V4e CPU with a Voodoo-class 3D GPU, targeting developers who already know the 68K architecture and the Glide API.

### Company: Vertex Technologies

- Founded ~1999, San Jose, CA (down the street from 3Dfx's old office at 4435 Fortran Drive)
- ~50–100 engineers, VC-funded during the dot-com bubble's tail
- Philosophy: "The best technology from 5 years ago, at today's prices, with a modern CPU"
- Licensed the ColdFire V4e core from Motorola (Q1 2000), licensed SST-1 rasterizer IP from Nvidia after 3Dfx acquisition (Q1 2001)
- No proprietary SDK fees — GCC cross-compiler, open Glide API, $299 dev kit
- Target: small studios and bedroom developers priced out of PS2/Xbox dev kits ($10K–$20K)
- The dev kit IS the console — plug in a keyboard and a serial cable, and you're developing. No separate hardware needed. That's the revolution.

### Timeline

| Date | Milestone |
|---|---|
| Q1 2000 | License ColdFire V4e core from Motorola |
| Q3 2000 | SoC design begins (V4e + GPU + audio on one die) |
| Oct 2000 | 3Dfx files for bankruptcy |
| Dec 2000 | Nvidia acquires 3Dfx IP; Vertex negotiates SST-1 license |
| Q1 2001 | Tape-out custom SoC |
| Q2 2001 | Engineering samples, dev kits ship to studios |
| E3 2001 (May) | Public announcement, playable demos on show floor |
| Q3 2001 | Volume production |
| Nov 2001 | Launch — $199, bundles with 4 GB HDD and one controller |

### Market Position

| Console | Launch | Price | CPU | Target |
|---|---|---|---|---|
| PlayStation 2 | Mar 2000 | $299 | MIPS R5900 | Mass market |
| GameCube | Nov 2001 | $199 | PowerPC Gekko | Nintendo fans |
| Xbox | Nov 2001 | $299 | Intel PIII | PC gamers |
| Dreamcast | 1999 (dying) | $99 | SH-4 | Fire sale |
| **Triton** | **Nov 2001** | **$199** | **ColdFire V4e** | **Developers, indie, value** |

The Triton can't match PS2 or Xbox on raw power, but it's the cheapest to develop for, the easiest to program, and the most open. Its pitch is the same one that made the original PlayStation succeed against the Saturn: "simple hardware that's easy to get triangles out of."

## CPU

- ColdFire V4e @ 200 MHz
- 32-bit, flat address space
- FPU (IEEE-754 double precision, 8 registers FP0–FP7)
- EMAC (enhanced multiply-accumulate, four 48-bit accumulators)
- Hardware 32-bit multiply, divide, remainder
- Toolchain: `m68k-linux-gnu-gcc -mcpu=5475`, Free Pascal m68k target
- Estimated emulator: ~2,080–3,120 LOC

## Memory Map

```
See "Updated Memory Map" section under Peripheral Bus for full layout.
```

## GPU (Glide-Inspired)

### Pixel Pipeline

```
vertex → triangle setup → rasterizer → texel lookup →
  color combine → fog → alpha test → depth test →
  alpha blend → write to framebuffer
```

### Specifications

| Component | Specification |
|---|---|
| VRAM | 4 MB dedicated (framebuffer + textures) |
| Resolution | 320×240 (16-bit, 2× FB + Z), 640×480 (16-bit, 2× FB) |
| Color depth | 16-bit (RGB 565) framebuffer, 16-bit Z-buffer |
| Texture memory | ~2 MB (after framebuffer at 320×240) |
| Max texture size | 256×256, with mipmaps |
| Texture filtering | Point sampled or bilinear |
| TMU count | 1 (multi-pass for multi-texture) |
| Fill rate | ~50 Mpixels/sec (Voodoo1-class) |
| Triangle rate | ~500K triangles/sec |

### Texture Formats

- RGB 565 (16-bit, no alpha)
- ARGB 1555 (16-bit, 1-bit alpha)
- ARGB 4444 (16-bit, 4-bit alpha)
- P_8 (8-bit paletted, 256-color palette)
- ALPHA_8 (8-bit alpha)
- INTENSITY_8 (8-bit grayscale)
- ALPHA_INTENSITY_88 (16-bit, alpha + intensity)
- RGB_332 (8-bit, low-quality color)

### API Subset (~45 functions from Glide 3.0)

**Init/Shutdown:**
- `grGlideInit`, `grSstSelect`, `grSstWinOpen`, `grSstWinClose`, `grGlideShutdown`

**Buffer Operations:**
- `grBufferClear`, `grBufferSwap`, `grRenderBuffer`, `grClipWindow`

**Drawing:**
- `grDrawTriangle`, `grDrawLine`, `grDrawPoint`
- `grDrawVertexArray`, `grDrawVertexArrayContiguous`

**Vertex:**
- `grVertexLayout`, `grCoordinateSpace`

**Color:**
- `grColorCombine`, `grConstantColorValue`, `grColorMask`, `grDitherMode`

**Alpha:**
- `grAlphaCombine`, `grAlphaBlendFunction`, `grAlphaTestFunction`, `grAlphaTestReferenceValue`

**Depth:**
- `grDepthBufferMode`, `grDepthBufferFunction`, `grDepthMask`, `grDepthBiasLevel`, `grDepthRange`

**Fog:**
- `grFogMode`, `grFogColorValue`, `grFogTable`

**Texture:**
- `grTexSource`, `grTexCombine`, `grTexClampMode`, `grTexFilterMode`
- `grTexMipMapMode`, `grTexDownloadMipMap`
- `grTexCalcMemRequired`, `grTexMinAddress`, `grTexMaxAddress`

**Culling:**
- `grCullMode`

**Chroma-key:**
- `grChromakeyMode`, `grChromakeyValue`

**Linear Framebuffer (2D access):**
- `grLfbLock`, `grLfbUnlock`, `grLfbWriteRegion`

**Query:**
- `grGet`, `grGetString`

**Utilities:**
- `guFogGenerateExp`, `guFogGenerateLinear`, `guGammaCorrectionRGB`

### What Was Cut from Full Glide 3.0

- Multi-TMU (TMU1, TMU2) — single TMU, use multi-pass
- `*Ext` functions (stencil, surfaces, combine extensions) — Voodoo3/4/5 era
- Multi-board (SLI, scanline interleave) — single GPU
- State save/restore (`grGlideGetState`/`grGlideSetState`)
- LFB readback (`grLfbReadRegion`) — write-only for 2D blitting
- NCC tables, texture detail control, LOD bias
- Fragmented texture memory (`grTexMultibase`)
- Extension discovery (`grGetProcAddress`)

## Audio

### Architecture: Hardware Channel Mixer + Optional Wavetable Synth

16 channels total, configurable as PCM or wavetable synth voices.

### PCM Channels (default: all 16)

Each channel has hardware registers for:

| Register | Size | Description |
|---|---|---|
| `SAMPLE_ADDR` | 32-bit | Pointer to sample data in main RAM |
| `SAMPLE_LEN` | 24-bit | Sample length in frames |
| `LOOP_START` | 24-bit | Loop point (0 = no loop) |
| `LOOP_LEN` | 24-bit | Loop length in frames |
| `FREQUENCY` | 16-bit | Playback rate as fixed-point ratio (8.8), relative to 44.1 kHz base |
| `VOLUME_L` | 8-bit | Left channel volume (0–255) |
| `VOLUME_R` | 8-bit | Right channel volume (0–255) |
| `CONTROL` | 8-bit | Bits: enable, loop, format (PCM16/PCM8/ADPCM), key-on trigger |
| `STATUS` | 8-bit | Bits: playing, loop point reached (sticky, clear on read) |

- **Output**: 16-bit stereo PCM at 44.1 kHz
- **Sample formats**: 16-bit signed PCM, 8-bit signed PCM, 4-bit ADPCM (IMA/DVI)
- **Hardware mixing**: volume-scaled sum of all active channels, clipped to 16-bit
- **Panning**: independent L/R volume gives full pan control (hard left = L:255 R:0)
- **Pitch control**: frequency register allows arbitrary playback rates from ~172 Hz to ~11.3 MHz sample rate. Pitch = (freq_reg / 256) × 44100.
- **Interrupt**: fires when any channel reaches end-of-sample or loop point (configurable per-channel)

### Wavetable Synth Mode (optional, replaces PCM channels)

Channels can be switched from PCM mode to wavetable synth mode in groups of 4. When a group is in wavetable mode, those 4 channels become synth voices with:

| Register | Size | Description |
|---|---|---|
| `WAVE_TABLE` | 32-bit | Pointer to 256-sample waveform in RAM |
| `NOTE` | 8-bit | MIDI-style note number (0–127) |
| `VELOCITY` | 8-bit | Note velocity / volume (0–255) |
| `ATTACK` | 8-bit | Attack time (0 = instant, 255 = ~2 sec) |
| `DECAY` | 8-bit | Decay time |
| `SUSTAIN` | 8-bit | Sustain level (0–255) |
| `RELEASE` | 8-bit | Release time |
| `GATE` | 1-bit | Key on/off (triggers ADSR envelope) |
| `VOLUME_L` | 8-bit | Left volume (post-envelope) |
| `VOLUME_R` | 8-bit | Right volume (post-envelope) |

- **Waveform**: 256-sample cycle stored in RAM, played back at note frequency. Any waveform — sine, saw, square, sampled instrument single-cycle, etc.
- **ADSR envelope**: hardware envelope generator per voice, applied to amplitude
- **Note frequency**: derived from MIDI note number using standard equal temperament. Register provides fractional pitch bend.
- **Configuration**: channels 0–3, 4–7, 8–11, 12–15 can independently be PCM or wavetable. E.g., channels 0–7 = wavetable for music, channels 8–15 = PCM for sound effects.

### Audio DMA

Audio channels read sample data directly from main RAM via DMA. The audio hardware fetches ahead into a small internal FIFO (~64 samples per channel). CPU can update sample pointers between buffers for streaming audio.

### Audio Buffer / Timing

- Output DAC runs continuously at 44.1 kHz
- Audio interrupt configurable: per-sample-end, per-loop, or periodic (every N output samples, typically every 735 samples = once per frame at 60 Hz)

### Real-World Parallels

- PCM mixer is PS1 SPU-class (24ch) but simplified (no hardware reverb/effects)
- Wavetable mode is similar to Gravis Ultrasound (32ch wavetable) or SNES (8ch BRR wavetable)
- Channel count (16) is between SNES (8) and Saturn SCSP (32)

## Input Devices

### Controller Ports

2 controller ports on the front of the console. Each port auto-detects the peripheral type via an ID register.

### Gamepad (Standard Controller)

| Input | Type | Bits |
|---|---|---|
| D-pad | 4-way digital | Up, Down, Left, Right |
| Left analog stick | 2-axis, 8-bit signed | X: -128..127, Y: -128..127 |
| Right analog stick | 2-axis, 8-bit signed | X: -128..127, Y: -128..127 |
| Face buttons | Digital | A, B, C, X, Y, Z (Sega convention) |
| Shoulder buttons | Digital | L, R |
| System buttons | Digital | Start, Select |
| **Total** | | 14 digital bits + 4 analog axes |

Register layout (active-low for digital, as is Sega convention):

```
PORT_n_ID       (8-bit, read)  - 0x01 = gamepad, 0x02 = mouse, 0x03 = keyboard
PORT_n_BUTTONS  (16-bit, read) - active-low: bit 0=A, 1=B, 2=C, 3=X, 4=Y, 5=Z,
                                 6=L, 7=R, 8=Start, 9=Select, 10=Up, 11=Down,
                                 12=Left, 13=Right, 14-15=reserved
PORT_n_ANALOG_LX (8-bit, read) - left stick X (signed)
PORT_n_ANALOG_LY (8-bit, read) - left stick Y (signed)
PORT_n_ANALOG_RX (8-bit, read) - right stick X (signed)
PORT_n_ANALOG_RY (8-bit, read) - right stick Y (signed)
```

### Mouse (Optional Peripheral)

| Input | Type | Description |
|---|---|---|
| Delta X | 8-bit signed | Relative X movement since last read |
| Delta Y | 8-bit signed | Relative Y movement since last read |
| Buttons | 3 bits | Left, Right, Middle (active-low) |

Shares the same port registers — PORT_n_ID reads 0x02, ANALOG_LX/LY provide deltas, BUTTONS low 3 bits are mouse buttons. Read clears the delta accumulators.

### Keyboard (Optional Peripheral)

| Register | Description |
|---|---|
| PORT_n_ID | 0x03 = keyboard |
| PORT_n_KEY_DATA | 8-bit scancode (read clears) |
| PORT_n_KEY_STATUS | Bit 0: key available, Bit 1: key-up event (vs key-down) |

- Uses PS/2 scancodes (Set 2) — the most cost-effective interface for a 2001
  console. PS/2 keyboards and mice were commodity parts at $5–10.
- 8-byte FIFO buffer in hardware, interrupt on key event
- Modifier state (Shift, Ctrl, Alt) tracked as sticky bits in a separate register
- The SoC integrates a PS/2 controller that handles the serial clock/data
  protocol and presents scancodes to the CPU via the PORT registers

## Storage / Media

### SCSI Bus — NCR 53C80 (5380)

The console uses an NCR 5380-compatible SCSI controller for mass storage. The 5380 was used in the Macintosh Plus/SE/Classic/II, Amiga A3000, and many others. By 2001, these were commodity parts — cheap and well-understood.

The 5380 is an 8-bit SCSI-1 controller. It has only 8 registers, and its
DRQ/DACK signals are wired to the SoC's DMA engine for bulk data transfers:

| Register | Offset | Description |
|---|---|---|
| Current SCSI Data | 0x00 | Data bus value (active data during transfer) |
| Initiator Command | 0x01 | Assert SCSI bus signals (SEL, ATN, BSY, ACK, etc.) |
| Mode | 0x02 | Enable target mode, DMA mode, parity, etc. |
| Target Command | 0x03 | Assert I/O, C/D, MSG signals |
| Current SCSI Bus Status | 0x04 | Read SCSI bus signal state |
| Bus and Status | 0x05 | Phase match, DMA request, parity error, IRQ active |
| Input Data | 0x06 | Latched input data (valid during handshake) |
| Reset Parity/Interrupt | 0x07 | Clear parity error and IRQ (read to clear) |

**Internal SCSI bus** — the CPU doesn't see SCSI cables. The bus is internal to the console PCB.

**SCSI IDs:**
- ID 7: Console (initiator, highest priority)
- ID 0: Internal hard drive (4 GB)
- ID 1: Internal CD-ROM drive
- IDs 2–6: Available for expansion (external SCSI port on back panel)

### Internal Hard Drive — 4 GB SCSI

- 4 GB capacity (plausible for 2001 — 4 GB SCSI drives were common by 1999)
- Used for: game installs, save data, OS/firmware, downloadable content
- SCSI-1 commands: INQUIRY, READ(6)/READ(10), WRITE(6)/WRITE(10), TEST UNIT READY, REQUEST SENSE
- For emulation: backed by a flat file (raw disk image or per-partition files)
- Could be formatted with a simple filesystem (FAT16 with long filenames, or a custom console filesystem)

### Internal CD-ROM Drive — 650 MB

- Read-only, 650 MB (standard CD-ROM / ISO 9660)
- 2× speed (~300 KB/s) — era-appropriate for a budget console
- SCSI-1 commands: INQUIRY, READ(10), READ TOC, TEST UNIT READY, REQUEST SENSE
- Supports CD-DA audio tracks (red book audio playback)
- For emulation: ISO image file or BIN/CUE with audio tracks

### MMC Memory Card — 32 MB standard

MultiMediaCard (MMC) is a simple serial flash interface, predecessor to SD. Chosen for:
- Minimal hardware: SPI-compatible serial bus (MOSI, MISO, CLK, CS)
- Tiny controller footprint: just a SPI shift register + CS line
- Standard block commands: CMD0 (reset), CMD1 (init), CMD17 (read block), CMD24 (write block)
- 512-byte blocks, same as SCSI sectors

**Sizes:**
- 32 MB standard card (bundled with console)
- 64 MB, 128 MB, 256 MB available at retail (256 MB was ~$100 in 2001)
- Max supported: 256 MB (28-bit block addressing × 512 bytes)

**MMC registers (memory-mapped):**

| Register | Size | Description |
|---|---|---|
| MMC_DATA | 8-bit | SPI data register (read/write) |
| MMC_STATUS | 8-bit | Bit 0: busy/transfer in progress, Bit 1: card present, Bit 2: write protect |
| MMC_CONTROL | 8-bit | Bit 0: chip select, Bit 1: clock speed (slow=400kHz for init, fast=8MHz) |

**Usage:**
- Save games (primary use) — each game gets a directory
- Memory card manager in system firmware lets user copy/delete saves
- For emulation: backed by a flat file (raw block image)

### Boot Media

No cartridge slot — the console is disc/HDD-based (like PlayStation or Dreamcast). Games are distributed on CD-ROM and can be installed to the hard drive. Saves go to MMC memory card or HDD.

### System NOR Flash — 4 MB (XIP)

4 MB NOR flash mapped at 0x00E00000–0x00FFFFFF. Execute-in-place (XIP) — the ColdFire can execute code directly from NOR without copying to RAM first. This is how the Mac Classic, many routers, and embedded systems of the era worked.

**Contents:**
- Reset vector and exception table (ColdFire vector table at base)
- Boot ROM / BIOS (~32 KB) — hardware init, POST, peripheral detection
- System shell / OS (~1–2 MB) — file manager, memory card manager, CD player, settings
- System font(s) and UI graphics (~256 KB)
- SCSI driver, filesystem driver (FAT16 or custom), CD-ROM ISO 9660 driver
- Glide library (the console's "SDK runtime") — linked into flash for games that call via trap
- Audio driver / mixer engine
- Spare / updateable region (~1 MB) — firmware updates written via SCSI or MMC

NOR flash is read-only during normal operation. Firmware updates would use a special boot mode that writes to flash (sector erase + program), gated by a physical button or MMC-based updater. In emulation, the flash image is a 4 MB file.

**Technology context:** In 2000, 4 MB NOR flash (e.g., AMD Am29F032B, Intel 28F320) was standard for embedded firmware. The Dreamcast had 2 MB flash for its BIOS. Routers, set-top boxes, and network appliances commonly ran Linux or custom OS entirely from 2–8 MB NOR flash via XIP or copy-to-RAM.

### Boot Sequence

1. ColdFire reset — fetches initial SP and PC from NOR flash vector table at 0x00E00000
2. Boot ROM initializes hardware: RAM test, GPU init, audio silence, SCSI bus reset
3. Boot system shell from NOR flash — presents menu for:
   - Boot from CD-ROM (load and execute boot sector)
   - Browse hard drive (run installed games)
   - Memory card manager (copy/delete saves)
   - CD audio player
   - Settings (video mode, audio levels, clock)
4. System shell can install games from CD to hard drive, manage saves across MMC and HDD

## Peripheral Bus and Interrupts

### Updated Memory Map

```
0x00000000 - 0x007FFFFF  Main RAM (8 MB)
0x00800000 - 0x00BFFFFF  VRAM (4 MB, CPU-accessible via LFB)
0x00C00000 - 0x00C0FFFF  GPU registers (Glide state machine)
0x00D00000 - 0x00D003FF  Audio registers (16 channels × 32 bytes + global regs)
0x00D10000 - 0x00D100FF  NCR 5380 SCSI registers (8 bytes, mirrored)
0x00D20000 - 0x00D200FF  MMC/SPI registers
0x00D30000 - 0x00D300FF  Input port registers (2 ports)
0x00D40000 - 0x00D400FF  Timer / system control registers
0x00D50000 - 0x00D500FF  UART (debug/link cable)
0x00D60000 - 0x00D600FF  DMA controller registers (2 channels)
0x00E00000 - 0x011FFFFF  System NOR Flash (4 MB, XIP — execute in place)
```

### Interrupt Assignments

ColdFire V4e supports 7 interrupt priority levels (IPL 1–7, where 7 is NMI).

| IPL | Source | Frequency | Notes |
|---|---|---|---|
| 7 (NMI) | Reset / watchdog | Rare | Non-maskable |
| 6 | VBlank | 60 Hz | GPU signals end of frame, highest game-relevant priority |
| 5 | Audio | ~60 Hz (per-buffer) | Audio buffer needs refill |
| 4 | SCSI | On completion | 5380 asserts IRQ on phase change / transfer complete |
| 3 | Timer | Programmable | 32-bit countdown timer, 1 MHz clock |
| 2 | Input (keyboard) | On keypress | Keyboard FIFO non-empty |
| 1 | UART | On byte rx/tx | Debug serial port |

Gamepad/mouse are polled (no interrupt) — read once per frame in VBlank handler.

### DMA Controller

2-channel DMA engine for bulk transfers. Both channels are general-purpose and
identical. The DMA engine sits on the SoC bus and can access any address in the
memory map — RAM, VRAM, GPU registers, device registers, and NOR flash.

Base address: 0x00D60000. Channel 1 registers are at +0x10.

#### Registers (Per Channel)

| Register | Offset | Size | Description |
|---|---|---|---|
| DMA_SRC | +0x00 | 32-bit | Source address |
| DMA_DST | +0x04 | 32-bit | Destination address |
| DMA_LEN | +0x08 | 32-bit | Transfer length in bytes (24-bit, max 16 MB) |
| DMA_CTRL | +0x0C | 16-bit | Control register (see below) |
| DMA_STAT | +0x0E | 8-bit | Status register (see below) |

#### DMA_CTRL — Control Register

| Bits | Name | Description |
|---|---|---|
| 0 | START | Write 1 to begin transfer. Self-clearing. |
| 1 | IRQ_EN | Interrupt on completion (asserts DMA IRQ at IPL 3) |
| 3-2 | WIDTH | Transfer width: 00=byte, 01=word, 10=long, 11=reserved |
| 4 | SRC_INC | 1=increment source address, 0=fixed (for device registers) |
| 5 | DST_INC | 1=increment dest address, 0=fixed (for device registers) |
| 7-6 | DEV_MODE | Device mode: 00=memory, 01=SCSI, 10=MMC, 11=reserved |
| 8 | DIR | Direction for device modes: 0=device→memory, 1=memory→device |
| 15-9 | — | Reserved (write 0) |

#### DMA_STAT — Status Register

| Bits | Name | Description |
|---|---|---|
| 0 | BUSY | 1=transfer in progress |
| 1 | COMPLETE | 1=transfer finished (write 1 to clear) |
| 2 | ERROR | 1=bus error during transfer (write 1 to clear) |
| 7-3 | — | Reserved |

#### Device Modes

**Memory-to-memory (DEV_MODE=00):** Both SRC_INC and DST_INC are typically set.
The DMA engine copies DMA_LEN bytes from DMA_SRC to DMA_DST at the configured
WIDTH. Useful for RAM→VRAM blits, ROM→RAM loading, and buffer copies.

**SCSI mode (DEV_MODE=01):** Integrates with the NCR 5380's DRQ/DACK
handshaking. When DIR=0 (device→memory), the DMA engine reads from the 5380's
data register on each DRQ assertion and writes to DMA_DST with auto-increment.
DMA_SRC is ignored. When DIR=1 (memory→device), it reads from DMA_SRC and
writes to the 5380's data register. The 5380 must be placed in DMA mode (Mode
Register bit 1) before starting the DMA channel.

**MMC mode (DEV_MODE=10):** Drives the SPI clock and transfers DMA_LEN bytes
to/from the MMC data register. DIR=0 reads from MMC to DMA_DST, DIR=1 writes
from DMA_SRC to MMC.

#### Emulation Shortcut

In an emulator, device-mode DMA transfers can be collapsed into a single
operation. When SCSI DMA starts, the emulator already knows the full sector data
from the disk image — it can memcpy directly into the destination buffer and
immediately assert the completion interrupt, skipping the per-byte 5380 register
handshake. This is the same optimization MAME and other emulators use for
DMA-capable storage controllers. Memory-to-memory transfers likewise become a
single memcpy.

#### Typical SCSI Read Sequence

1. CPU sends READ(10) command to 5380 via PIO (6 command bytes)
2. 5380 enters data-in phase, asserts DRQ
3. CPU sets up DMA channel: DST=RAM buffer, LEN=sector count × 512,
   CTRL=SCSI mode | DST_INC | byte width | IRQ_EN | START
4. DMA engine transfers bytes as 5380 asserts DRQ for each byte
5. On completion, DMA asserts IRQ at IPL 3
6. ISR clears DMA_STAT.COMPLETE, processes data

#### Interrupt

DMA completion shares IPL 3 with the system timer. The ISR reads DMA_STAT to
distinguish the source. Each channel has an independent COMPLETE flag.

#### Real-World Parallels

- **Amiga Blitter**: 2-channel DMA with source/dest address registers and
  length counter — same concept, different purpose (bitplane blitting vs bulk
  transfer)
- **Atari TT DMA**: External DMA controller wired to NCR 5380 DACK/DRQ for
  SCSI transfers — exactly the approach used here, but integrated on-die
- **PlayStation 1 DMA**: 6-channel DMA with device modes for GPU, SPU, CD-ROM,
  MDEC — similar device-mode concept but more channels and specialized

## Article Series Plan

**Part 1: Building a ColdFire Emulator**
- Light comparative analysis of real CPU ISAs (the selection process)
- ColdFire V4e ISA overview — instruction encoding, addressing modes, registers
- Emulator implementation in C — clean, portable, embeddable, no dependencies
- Memory bus interface (callback-based read/write, so any project can map its own peripherals)
- Test harness: cross-compile C programs with `m68k-linux-gnu-gcc -mcpu=5475`, run on emulator
- Validate against QEMU (`qemu-m68k`) for correctness
- Standalone article — useful for sandboxing, retro projects, VMs, or as a fantasy console CPU

**Part 2: The Triton**
- Vertex Technologies and the alternate history (Genesis → ColdFire V4e, 3Dfx bankruptcy, commodity console)
- Full system spec: memory map, GPU (Glide-based), audio, SCSI, MMC, input
- How the CPU emulator from Part 1 plugs into the Triton's memory bus
- Boot sequence and system firmware concept

**Part 3: Triton GPU — Software Rasterizer**
- Implementing the Glide API subset (~45 functions)
- The pixel pipeline: triangle setup, rasterization, texturing, depth, blending
- Texture memory management, mipmap support
- Connecting to SDL or similar for display output

**Part 4: Triton Audio Engine**
- 16-channel hardware PCM mixer
- Wavetable synth mode with ADSR envelopes
- ADPCM decompression
- Audio output via SDL or similar

**Part 5+: Triton Storage, Input, System Firmware, Demo Game**
- NCR 5380 SCSI emulation (8 registers, PIO)
- MMC/SPI memory card
- Input devices (gamepad, mouse, keyboard)
- System shell / boot firmware
- A small demo game to tie it all together

## Reference Materials

- Glide 3.0 Reference Manual (Jul 1998): ~/Documents/Fantasy-Console/glide-docs/glide3ref.pdf
- Glide 3.0 Programming Guide (Jun 1998): ~/Documents/Fantasy-Console/glide-docs/glide3pgm.pdf
- Glide 3.10 source/headers: ~/tmp/glide3x/
- ColdFire V4e announcement (Oct 2000): design-reuse.com
- MCF5475 datasheet: nxp.com/docs/en/data-sheet/MCF5475EC.pdf
- MCF547x reference manual: people.freebsd.org/~wpaul/MCF5475RM.pdf
- NCR 5380 datasheet: widely available (8-register SCSI controller)
- NCR 5380 in Macintosh: Mac Plus, SE, Classic, II all used 5380 for SCSI
- MMC specification: JEDEC/MMCA standard, SPI mode subset
