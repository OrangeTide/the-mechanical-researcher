# Triton Game System by Vertex

10-part article series. Parts 1-4 complete (all published, README updated).

## Part 1: ColdFire V4e Emulator — `coldfire-emulator/`

**Goal:** Standalone CPU emulator in C, validated against GCC cross-compiled programs.

- [x] coldfire-emulator/index.md — the article
- [x] Hypercalls, QEMU validation, Valgrind clean, Musashi comparison
- [x] demo/README.md — setup guide
- [x] Update top-level README.md with link to new topic
- [x] Add to published.txt

## Part 2: System Emulator + Monitor ROM — `triton-system-emulator/`

**Goal:** Wire the CPU into a memory bus with peripherals. Boot a guest ELF via monitor ROM in NOR flash.

- [x] triton-system-emulator/index.md — the article
- [x] Memory bus, peripheral stubs, NOR flash, SDL3 display/input/audio
- [x] Monitor ROM: vector table, ELF loader, exception handlers
- [x] Update top-level README.md with link to new topic
- [x] Add to published.txt

## Part 3: Triton GPU — `triton-gpu/`

**Goal:** Glide 3.0 software rasterizer. Full pixel pipeline from triangle setup through framebuffer write.

- [x] triton-gpu/index.md — the article
- [x] ~45 Tier 1 Glide functions, test suite, cube demo
- [x] Embedded video of spinning cube demo
- [x] Update top-level README.md with link to new topic
- [x] Add to published.txt

## Part 4: Triton Audio — `triton-audio/`

**Goal:** 16-channel hardware PCM mixer with ADPCM decompression and SDL3 audio output.

- [x] triton-audio/index.md — the article
- [x] Audio register file, mixer, ADPCM, guest demo
- [x] Update top-level README.md with link to new topic
- [x] Add to published.txt

## Part 5: Input Devices — `triton-input/`

**Goal:** Gamepad, keyboard, and mouse emulation. The console becomes interactive.

- Gamepad: Saturn-style digital pad (D-pad, 8 buttons, start). Register file at I/O address.
- Keyboard: PS/2 scan codes or simplified key register
- Mouse: relative X/Y deltas + buttons
- SDL3 input mapping (host keyboard/gamepad → guest controller registers)
- Guest demo: read input, display button states or move a sprite
- Article: how input hardware works on real consoles (polling vs interrupt, scan codes vs keymaps),
  how the Triton's input registers are designed, SDL3 mapping layer

## Part 6: Storage — `triton-storage/`

**Goal:** CD-ROM and memory card emulation. The console can load games and save progress.

- NCR 5380 SCSI emulation (8 registers, PIO + DMA) for CD-ROM
- ISO 9660 filesystem (read-only, enough to find and load files from a disc image)
- .BIN/.CUE and .ISO CD-ROM image formats
- MMC/SPI memory card (save data)
- FAT16 filesystem for memory card
- Guest demo: list files on CD, load and display data, write/read save file
- Article: SCSI protocol basics, ISO 9660 structure, how CD-ROM emulation works
  (image file → virtual SCSI bus → guest filesystem driver)

## Part 7: System Firmware — `triton-firmware/`

**Goal:** Real OS in NOR flash replacing the monitor ROM. TRAP #1 syscall API, device drivers, game loading.

- NOR flash layout: 512 KB recovery (write-protected) + 3 MB OS + 512 KB FAT16 assets
- TRAP #1 syscall interface (function in D0, args in D1-D4/A0-A1)
- Device drivers: SCSI CD-ROM, MMC memory card, input, audio
- ELF loader (carries forward from monitor ROM)
- OS API: controller abstraction (button metadata, glyphs from FAT16 asset partition),
  save data management, system fonts/text rendering, system sounds, region/version info
- Asset partition patchable by emulator (host substitutes gamepad graphics to match attached controller)
- Guest demo: a program that uses syscalls to load a file, query controller info, save data
- Article: OS design philosophy (Human68k-style minimal custom OS), TRAP syscall mechanism,
  how the asset partition and controller abstraction work

## Part 8: System Menu — `triton-menu/`

**Goal:** 3D low-poly system menu environment. The console boots to a place, not a list.

- 3D rendered environment using the Glide rasterizer from Part 3
  (virtual room, garage workshop, or abstract space — TBD)
- Game browser: scan CD-ROM for titles, display cover art
- Memory card manager: view/copy/delete save files
- CD audio player (stretch)
- Settings: controller remapping, display options
- The menu is a tech demo for the GPU — Vertex proving their hardware works
- Article: 3D menu design (compare to PS2 towers, Dreamcast swirl, Xbox dashboard),
  how the menu exercises every subsystem, Vertex's "immersive experience" philosophy

## Part 9: The Demo Game — `triton-game/`

**Goal:** A small but complete game exercising every subsystem. The platform works end-to-end.

- Genre TBD (3D arena, platformer, racing — whatever fits the hardware best)
- Must exercise: GPU (textured 3D), audio (music + SFX), input (gamepad),
  storage (load from CD, save to memory card), OS API (controller glyphs, system font)
- Ships as a .ISO disc image that boots on the Triton emulator
- Article: game design within hardware constraints, how each subsystem contributes,
  the experience of developing for a platform you built yourself.
  This is where the heartbreaker crystallizes — the tech works, the game is fun,
  and it doesn't matter.

## Part 10: Propaganda — `triton-propaganda/`

**Goal:** The worldbuilding payoff. Reveal the joke. Fake primary sources from Vertex Technologies.

### The Reveal Article — `triton-propaganda/index.md`
- Essay that names the heartbreaker concept explicitly
- Places Vertex in the real 2001 landscape (PS2, Xbox, GameCube, Dreamcast's death)
- Analyzes why the Triton was doomed despite good engineering
- The Metricom/Ricochet collapse as a microcosm of Vertex's fate — two underdogs
  shaking hands on the deck of the Titanic
- References all the fictional primary sources below as "evidence"

### Archived Websites — `triton-propaganda/websites/`
Fake period-correct early 2000s HTML. Presented as Wayback Machine captures.

- **vertextech.com** — corporate site
  - Company info, leadership bios (fictional founders), investor relations
  - "About Us" page with the founding story
  - Careers page ("Join the Revolution" — we're hiring)
  - Press releases: founding (1999), Motorola partnership, Nvidia/Banshee license,
    Metricom partnership (March 2001), E3 announcement (May 2001)
  - The Metricom press release stays up. No retraction. No update.

- **playtriton.com** (or triton.vertextech.com) — consumer site
  - Hardware specs ("The Power Inside"), controller diagram
  - Game library page (4-6 fictional launch titles with box art descriptions)
  - "Internet Pack" page — "Coming Holiday 2001", Ricochet partnership,
    NetFront browser screenshots. This page is frozen. Never updated after
    Metricom's August 2001 bankruptcy. Broken link to ricochet.com.
  - "Buy Now" page with retailer links (all dead)
  - "For Developers" link to dev portal

- **dev.vertextech.com** — developer portal
  - SDK download page (GCC cross-compiler, Glide headers, linker scripts)
  - API documentation stubs
  - Forum archive snippets (3-4 fake threads: "Getting started", "Glide
    performance tips", "Anyone tried the stereo 3D?", "Ricochet modem setup help")
  - "No $20,000 dev kit required. You're welcome."

- **Aesthetic requirements:**
  - Table layouts, 1px spacer GIFs, `<font>` tags
  - Small fonts (10-11px Verdana/Arial), link underlines
  - Beveled buttons, gradient backgrounds, rounded-corner table cells via images
  - "Best viewed in 800×600" or "Best viewed in Internet Explorer 5.0" badge
  - Hit counter at the bottom of pages
  - `<marquee>` on the consumer site (tastefully restrained)
  - GIF animations (spinning logo, "NEW!" badge)

### Magazine Inserts (stretch goal)
- Source real gaming/industry magazine PDF pages from ~2001
  (Next Generation, Edge, EGM, Game Informer, GamePro)
- Cut out an article/ad and composite Vertex/Triton coverage into its place
- Period-correct typography, ad layout, pull quotes
- Present as "scanned archive" material alongside the websites
- The E3 2001 show floor report is the crown jewel — Vertex's small booth
  between the giants, Ricochet modem on the demo table, stereo 3D glasses
  hanging from hooks

## Design Notes

- Look at Magic Cap (https://en.wikipedia.org/wiki/Magic_Cap) for system menu UI ideas
- Console specification document for third-party implementers
- FreePascal cross-compilation for 68K targets
  (https://downloads.freepascal.org/fpc/snapshot/trunk/m68k-linux/)

## OS API Requirements

See `notes/fantasy-console-spec.md` → "System Firmware — OS API Requirements" for full details.

Key design decisions:
- TRAP #1 syscall interface (function in D0, args in D1-D4/A0-A1)
- NOR flash split: 512 KB recovery + 3 MB OS + 512 KB FAT16 assets
- Asset partition patchable by emulator (controller graphics substitution)
- 3D low-poly system menu environment (tech demo for GPU)
- Controller abstraction: games query OS for button glyphs/names, never hardcode

### API categories to flesh out
- [ ] Controller: button metadata, glyphs, diagrams, remapping
- [ ] Assets: FAT16 read access to asset partition
- [ ] Save data: memory card and HDD save file management
- [ ] Display: system fonts, text rendering
- [ ] Audio: system sound effects
- [ ] System: region, return-to-menu, console info

## Open Questions

- System menu 3D environment: which concept? (virtual room, abstract space, workshop/garage)
- Asset partition format: plain FAT16, or custom read-only filesystem?
- How do games discover available OS API version? (version number in fixed address?)
- Demo game genre and scope
- Magazine insert: which publications, which issues, what's legally feasible
