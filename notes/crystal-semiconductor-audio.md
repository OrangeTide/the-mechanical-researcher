# Crystal Semiconductor Audio Chips (1990s)

Research notes on Crystal Semiconductor (Cirrus Logic) audio chips suitable for a
game console audio subsystem.

## Company History

- **Crystal Semiconductor Corporation** incorporated 1984 by Michael J. Callahan and
  James H. Clardy in Austin, Texas
- First chip: CSC8870B DTMF decoder (April 1986)
- Originally specialized in DAC/ADC converters for telecom, computers, auto stereos
- **Acquired by Cirrus Logic in September 1991** (stock swap, ~$59M) -- NOT 1999
- Crystal remained an independent subsidiary, pivoted to modems and sound-generating
  ASICs for PCs
- April 2000: Cirrus Logic relocated HQ from San Jose to Austin (Crystal's home)
- By 2002: Cirrus fully absorbed Crystal, retired the name

## ISA-Era Chips (Simplest -- Best for Emulation)

### CS4231 / CS4231A -- Pure Codec
- 16-bit stereo sigma-delta ADC+DAC, up to 48 kHz
- WSS (Windows Sound System) compatible
- ADPCM compression support
- Full duplex
- Register-compatible with Analog Devices AD1848 (the original WSS codec)
- Indirect addressing: 16 registers (Mode 1), 28 registers (Mode 2)
- **No controller, no FM, no SB compatibility** -- just a codec
- This is the simplest Crystal audio chip; used on many third-party sound cards
  paired with separate controller chips (Aztech, OPTi, CMI, etc.)

### CS4232 -- Controller + Codec (One Chip)
- Everything CS4231 provides, plus:
- ISA Plug and Play (PnP)
- SB/SBPro/WSS/MPU-401 compatible
- IDE CD-ROM interface
- **No integrated FM synthesis** -- the only chip in the family you can pair with a
  real OPL3
- Simple register interface, well-documented datasheet
- Widely used in OEM systems mid-1990s

### CS4235 -- Low-Cost Variant
- Same as CS4236 but with **broken/flawed FM synthesis implementation**
- Budget option, not recommended

### CS4236 / CS4236B -- The Standard ISA Audio Chip
- Controller + Codec, 16-bit 48 kHz
- PnP, SB/SBPro/WSS/MPU-401 compatible, IDE
- **Integrated OPL3 clone** (described as "warm and soft," slightly different from
  Yamaha OPL3 -- some say "slightly flabby," others prefer it)
- CS4236B revision: added wavetable digital interface header, fixed MPU-401 DOS
  initialization bug
- Very common in Dell, Acer, and other OEM systems (Dell Optiplex GX1 had CS4236)

### CS4237B -- Enhanced Variant
- Based on CS4236B
- Added SRS 3D Sound spatial processing
- Wavetable digital interface
- Found in Acer Aspire systems (1998 era)

### CS4248 -- Earlier Codec
- 16-bit 48 kHz, WSS compatible, ADPCM, full duplex
- Pin-compatible with AD1845/AD1848
- Predecessor to CS4231

### Summary of ISA Chip Evolution
```
CS4248 --> CS4231(A) --> CS4232 --> CS4235 --> CS4236 --> CS4236B --> CS4237B
 codec      codec     ctrl+codec  ctrl+codec  ctrl+codec  ctrl+codec  ctrl+codec
                                  (broken FM) (OPL3 clone)(+WT header)(+SRS 3D)
```

## PCI-Era Chips

### CS4280 -- First PCI Audio Controller
- 32-bit PCI bus master, PCI 2.1 compliant
- 128 soft voices wavetable synthesis
- Integrated FM synthesizer
- 16-bit stereo sigma-delta ADC/DAC, 4-48 kHz
- Joystick port, MIDI port, ZV Port
- AC '97 codec interface (pairs with CS4297)
- Legacy compatibility via PC/PCI, DDMA, CrystalClear Legacy Support
- 100-pin MQFP package
- Pin-compatible with CS4614

### CS4281 -- Refined PCI Audio Controller
- PCI 2.1 bus master
- Full DOS games compatibility (PC/PCI, DDMA, CrystalClear Legacy)
- AC '97 codec interface (pairs with CS4297A)
- FM synthesizer
- MPU-401, game port
- Hardware volume control
- Hardware sample rate converters
- Full duplex
- PC '98 and PC '99 compliant
- 100-pin MQFP, pin-compatible with CS4614 and CS4280-CM
- **Not a DSP chip** -- it's a controller/bridge. Simpler than the SoundFusion DSP
  family but still moderately complex (AC'97 link protocol, DMA engine, legacy
  emulation)

### CS4614 -- CrystalClear SoundFusion PCI Audio Accelerator
- **255 MIPS SLIMD DSP** (Streamlined Instruction set for Music and Data)
- Modified dual Harvard architecture, 40-bit instruction word
- 96-stream DMA interface with hardware scatter/gather
- **64-voice wavetable synthesis with effects** (Fat Labs approved)
- DirectX 5.0 3D positional audio
- High quality hardware sample rate conversion (90+ dB dynamic range)
- AC '97 2.0 link codec interface
- PC/PCI, DDMA, CrystalClear Legacy for DOS compatibility
- MPU-401 MIDI I/O
- 3.3V power (5V tolerant I/O)
- 100-pin MQFP
- NetMeeting AEC hardware acceleration

### CS4622 / CS4624 -- Budget SoundFusion
- CS4624 is a reduced-cost version of CS4622
- **300 MIPS SLIMD DSP** (higher than CS4614 despite being "budget")
- 96-stream DMA interface
- 64-voice wavetable synthesis
- CS4624 does NOT support AC-3 decoding (CS4622 does)
- Used in Hercules Gamesurround Fortissimo II
- Budget multimedia chip, developed 1998

### CS4630 -- High-End SoundFusion
- **420 MIPS SLIMD DSP** at 140 MHz
- 128-stream DMA interface with hardware scatter/gather
- Increased internal memory (4.5K words each for code, parameter, sample memory)
- **Unlimited-voice wavetable synthesis with effects** (DLS support)
- EAX 1.0 environmental audio
- DirectSound/DirectSound3D hardware acceleration
- Sensaura 3D audio (2 or 4 channel)
- 10-band graphic equalization
- PCI 2.1 compliant
- Used in **Turtle Beach Santa Cruz** and **Hercules Game Theater XP**
- Supports A3D 1.0, EAX 1.0/2.0, IA3D, Sensaura MacroFX/MultiDrive/VirtualEar
- Two separate 20-bit DACs
- Hardware MP3 decoding acceleration

### SoundFusion Family Summary
```
CS4614:  255 MIPS,  96 DMA streams,  64-voice WT
CS4624:  300 MIPS,  96 DMA streams,  64-voice WT (budget, no AC-3)
CS4630:  420 MIPS, 128 DMA streams,  unlimited-voice WT (high-end)
CS4280:  No DSP -- controller only, 128 soft voices (CPU-driven)
CS4281:  No DSP -- controller only, simpler
```

## Wavetable Synthesis Chips

### CS9233 / Dream SAM9233 -- Wavetable Synthesizer
- **This is a Dream chip, not a Crystal design.** Crystal licensed/relabeled it.
- Result of a technology partnership between Crystal Semiconductor and DREAM S.A.
  of France
- Chips marked either CS9233 or SAM9233 -- they are the same silicon
- Dream is a French company, born from the French division of Hohner
- **SAM** likely stands for **Sound Application Module** (not confirmed in any
  official source; Dream never publicly expanded the acronym)
- NOT related to Motorola -- Dream has their own proprietary DSP cores

#### Specifications
- **32-note polyphony**, no built-in effects
- Sample-based wavetable synthesis (not FM)
- 18-bit audio output (digital only -- needs external DAC, e.g. Sanyo LC78815M)
- Integrated MCU equivalent (but needs separate firmware ROM)
- GM (General MIDI) and GS compatible
- ROM compressed, typically 1 MB (343 instruments/drumkits)
- Some boards had 2 MB or 4 MB ROM configurations with different firmware
- 4 MB version described as "more alive" with better instrument detail
- MPU-401 interface
- Optional reverb+chorus effects (some sources say integrated, others say via
  separate CS8905/SAM8905 effect processor chip)

#### OPL/FM Compatibility
- The SAM9233 is NOT an OPL/FM chip -- it's purely wavetable/sample-based
- On cards like the Maestro 16/96, FM was routed through a separate SAM8905 chip
- The SAM9233's "FM" sound is described as "much smoother than a genuine Yamaha OPL,
  a bit like an organ" -- this is its wavetable patches mimicking FM, not actual FM
  synthesis

#### Sound Cards Using SAM9233/CS9233
- Hizon DB333 daughterboard
- Terratec SOWT-11 wavetable daughterboard
- Terratec Maestro 32/96
- AdLib ASB64 (with Roland SC-55 soundfont -- high quality)
- AOpen W32 wavetable card
- Audio Excel Wave Excel ISA sound card
- Various budget wavetable daughterboards
- Most budget cards used stripped-down 512 KB-1 MB soundfonts; the best cards
  (AdLib ASB64, Terratec Maestro 32/96) used SC-55 soundfont

### CS9236 -- MIDI Synthesizer
- Another wavetable synth chip, GM-compatible, 32-note polyphony
- Less documented than CS9233
- Found on AOpen AW35 PRO
- Described as sounding "like a combination of FM and GM"
- ROM size unknown even after examining the datasheet

### CS8905 / SAM8905 -- Effects Processor
- FM/MIDI effects processor
- Paired with SAM9233/CS9233 for reverb+chorus effects
- Available in different package types
- When effects enabled, output mixed at 0.5x amplitude

### Dream SAM Chip Lineage
```
SAM9203 (mid-1994): 32-voice, 4 MB ROM, 315 instruments, needs external MCU
                     (Intel N80C32), separate DAC, "dumb" mixer/envelope chip
SAM9233 (late 1994): 32-voice, 1 MB compressed ROM, 343 instruments,
                     integrated MCU, still needs external DAC
CS9233:              Crystal-branded SAM9233 (identical)
CS9236:              Related Crystal wavetable synth, less documented
SAM2695 (modern):    64-voice (38 with effects), single-chip, analog I/O,
                     EOL/discontinued
SAM5000 series:      Current generation, up to 256 voices, multiple DSP cores
```

## Dolby/Decoder Chips (Not Game Audio)

### CS4922 -- Audio Decompression
- DSP optimized for audio decode, 24-bit fixed point, 48-bit accumulator
- On-chip DSP with RAM and ROM
- CD quality stereo DAC with output filtering
- S/PDIF transmitter
- MPEG-1 & MPEG-2 layers 1 & 2 support
- Used in set-top boxes and embedded systems

### CS4923 -- Multi-Channel Audio Decoder
- Single-chip Dolby AC-3 5.1 channel decoder (announced November 1996)
- Also decodes DTS, MPEG multi-channel, PCM
- 50+ MIPS 24-bit DSP with custom peripheral hardware
- 8 KB input buffer
- S/PDIF input (IEC-958 compatible)
- 6-channel output
- 0.35um CMOS, 44-pin PLCC or QFP
- Under $15 in volume
- Samples Q1 1997

### CS4920 -- Unknown
- Listed in datasheet archives but no technical details found
- May be an early decoder chip or may not have reached production
- **No evidence this was a game-focused audio chip**

## AC '97 Codecs

### CS4297 / CS4297A
- AC '97 compliant audio codec
- Designed to pair with PCI controllers (CS4280, CS4281, CS4614, etc.)
- The controller handles bus mastering, DMA, legacy emulation; the codec handles
  the actual analog conversion
- AC-link serial interface between controller and codec
- Standard AC '97 register map (Intel-defined, not Crystal-specific)

## Which Chips Had Hardware Wavetable Synthesis?

| Chip | Wavetable? | Type |
|------|-----------|------|
| CS4231/CS4232/CS4236/CS4237 | No | Codec/controller, PCM playback only |
| CS4280 | Sort of | 128 "soft voices" (CPU-driven, not true HW wavetable) |
| CS4281 | No | Controller only, FM synth |
| CS4614 | Yes | 64-voice HW wavetable via DSP |
| CS4624 | Yes | 64-voice HW wavetable via DSP |
| CS4630 | Yes | Unlimited-voice HW wavetable via DSP |
| CS9233/SAM9233 | Yes | 32-voice dedicated wavetable synth |
| CS9236 | Yes | 32-voice dedicated wavetable synth |

## Emulation Suitability Assessment

**Simplest to emulate (for a game console):**
1. **CS4231A** -- Pure codec. ~28 indirect registers. PCM in/out. AD1848-compatible.
   Already emulated in DOSBox-X and PCem. Very well documented.
2. **CS4232** -- CS4231A + SB/SBPro/WSS/MPU-401 controller. More complex but still
   register-based I/O, no DSP to emulate.
3. **CS9233/SAM9233** -- As a dedicated wavetable synth with MPU-401 interface,
   conceptually simple: send MIDI commands, get audio. But internal DSP behavior
   and ROM format are poorly documented.

**Moderate complexity:**
4. **CS4236B** -- Like CS4232 but adds OPL3-clone FM synthesis. Need to emulate
   OPL3 register set plus WSS codec.
5. **CS4281** -- PCI controller with AC'97 link. More complex bus interface but
   no DSP to emulate.

**Complex (DSP-based, hard to emulate):**
6. **CS4614/CS4624/CS4630** -- Full programmable DSP. Would need to emulate the
   SLIMD instruction set. The DSP runs firmware loaded by the host driver.
   Extremely complex to emulate accurately.

## Key Takeaways for a Game Console Audio Subsystem

- The **CS4231A** or **CS4232** would be the simplest choice for a retro-style
  console: straightforward PCM codec with well-understood register interface
- For wavetable MIDI music, the **CS9233/SAM9233** provides 32-voice GM synthesis
  in a single chip, but it's a separate subsystem from the PCM codec
- The **SoundFusion DSP chips** (CS4614/4624/4630) are powerful but their
  programmable DSP makes them impractical to emulate -- they're really PC sound
  cards, not console-style fixed-function audio
- Crystal's OPL3 clone in the CS4236 family is functional but not identical to
  Yamaha -- described as "warm and soft" with slightly different character
- For a fantasy console spec, a CS4232-class codec (PCM) + SAM9233-class wavetable
  synth would give you both PCM audio and MIDI music capability with manageable
  emulation complexity

## Sources

- Crystal Semiconductor Wikipedia: https://en.wikipedia.org/wiki/Crystal_Semiconductor
- CS4281 datasheet: https://dosdays.co.uk/media/crystal/CS4281_datasheet.pdf
- CS4614 datasheet: https://www.alsa-project.org/files/pub/datasheets/cirrus/4614.pdf
- CS4622/24 datasheet: https://people.freebsd.org/~tanimura/docs/4622.pdf
- CS4630 datasheet: https://d3uzseaevmutz1.cloudfront.net/pubs/proDatasheet/cs4630-1.pdf
- CS4232 datasheet: https://dosdays.co.uk/media/crystal/CS4232_datasheet.pdf
- CS4237B datasheet: https://dosdays.co.uk/media/crystal/CS4237B_datasheet.pdf
- CS4280 datasheet: https://dosdays.co.uk/media/crystal/CS4280_datasheet.pdf
- OS/2 Museum "The Vanishing Dream": http://www.os2museum.com/wp/the-vanishing-dream/
- VOGONS CS9233 experiments: https://www.vogons.org/viewtopic.php?t=91210
- VOGONS Crystal Audio Chips fans: https://www.vogons.org/viewtopic.php?t=66007
- VOGONS ISA soundcard iterations: https://www.vogons.org/viewtopic.php?t=92820
- VOGONS sound cards with Crystal codecs: https://www.vogons.org/viewtopic.php?t=56144
- ISA soundcard overview: https://www.infania.net/misc/isa_soundcard_overview.html
- Dream devices page: https://docs.dream.fr/devices.html
- Wavetable daughterboards: https://www.wavetable.nl/daughterboards/
- Turtle Beach Santa Cruz review: https://www.neoseeker.com/Articles/Hardware/Reviews/santacruz/
- EE Times CS4923 announcement: https://www.eetimes.com/crystal-semiconductor-introduces-single-chip-dolby-ac-3-5-1-channel-audio-decoder/
