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

The Triton is a fantasy game console that never existed — built by Vertex Technologies, a scrappy Silicon Valley startup that bet on commodity hardware and off-the-shelf 1990s inventory to undercut Sony, Nintendo, and Sega. Launched for Christmas 2001 at $199, the Triton pairs a Motorola ColdFire V4e CPU with a Voodoo Banshee-derived GPU (integrated 2D + 3D), targeting developers who already know the 68K architecture and the Glide API.

### Company: Vertex Technologies

- Founded ~1999, San Jose, CA (down the street from 3Dfx's old office at 4435 Fortran Drive)
- ~50–100 engineers, VC-funded during the dot-com bubble's tail
- Philosophy: "The best technology from 5 years ago, at today's prices, with a modern CPU"
- Licensed the ColdFire V4e core from Motorola (Q1 2000), licensed Voodoo Banshee (SST-2) IP from Nvidia after 3Dfx acquisition (Q1 2001)
- Nvidia's motivation: flat licensing fee for IP they weren't using (Banshee was EOL), plus a foothold in the console industry after NV1 flopped and NV2/Dreamcast fell through. If Triton succeeds, Nvidia has royalties and a console platform. If it fails, they already have the NRE payment. Jensen doesn't make speculative bets — he takes guaranteed money.
- No proprietary SDK fees — GCC cross-compiler, open Glide API, $299 dev kit
- Target: small studios and bedroom developers priced out of PS2/Xbox dev kits ($10K–$20K)
- The dev kit IS the console — plug in a keyboard and a serial cable, and you're developing. No separate hardware needed. That's the revolution.

### Timeline

| Date | Milestone |
|---|---|
| Q1 2000 | License ColdFire V4e core from Motorola |
| Q3 2000 | SoC design begins (V4e + GPU + audio on one die) |
| Oct 2000 | 3Dfx files for bankruptcy |
| Dec 2000 | Nvidia acquires 3Dfx IP; Vertex negotiates Banshee (SST-2) license |
| Q1 2001 | Tape-out custom SoC |
| Q2 2001 | Engineering samples, dev kits ship to studios |
| Mar 2001 | Partnership announced with Metricom for Ricochet-based online gaming |
| E3 2001 (May) | Public announcement, playable demos on show floor. Triton Internet Pack announced. |
| Aug 2001 | Metricom files for bankruptcy. Ricochet network goes dark. Internet Pack shelved. |
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

### Design Philosophy — "3D First"

Vertex made deliberate bets about what to include and what to leave out:

- **No FMV / video playback hardware.** Vertex saw FMV as a failed 90s fad — the Sega CD debacle, Night Trap, Sewer Shark, and a generation of bad interactive movies proved that consumers wanted real-time 3D, not pre-rendered video. The market had already spoken. Omitting video decode kept the SoC die smaller and the BOM cheaper.

- **Stereoscopic 3D with shutter glasses.** The Banshee rasterizer already supports stereo buffer rendering. Vertex added a mini-DIN sync output and bundled LCD shutter glasses with a launch title. They were convinced that Nintendo's Virtual Boy was the right idea with terrible execution (monochrome, 32 pixels, neck cramps). A proper stereoscopic 3D experience at 640×480 in full color would be transformative. This was their differentiator. In hindsight, another miscalculation.

- **No overlay / scaler / YUV CSC.** The Banshee design includes a video overlay unit, but Vertex stripped it from the SoC to save die area. Without video playback to support, there's no need for overlay planes or color space conversion hardware. If a game needs to display a full-screen image, it can LFB-blit to the framebuffer. Cutscenes use real-time rendering.

These decisions are narratively consistent with a scrappy startup that puts all its chips on one bet. Vertex truly believed 3D was the only thing that mattered, and that VR was the future. They weren't wrong about 3D — they were just wrong about everything else.

- **No networking hardware.** The Triton has no Ethernet port, no modem, no USB host controller. Vertex planned to solve online connectivity through a partnership with Metricom's Ricochet wireless network — a city-wide mesh of shoebox-sized radios on lampposts offering 128 kbps wireless internet. The Ricochet modem connects via RS-232 serial (the Triton already has a UART for its dev kit / link cable). No USB stack needed, no Ethernet MAC, no PHY — just a serial port talking PPP to a wireless modem. Vertex announced the "Triton Internet Pack" at E3 2001: a CD-ROM bundle containing a TCP/IP stack, NetFront web browser (licensed from ACCESS Co., the same embedded browser used in the Sega Dreamcast), an email client that stores mail on the GameCard (MMC), and a coupon for 3 months of free Ricochet service. The Internet Pack never shipped. Metricom went bankrupt in August 2001 — four months before the Triton's launch — after spending $500 million and attracting only 51,000 subscribers. The modems were already in the warehouse. The marketing materials were already printed.

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

## GPU (Banshee-Derived, Integrated 2D + 3D)

The Triton GPU is derived from the 3Dfx Voodoo Banshee (SST-2), licensed from Nvidia. Unlike the original Voodoo1 (3D-only, required a separate VGA card), the Banshee integrates 2D acceleration, VGA output, and 3D rendering in a single unified design with one memory controller and one command FIFO. Vertex stripped the VGA compatibility, DAC, and video overlay unit to save die area, keeping the 2D engine, 3D pipeline, and framebuffer controller.

### 3D Pixel Pipeline

```
vertex → triangle setup → rasterizer → texel lookup →
  color combine → fog → alpha test → depth test →
  alpha blend → write to framebuffer
```

### 2D Engine

The Banshee 2D engine provides hardware-accelerated operations on the framebuffer:

- **BitBLT**: screen-to-screen, host-to-screen, with full ROP support (256 raster operations)
- **Rectangle fill**: solid color and 8×8 pattern fills
- **Line draw**: Bresenham line engine with clipping
- **Color expand**: 1bpp→Nbpp expansion (monochrome font/glyph rendering)
- **Hardware clipping**: scissor rectangle for all 2D operations
- **Hardware cursor**: 64×64 2-color sprite overlay
- **Chroma key**: source color compare for transparency (skip writes where source matches key)

The 2D and 3D engines share VRAM through a single unified memory controller — no arbitration complexity. They submit work through the same command FIFO. Games typically use 2D for menus, HUD overlays, and text rendering, then switch to 3D for gameplay.

### Specifications

| Component | Specification |
|---|---|
| Architecture | Voodoo Banshee (SST-2) derivative |
| VRAM | 8 MB dedicated (framebuffer + textures) |
| Resolution | 640×480 (16-bit, 2× FB + Z-buffer) |
| Color depth | 16-bit (RGB 565) framebuffer, 16-bit Z-buffer |
| Texture memory | ~6.2 MB (after double-buffered framebuffer + Z-buffer) |
| Max texture size | 256×256, with mipmaps |
| Texture filtering | Point sampled or bilinear |
| TMU count | 1 (multi-pass for multi-texture) |
| Fill rate | ~50 Mpixels/sec (Voodoo1-class) |
| Triangle rate | ~500K triangles/sec |
| 2D engine | BitBLT, rect fill, line draw, color expand, HW cursor |

### Texture Formats

- RGB 565 (16-bit, no alpha)
- ARGB 1555 (16-bit, 1-bit alpha)
- ARGB 4444 (16-bit, 4-bit alpha)
- P_8 (8-bit paletted, 256-color palette)
- ALPHA_8 (8-bit alpha)
- INTENSITY_8 (8-bit grayscale)
- ALPHA_INTENSITY_88 (16-bit, alpha + intensity)
- RGB_332 (8-bit, low-quality color)

### Glide 3.0 API

Glide 3.0 shipped alongside the Banshee in June 1998. In the open-source Glide
codebase, Banshee and Voodoo3 share the same `h3` driver — they're the same
architecture, Voodoo3 just re-adds the second TMU. The full Glide 3.x export
table is ~98 functions. The Triton implements a practical subset organized into
three tiers.

#### Tier 1 — Implement (~45 functions)

The core rendering pipeline. Every function here does real work.

**Lifecycle:**
- `grGlideInit`, `grGlideShutdown`
- `grSstSelect`, `grSstWinOpen`, `grSstWinClose`

**Buffers:**
- `grBufferClear`, `grBufferSwap`, `grRenderBuffer`
- `grClipWindow`, `grSstOrigin`

**Drawing:**
- `grDrawTriangle`, `grDrawLine`, `grDrawPoint`
- `grDrawVertexArray`, `grDrawVertexArrayContiguous`

**Vertex format:**
- `grVertexLayout`, `grCoordinateSpace`

**Color combine:**
- `grColorCombine`, `grConstantColorValue`, `grColorMask`, `grDitherMode`

**Alpha:**
- `grAlphaCombine`, `grAlphaBlendFunction`
- `grAlphaTestFunction`, `grAlphaTestReferenceValue`

**Depth:**
- `grDepthBufferMode`, `grDepthBufferFunction`, `grDepthMask`, `grDepthBiasLevel`

**Fog:**
- `grFogMode`, `grFogColorValue`, `grFogTable`

**Texture:**
- `grTexSource`, `grTexCombine`, `grTexClampMode`, `grTexFilterMode`
- `grTexMipMapMode`, `grTexDownloadMipMap`, `grTexDownloadMipMapLevel`
- `grTexCalcMemRequired`, `grTexTextureMemRequired`
- `grTexMinAddress`, `grTexMaxAddress`

**Culling:**
- `grCullMode`

**Chroma-key:**
- `grChromakeyMode`, `grChromakeyValue`

**Linear framebuffer:**
- `grLfbLock`, `grLfbUnlock`, `grLfbWriteRegion`

**Query:**
- `grGet`, `grGetString`

**Enable/disable:**
- `grEnable`, `grDisable`

**Sync:**
- `grFinish`, `grFlush`

#### Tier 2 — Stub (~20 functions)

Safe to return success or no-op. Games either check for NULL, use these as
hints, or call them in non-critical paths.

| Function | Stub behavior |
|---|---|
| `grGetProcAddress` | Return NULL — games check before calling extensions |
| `grSplash` | No-op (3dfx logo splash screen) |
| `grLoadGammaTable` | No-op (display looks fine without gamma correction) |
| `grStippleMode`, `grStipplePattern` | No-op (line stipple, almost never used) |
| `grTexDetailControl` | No-op (LOD detail texturing, rare) |
| `grTexNCCTable` | No-op (Narrow Channel Compression, uncommon format) |
| `grTexMultibase`, `grTexMultibaseAddress` | No-op (TMU bank splitting, very rare) |
| `grDepthRange`, `grViewport` | No-op (only needed with clip-space coordinates) |
| `grGlideGetState`, `grGlideSetState` | No-op (full state save/restore) |
| `grGlideGetVertexLayout`, `grGlideSetVertexLayout` | No-op (vertex layout save/restore) |
| `grSetNumPendingBuffers` | No-op (tuning hint) |
| `grCheckForRoom` | Return true (FIFO space check) |
| `grReset` | No-op (stats reset) |
| `grAADrawTriangle` | Fall through to `grDrawTriangle` (edge AA, rarely used) |
| `grAlphaControlsITRGBLighting` | No-op (special alpha mode, uncommon) |
| `grDisableAllEffects` | Reset state to defaults (trivial) |
| `grErrorSetCallback` | Store pointer (never called back in practice) |

#### Tier 3 — Not implemented

These either require Voodoo3/4/5 hardware, serve no purpose on a console, or
are host-side utilities that games can replace.

| Category | Functions | Reason |
|---|---|---|
| Extensions (V4/V5) | `grStencilFunc/Mask/Op`, `gr*CombineExt`, `grTextureBufferExt` | Voodoo4/5 only — `grGetProcAddress` returns NULL |
| Multi-TMU | TMU1 state in `grTexCombine`, `grTexSource` | Banshee has 1 TMU — `grGet(GR_NUM_TMU)` returns 1, games multi-pass |
| Multi-context | `grSelectContext` | Console has one screen |
| Multi-board | SLI functions | Single GPU |
| File I/O | `gu3dfLoad`, `gu3dfGetInfo` | .3df texture loader — games do their own loading |
| Encoding | `guEncodeRLE16` | Host-side utility |
| LFB config | `grLfbConstantAlpha/Depth`, `grLfbWriteColorSwizzle/Format` | Advanced LFB modes — default RGB565 write is sufficient |
| LFB readback | `grLfbReadRegion` | Write-only framebuffer access |
| Texture partial | `grTexDownloadMipMapLevelPartial`, `grTexDownloadTablePartial` | Partial updates — rare, full upload is fine |

#### Utility functions (gu* — pure math, no hardware)

These are trivial helpers (~10-20 lines each) that games call directly.
Implement them because they're free.

- `guFogGenerateExp`, `guFogGenerateExp2`, `guFogGenerateLinear` — fill a 64-entry fog table
- `guFogTableIndexToW` — convert fog table index to W depth value
- `guGammaCorrectionRGB` — convenience gamma wrapper

### Stereoscopic 3D (Shutter Glasses)

The Banshee rasterizer has native stereo 3D support: alternating left-eye/right-eye framebuffers with a sync signal for LCD shutter glasses. Vertex exposes this as a first-party feature:

- **Stereo sync output**: mini-DIN connector on the back of the console
- **Shutter glasses**: bundled with a launch title (pack-in accessory)
- **API**: `grSstControl(GR_CONTROL_ACTIVATE)` with stereo buffer mode — games render two views per frame, the GPU alternates them at field rate
- **Fallback**: games must work without glasses (mono mode default)

Vertex's pitch: "the first affordable stereoscopic 3D gaming platform." They believed Nintendo's Virtual Boy failed because of bad hardware (monochrome, no head tracking, neck pain), not because of a bad concept. In retrospect, this was another miscalculation — consumers in 2001 didn't want glasses any more than they wanted Virtual Boy.

### What Was Cut from Banshee Hardware

Vertex stripped these from the SoC to save die area and reduce cost:

- **VGA compatibility mode and VGA DAC** — console doesn't need legacy PC display modes
- **Video overlay unit and scaler** — no FMV, no video playback (see Design Philosophy)
- **AGP interface** — replaced with SoC-internal bus
- **PCI configuration space** — not a PC add-in card
- **Second TMU slot** — Banshee already has 1 TMU; Voodoo3 re-added TMU1, but Vertex kept the cheaper single-TMU config

## Audio

### Architecture: 16-Channel Hardware PCM Mixer

16 PCM channels with hardware mixing, stereo panning, and DMA from main RAM.

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

### Audio DMA

Audio channels read sample data directly from main RAM via DMA. The audio hardware fetches ahead into a small internal FIFO (~64 samples per channel). CPU can update sample pointers between buffers for streaming audio.

### Audio Buffer / Timing

- Output DAC runs continuously at 44.1 kHz
- Audio interrupt configurable: per-sample-end, per-loop, or periodic (every N output samples, typically every 735 samples = once per frame at 60 Hz)

### Real-World Parallels

- PCM mixer is PS1 SPU-class (24ch) but simplified (no hardware reverb/effects)
- Channel count (16) is between SNES (8) and Saturn SCSP (32)
- Homebrew developers can implement software synth (wavetable, FM, etc.) by mixing into a PCM buffer

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

4 MB NOR flash mapped at 0x01200000–0x015FFFFF. Execute-in-place (XIP) — the ColdFire can execute code directly from NOR without copying to RAM first. This is how the Mac Classic, many routers, and embedded systems of the era worked.

**Layout:**

```
0x01200000 - 0x0127FFFF  Recovery loader (512 KB) — write-protected
0x01280000 - 0x0157FFFF  OS / BIOS (3 MB) — updatable
0x01580000 - 0x015FFFFF  Asset partition (512 KB) — FAT16
```

**Recovery loader** (512 KB, hardware write-protected):
- Reset vector and exception table (ColdFire vector table at base)
- Hardware init, POST, peripheral detection
- Can boot a CD-ROM and reflash the OS/BIOS region
- Also serves as development monitor (ELF loader for Part 2)
- Never overwritten — a bad firmware update cannot brick the console

**OS / BIOS** (3 MB, updatable via recovery loader):
- System shell / 3D menu environment
- SCSI driver, CD-ROM ISO 9660 driver, HDD driver
- MMC/SPI driver, FAT16 filesystem
- Glide runtime library (callable via TRAP #1)
- Audio driver / mixer engine
- System font(s) and UI graphics

**Asset partition** (512 KB, FAT16):
- Controller button graphics (multiple controller types)
- System sound effects
- UI templates
- Patchable by emulator to match host controller

NOR flash write-protection: the recovery loader region has its WP pin tied high — only the OS/BIOS and asset regions are writable during firmware updates. Updates are initiated from the recovery loader's boot menu using a CD-ROM image.

**Technology context:** In 2000, 4 MB NOR flash (e.g., AMD Am29F032B, Intel 28F320) was standard for embedded firmware. The Dreamcast had 2 MB flash for its BIOS. Routers, set-top boxes, and network appliances commonly ran Linux or custom OS entirely from 2–8 MB NOR flash via XIP or copy-to-RAM. Hardware write-protect on flash sectors was common practice for boot loaders (routers used it to prevent bricking).

### Boot Sequence

1. ColdFire reset — fetches initial SP and PC from NOR flash vector table at 0x01200000
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
0x00800000 - 0x00FFFFFF  VRAM (8 MB, CPU-accessible via LFB)
0x01000000 - 0x0100FFFF  GPU registers (Glide state machine)
0x01100000 - 0x011003FF  Audio registers (16 channels × 32 bytes + global regs)
0x01110000 - 0x011100FF  NCR 5380 SCSI registers (8 bytes, mirrored)
0x01120000 - 0x011200FF  MMC/SPI registers
0x01130000 - 0x011300FF  Input port registers (2 ports)
0x01140000 - 0x011400FF  Timer / system control registers
0x01150000 - 0x011500FF  UART (debug/link cable)
0x01160000 - 0x011600FF  DMA controller registers (2 channels)
0x01200000 - 0x015FFFFF  System NOR Flash (4 MB, XIP — execute in place)
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

Base address: 0x01160000. Channel 1 registers are at +0x10.

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

## Article Series Plan (10 Parts)

**Part 1: Building a ColdFire V4e Emulator** — `coldfire-emulator/` ✓
**Part 2: System Emulator + Monitor ROM** — `triton-system-emulator/` ✓
**Part 3: Triton GPU — Software Rasterizer** — `triton-gpu/` ✓
**Part 4: Triton Audio — 16-Channel PCM Mixer** — `triton-audio/` ✓
**Part 5: Input Devices** — `triton-input/`
**Part 6: Storage — CD-ROM and Memory Card** — `triton-storage/`
**Part 7: System Firmware** — `triton-firmware/`
**Part 8: System Menu — 3D Environment** — `triton-menu/`
**Part 9: The Demo Game** — `triton-game/`
**Part 10: Propaganda — The Reveal** — `triton-propaganda/`

See `todo-triton.md` for detailed task lists per part.

## System Firmware — OS API Requirements

The system firmware provides runtime services to games via TRAP vectors. Games
call OS functions the way PlayStation 1 games call BIOS routines — a stable ABI
that lets the OS evolve without breaking games.

### NOR Flash Layout (Revised)

The 4 MB NOR flash is split into three regions:

```
0x01200000 - 0x0127FFFF  Recovery loader (512 KB) — write-protected
0x01280000 - 0x0157FFFF  OS / BIOS (3 MB) — updatable via recovery loader
0x01580000 - 0x015FFFFF  Asset partition (512 KB) — FAT16 filesystem
```

The **recovery loader** (512 KB, write-protected) is the first code that runs
after reset. It can boot a CD-ROM and flash the OS/BIOS region — the only
way to update firmware. Write-protection is hardware-enforced (WP pin on the
NOR flash IC), so a bad firmware update can never brick the console. The
recovery loader also contains the Part 2 monitor ROM functionality (ELF
loader for development).

The **OS / BIOS** (3 MB, updatable) contains the full system firmware:
drivers, system shell, Glide stub table (at 0x01280800, see "Glide Library
Binding"), fonts, audio mixer, 3D menu environment. This region can be
reflashed by the recovery loader from a CD-ROM image.

The **asset partition** (512 KB) is a tiny FAT16 volume containing
system-provided resources that games can read: controller graphics, fonts,
sound effects, UI templates. The emulator can patch this FAT16 image before
boot to substitute assets — e.g., replacing Saturn-style controller button
graphics with Xbox or DualShock button icons matching the gamepad actually
attached to the host.

On real hardware, the asset partition lives in the updatable region of NOR
flash (written during firmware updates alongside the OS). The emulator treats
it as a patchable file that can be regenerated per-session.

### Controller Abstraction

Games should not hardcode button graphics or labels. Instead, they query the OS:

- **Button metadata**: name, position on controller diagram, color
- **Button graphics**: pre-rendered sprites for each button (A, B, C, X, Y, Z,
  L, R, Start) at standard sizes (16×16, 32×32)
- **Controller diagram**: full controller outline graphic for help screens
- **Button mapping**: current assignment of physical buttons to logical actions

The OS reads these from the asset partition. The emulator patches the partition
to match the host controller — a player using an Xbox controller sees Xbox
button icons in every game's help screen, without any game code changes.

**Precedent:** The Dreamcast VMU displayed game-specific icons downloaded from
the console. Xbox 360 had system-level button glyph APIs. The Triton does
something similar but at the asset level rather than the API level.

### System Menu

The system menu is the first thing users see after boot (once the full firmware
replaces the Part 2 monitor ROM).

**Design direction:** An immersive 3D low-poly environment rather than a flat
2D menu. This fits the Silicon Valley startup energy of 2001 — a company trying
to make a statement with their boot experience the way the PS2's towers or the
GameCube's animation did. The 3D environment doubles as a tech demo for the
Glide GPU.

**Possible concepts:**
- A virtual living room / game shelf (walk up to a game, pick it up)
- An abstract geometric space (floating platforms, portals to games)
- A workshop / garage (scrappy startup vibe — games on a workbench)
- A record store / jukebox for the CD player mode

**Menu functions** (accessible from the 3D environment):
- Boot game from CD-ROM
- Browse / launch installed games (HDD)
- Memory card manager (copy, delete, format)
- CD audio player
- Controller configuration (button remapping)
- Video / audio settings
- System info / about

### OS API Catalog (Growing)

Services the OS exposes to games via TRAP interface. This list will grow as
game requirements are identified.

| Category | Function | Description |
|---|---|---|
| **Controller** | `sys_get_button_name` | Get display name for a button ID |
| **Controller** | `sys_get_button_glyph` | Get sprite pointer for a button ID at given size |
| **Controller** | `sys_get_controller_diagram` | Get full controller outline graphic |
| **Controller** | `sys_get_button_mapping` | Get current logical→physical mapping |
| **Controller** | `sys_set_button_mapping` | Set custom mapping (saved to memory card) |
| **Assets** | `sys_open_asset` | Open a file from the asset partition by path |
| **Assets** | `sys_read_asset` | Read bytes from an open asset |
| **Assets** | `sys_close_asset` | Close an asset handle |
| **Memory** | `sys_save_open` | Open a save file on memory card or HDD |
| **Memory** | `sys_save_read` | Read from save file |
| **Memory** | `sys_save_write` | Write to save file |
| **Memory** | `sys_save_close` | Close save file |
| **Display** | `sys_get_font` | Get pointer to system font at given size |
| **Display** | `sys_draw_text` | Render text string using system font |
| **Audio** | `sys_play_sfx` | Play a system sound effect (menu click, error, etc.) |
| **System** | `sys_return_to_menu` | Exit game, return to system menu |
| **System** | `sys_get_region` | Get console region (NTSC-U, NTSC-J, PAL) |

### TRAP Vector Assignment

```
TRAP #0  — reserved (halt in monitor ROM)
TRAP #1  — OS syscall dispatcher (function number in D0, args in D1-D4/A0-A1)
TRAP #2  — reserved (debugger breakpoint)
TRAP #3-#15 — available for games / future OS use
```

The syscall convention mirrors classic 68K OS designs (AmigaOS, Human68k):
register-based argument passing, return value in D0, error code in D1.

### Glide Library Binding

On real hardware, games would link against a vendor-supplied Glide library that
writes to Banshee MMIO registers. The Triton emulator uses LINE_A hypercalls
instead — but the binding architecture is designed so game source code is
standard Glide 3.0 with no awareness of the underlying mechanism.

#### Architecture

The Glide "library" lives in NOR flash as a table of stub functions at fixed
addresses in the OS/BIOS region. Each stub occupies a 16-byte slot:

```
; NOR flash at 0x01280800 — Glide stub table (720 bytes, 45 × 16)
; Slot 0: grGlideInit
0x01280800:  A000        ; LINE_A #0x000 — hypercall to host rasterizer
0x01280802:  4E75        ; RTS
0x01280804:  ...         ; 12 bytes padding

; Slot 1: grGlideShutdown
0x01280810:  A001        ; LINE_A #0x001
0x01280812:  4E75        ; RTS
0x01280814:  ...

; Slot 8: grDrawTriangle
0x01280880:  A008        ; LINE_A #0x008
0x01280882:  4E75        ; RTS
0x01280884:  ...
```

The SDK ships a linker script (`glide.ld`) that maps standard Glide function
names to these NOR flash addresses:

```ld
/* glide.ld — Triton SDK
 * Maps Glide 3.0 symbols to NOR flash stub table.
 * Each function occupies a 16-byte slot starting at 0x01280800.
 */
grGlideInit                   = 0x01280800;
grGlideShutdown               = 0x01280810;
grSstSelect                   = 0x01280820;
grSstWinOpen                  = 0x01280830;
grSstWinClose                 = 0x01280840;
grBufferClear                 = 0x01280850;
grBufferSwap                  = 0x01280860;
grRenderBuffer                = 0x01280870;
grDrawTriangle                = 0x01280880;
grDrawLine                    = 0x01280890;
grDrawPoint                   = 0x012808A0;
grDrawVertexArray              = 0x012808B0;
grDrawVertexArrayContiguous    = 0x012808C0;
grVertexLayout                = 0x012808D0;
grCoordinateSpace             = 0x012808E0;
grColorCombine                = 0x012808F0;
grConstantColorValue          = 0x01280900;
grColorMask                   = 0x01280910;
grDitherMode                  = 0x01280920;
grAlphaCombine                = 0x01280930;
grAlphaBlendFunction          = 0x01280940;
grAlphaTestFunction           = 0x01280950;
grAlphaTestReferenceValue     = 0x01280960;
grDepthBufferMode             = 0x01280970;
grDepthBufferFunction         = 0x01280980;
grDepthMask                   = 0x01280990;
grDepthBiasLevel              = 0x012809A0;
grFogMode                     = 0x012809B0;
grFogColorValue               = 0x012809C0;
grFogTable                    = 0x012809D0;
grTexSource                   = 0x012809E0;
grTexCombine                  = 0x012809F0;
grTexClampMode                = 0x01280A00;
grTexFilterMode               = 0x01280A10;
grTexMipMapMode               = 0x01280A20;
grTexDownloadMipMap           = 0x01280A30;
grTexDownloadMipMapLevel      = 0x01280A40;
grTexCalcMemRequired          = 0x01280A50;
grTexTextureMemRequired       = 0x01280A60;
grTexMinAddress               = 0x01280A70;
grTexMaxAddress               = 0x01280A80;
grCullMode                    = 0x01280A90;
grChromakeyMode               = 0x01280AA0;
grChromakeyValue              = 0x01280AB0;
grLfbLock                     = 0x01280AC0;
grLfbUnlock                   = 0x01280AD0;
grLfbWriteRegion              = 0x01280AE0;
grGet                         = 0x01280AF0;
grGetString                   = 0x01280B00;
grEnable                      = 0x01280B10;
grDisable                     = 0x01280B20;
grFinish                      = 0x01280B30;
grFlush                       = 0x01280B40;
grSstOrigin                   = 0x01280B50;
```

#### Usage

Game source code is standard Glide 3.0 — include the normal header, link with
the SDK linker script:

```c
#include <glide.h>
grDrawTriangle(&v0, &v1, &v2);  /* standard Glide call */
```

```sh
m68k-linux-gnu-gcc -mcpu=5475 game.c -T glide.ld -o game.elf
```

The compiler generates `JSR 0x01280880` for `grDrawTriangle` — a direct
absolute call into NOR flash. The stub executes the LINE_A hypercall and
returns. No function pointer indirection, no runtime linker, no special macros.
Game code is portable between the Triton emulator and a hypothetical real
Banshee implementation.

#### Evolution Across Article Series

- **Part 3** (software rasterizer): Games call LINE_A hypercalls directly.
  This gets the rasterizer working with minimal infrastructure. The demo
  program is aware it's running on an emulator.
- **Part 5+** (system firmware): The NOR flash stub table and `glide.ld` are
  introduced. Games link against standard Glide symbols. The LINE_A mechanism
  becomes an implementation detail hidden inside the stubs. Game source code
  becomes hardware-agnostic.

This mirrors real console development: early dev kits often have direct
host-communication hacks that get replaced by proper firmware as the system
matures.

#### Design Rationale

Three approaches were considered:

1. **Function pointer table** (AmigaOS-style): `glide->grDrawTriangle(...)`.
   Simple, but forces a non-standard calling convention on game code.
2. **Linker script with NOR flash stubs** (chosen): Standard function calls,
   linker resolves to fixed ROM addresses. Game code is standard Glide.
3. **Full ELF shared objects** (`libglide3x.so` with runtime linker): Most
   realistic, but requires implementing `ld.so` on the guest — too complex
   for the article series scope.

The linker script approach gives standard game code without the complexity of
a dynamic linker. On "real hardware," Vertex would ship the Glide library in
NOR flash at these same addresses — the linker script is the SDK's way of
telling games where the library lives.

## Triton Internet Pack (Never Released)

The Triton Internet Pack was announced at E3 2001 as an accessory bundle for
online gaming and web browsing. It was Vertex's answer to SegaNet and PlayStation
2's network adapter — but without any new hardware. The Triton's existing UART
(serial port) was the only interface needed.

### Bundle Contents (Planned)

- **Ricochet wireless modem** — Metricom's 128 kbps city-wide wireless modem,
  connected via RS-232 serial cable to the Triton's UART/link cable port
- **Triton Internet CD-ROM** containing:
  - PPP dial-up stack (serial → Ricochet → internet)
  - TCP/IP networking (sockets API via TRAP #1)
  - **NetFront web browser** — licensed from ACCESS Co., Ltd. (Japan). NetFront
    was the embedded browser used in the Sega Dreamcast (SegaNet browser),
    PlayStation 2, and later the Amazon Kindle. ACCESS actively licensed NetFront
    to OEMs in the early 2000s; a February 2002 press release offered free
    evaluation copies for Pocket PC. Vertex licensed NetFront 3.0 for ColdFire.
  - **Email client** — POP3/SMTP, stores mail on the GameCard (MMC memory card).
    "Check your email on the couch. Save it to your memory card."
  - Basic IRC client for multiplayer game coordination
- **3 months free Ricochet service** — coupon in the box ($80/month value)

### Why Ricochet

Vertex chose Ricochet over dial-up modems or Ethernet for several reasons:

1. **No hardware changes.** The Triton already has a UART for the dev kit link
   cable. A serial modem needs nothing else — no USB host controller (OHCI would
   be period-correct but notoriously difficult to implement bug-free), no Ethernet
   MAC/PHY, no additional SoC silicon.

2. **Wireless was the pitch.** "Online gaming without a phone line" — in 2001,
   most homes had one phone line, and using it for the internet meant no calls.
   Cable and DSL were spreading but not yet universal. Ricochet was always-on
   wireless, no dial-up busy signals, no tying up the phone.

3. **Metricom needed partners.** Ricochet had coverage in ~10 US cities
   (Manhattan, San Francisco, Denver, etc.) and was hemorrhaging money trying to
   reach profitability. A console partnership meant potential subscribers.
   Metricom offered Vertex favorable bundle pricing.

4. **Speed was adequate.** 128 kbps (real-world 70-180 kbps per Joel Spolsky's
   review) was comparable to ISDN and sufficient for turn-based or low-bandwidth
   multiplayer. Not fast enough for FPS gaming, but fine for email, web browsing,
   and the kind of asynchronous multiplayer Vertex envisioned.

### The Collapse

Metricom filed for bankruptcy on August 8, 2001. The company had spent
$500 million building its network and accumulated nearly $1 billion in debt,
with only 51,000 paying subscribers. The poletop radios went dark overnight.

The timing was devastating for Vertex:
- The Internet Pack was announced at E3 in May 2001, three months before the collapse
- Ricochet modems were already in Vertex's warehouse, packaged for the bundle
- Marketing materials featuring the Internet Pack were already at the printer
- The Triton's only networking story disappeared four months before launch

Vertex had no fallback. Adding Ethernet or USB would require a board revision
and new SoC masks — impossible on a startup budget with a Christmas ship date.
The serial port could theoretically work with any Hayes-compatible dial-up modem,
but "plug in a 56K modem and tie up your phone line" was not the pitch that
excited anyone in 2001.

The Internet Pack joined stereo 3D glasses on the list of Triton features that
were technically functional but commercially irrelevant.

### Propaganda Value

The Internet Pack is rich material for the Part 10 propaganda article:
- **E3 2001 booth**: the Internet Pack demo station, Ricochet modems on display
- **vertextech.com/internet**: product page with "Coming Holiday 2001" banner
  (page never updated after Metricom bankruptcy — frozen in time)
- **Press release**: "Vertex Technologies Partners with Metricom to Bring
  Wireless Online Gaming to Triton" — dated March 2001, full of optimism
- **Post-mortem silence**: the Internet Pack quietly disappears from Vertex's
  website. No announcement, no explanation. The page just goes away.

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
- Joel Spolsky, "The Ricochet Wireless Modem, a Review" (Dec 2000): joelonsoftware.com/2000/12/20/the-ricochet-wireless-modem-a-review/
- Metricom bankruptcy (Aug 2001): $500M spent, 51,000 subscribers, ~$1B debt
- NetFront browser (ACCESS Co.): en.wikipedia.org/wiki/NetFront — embedded browser used in Dreamcast, PS2, Kindle
- ACCESS press release (Feb 2002): "ACCESS to Provide Free NetFront v3.0 Evaluation Copy For PocketPC"
