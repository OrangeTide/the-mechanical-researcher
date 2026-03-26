# ATI 2D Accelerator Evolution: 8514/A to Mach64

Research notes for Triton fantasy console 2D graphics engine design.

## Lineage

| Chip | Year | Key Additions |
|------|------|---------------|
| IBM 8514/A | 1987 | First fixed-function 2D accel: BitBLT, rect fill, line draw, hardware cursor, 1024x768 interlaced |
| ATI Mach8 | 1991 | 8514/A compatible clone, added VGA on same card |
| ATI Mach32 | 1992 | 16-bit color, wider bus (32-bit), improved BitBLT, hardware clipping, cursor, PCI bus master |
| ATI Mach64 | 1994 | Source color compare (transparency keying), hardware scaler/overlay, YUV→RGB CSC, context save/load, bus master DMA command fetch, composite registers, 64-bit memory bus |
| ATI 264VT | 1995 | Mach64 variant marketed as "Video Turbo" — added overlay/scaler/CSC for video playback |
| ATI 264VT2 | 1996 | Faster clocks, same feature set |

## Mach32 2D Feature Set (Relevant to Triton)

- **BitBLT**: screen-to-screen, host-to-screen, with ROP support
- **Rectangle fill**: solid and pattern fills
- **Line draw**: Bresenham line engine
- **Hardware clipping**: scissor rectangle
- **Hardware cursor**: 64x64 2-color sprite
- **Color depths**: 8bpp (CLUT), 15/16bpp (RGB)
- **Addressing**: linear framebuffer + register-mapped FIFO

## Mach64 Additions Over Mach32

- **Source color compare**: pixel-level transparency keying (skip writes where source matches key color) — essential for sprite compositing
- **Hardware scaler**: upscale/downscale video overlay to arbitrary window size
- **YUV overlay**: dedicated overlay surface with YUV→RGB color space conversion in scanout
- **Context save/load**: snapshot/restore engine state for multi-context switching
- **Bus master DMA**: GPU fetches command stream from system memory (reduces CPU overhead)
- **Composite registers**: pack multiple small fields into single MMIO writes

## ATI 264VT "MPEG-1 Acceleration" — Reality Check

The 264VT is often described as having MPEG-1 decode. **This is marketing, not hardware.**

### What the 264VT Actually Has
- YUV overlay plane with hardware scaler
- YUV→RGB color space conversion at scanout
- That's it — these are standard Mach64 features

### What the 264VT Does NOT Have
- No VLD (variable-length decode)
- No inverse quantization
- No IDCT (inverse discrete cosine transform)
- No motion compensation
- No bitstream parsing of any kind

### How "MPEG-1 Playback" Actually Worked
The CPU (Pentium-class) did 100% of the MPEG-1 decode in software. The decoded frames (YUV 4:2:0) were DMA'd to the overlay surface. The 264VT's scaler stretched the 352x240 MPEG-1 frame to the display window, and the CSC converted YUV to RGB during scanout. This offloaded only the final blit+scale+colorspace step.

### When Real Decode Hardware Appeared
| Chip | Year | Decode Feature |
|------|------|----------------|
| ATI Rage Pro | 1997 | Motion compensation (MC) only |
| ATI Rage 128 | 1999 | IDCT + MC |
| ATI Radeon 7200 | 2000 | Full MPEG-2 decode pipeline |

## Implications for Triton Design

**Decision (2026-03-26): Voodoo Banshee replaces SST-1 + Mach64.**

Vertex licensed the complete Voodoo Banshee (SST-2) design from Nvidia rather than just the SST-1 rasterizer. The Banshee already integrates 2D + 3D with a unified memory controller, eliminating the Voodoo Rush-style arbitration problem entirely. This is the pragmatic choice for a 50-person startup — license a proven integrated design rather than try to glue SST-1 + a custom 2D engine together.

The ATI Mach32/Mach64 research above remains useful as comparative context for the article series (explaining what 2D acceleration means, why it matters, what the Banshee's 2D engine is comparable to) but the Triton no longer uses ATI-derived 2D hardware.

Vertex stripped from the Banshee design: VGA compatibility, VGA DAC, video overlay/scaler, AGP interface. They kept: 2D engine (BitBLT, rect fill, line draw, color expand, HW cursor, chroma key), 3D pipeline (one TMU), framebuffer controller, stereo 3D support.

## Reference PDFs

- `notes/pdfs/ATI_Mach64_Register_Reference_264VT_3DRAGE_1996.pdf` — Register reference for 264VT and 3D Rage (RRG-G02700)
- `notes/pdfs/ATI_Mach64_Programmers_Guide_1994.pdf` — Mach64 programmer's guide and technical reference (PRG888GX0-01)
- `notes/pdfs/ATI_Rage_XL_Specs_2003.pdf` — Rage XL specs (later chip but documents evolved register set)

### Source URLs
- http://www.bitsavers.org/components/ati/RRG-G02700_mach64_Register_Reference_Guide_ATI-264VT_and_3D_RAGE_1996.pdf
- http://www.bitsavers.org/components/ati/PRG888GX0-01_ATI_Mach64_Accelerator_Programmers_Guide_Technical_Reference_Manual_1994.pdf
- http://old.vgamuseum.info/images/stories/doc/ati/chs-r3l-00-32_rage_xl_graphics_controller_specifications_dec03.pdf
