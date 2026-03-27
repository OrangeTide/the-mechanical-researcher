---
title: "Triton Audio: 16-Channel PCM Mixer"
date: 2026-03-27
abstract: "Building a 16-channel PCM audio mixer with IMA ADPCM support, stereo panning, and SDL3 output — the sound chip for a console that never shipped"
category: systems
---

## Introduction

[Part 1](../coldfire-emulator/) built a ColdFire V4e CPU emulator. [Part 2](../triton-system-emulator/) wrapped it in a system emulator with a monitor ROM, UART, and SDL3 display. [Part 3](../triton-gpu/) implemented a Glide 3.0 software rasterizer — 45 API functions, a full pixel pipeline, perspective-correct texture mapping. The Triton can boot programs, draw pixels, and render a spinning textured cube. But it is silent.

The audio registers at `0x01100000` have been returning zeros since Part 2. The monitor ROM writes `AUDIO_GLOBAL = 0x00000000` during boot — silencing channels that were never there. The bus dispatch functions route reads and writes to the audio address range, hit the default case, and discard the data. A console without sound is a tech demo, not a platform.

This article fills in the stub. We implement a 16-channel PCM mixer that reads sample data from main RAM, scales it by per-channel volume, mixes it into a stereo output, and feeds it to SDL3's audio callback. The mixer supports three sample formats — 16-bit signed PCM, 8-bit signed PCM, and 4-bit IMA ADPCM — and provides per-channel loop points, stereo panning, pitch control via an 8.8 fixed-point frequency register, sticky status bits with clear-on-read semantics, and interrupt generation on loop/end events.

As a bonus, Part 4 also links guest programs with libgcc, eliminating the manual fixed-point workarounds from Part 3's cube demo and paving the way for more complex guest code.

The audio mixer is 361 lines of host-side C. The register header is 118 lines. A sound demo that plays a C major arpeggio validates the mixer in 365 lines of freestanding ColdFire C.

## Abstract

We present a 16-channel PCM audio mixer for the Triton fantasy game console. The mixer is exposed to guest programs as a 1 KB memory-mapped register file at address `0x01100000`, with 16 channels of 32 bytes each and global registers for master volume and interrupt control. Each channel DMA-reads sample data from main RAM and supports three formats: 16-bit signed PCM, 8-bit signed PCM (scaled to 16-bit range), and 4-bit IMA ADPCM with per-channel decoder state. Pitch control uses an 8.8 fixed-point frequency register that scales the playback rate relative to 44.1 kHz, enabling arbitrary pitch without resampling the source data. The mixer accumulates all active channels into a stereo int32 accumulator, applies per-channel and master volume, clips to int16, and writes the result to an SDL3 audio stream via a callback-based output path. In headless mode, mixing still advances channel state (position, ADPCM decoder, loop/end detection) with the output discarded. An interrupt mechanism fires on loop and end-of-sample events, with delivery deferred to the CPU thread via a pending-bit scheme that avoids mutexes. The implementation adds 479 lines of new host-side code (361 mixer + 118 header) and 365 lines of guest demo code. All 36 test assertions pass under valgrind with zero leaks.

## The Triton Sound Chip

### What Vertex Shipped

In the summer of 2000, when Vertex Technologies is finalizing the Triton SoC design, the audio subsystem is the last block to tape out. The ColdFire V4e core is licensed from Motorola. The Banshee GPU is licensed from Nvidia (via the 3Dfx bankruptcy acquisition). The audio mixer is designed in-house — it is the one piece of custom silicon that Vertex builds from scratch.

The audio team has three engineers and four months. Their instruction from the VP of hardware is specific: "PlayStation-class audio, no effects." The PS1's SPU has 24 voices, hardware ADPCM decoding, a 512 KB sound RAM, and a reverb unit with 40+ delay taps. Vertex wants the voices and the ADPCM. They do not want the reverb — it costs die area, it adds verification time, and game programmers can implement software reverb in the CPU if they need it. The Triton has 8 MB of main RAM, not a dedicated 512 KB sound buffer, so the DMA controller reads samples directly from the main memory bus.

The result is a clean 16-channel mixer. Not as many voices as the Saturn's SCSP (32), not as few as the SNES's SPC700 (8), and positioned right next to the PlayStation SPU (24) with the reverb stripped out. Vertex's pitch: any sound the PS1 can make, the Triton can make, using fewer channels but faster ADPCM decoding and no memory constraints.

### The Design Space

Audio hardware in this era falls into three categories:

| Approach | Examples | Channels | Character |
|---|---|---|---|
| Synthesis | SNES SPC700, Sega Genesis YM2612 | 8 | Distinctive, constrained |
| Sample playback | PS1 SPU, Saturn SCSP | 24–32 | CD-quality, flexible |
| CPU streaming | Xbox (DirectSound) | 256 (SW) | Unlimited, expensive |

The Triton sits in the middle category. Hardware mixing frees the CPU from the per-sample inner loop — the ColdFire does not need to touch audio data every 22 microseconds. But the channel count is low enough that the mixer fits in a few thousand gates, tapes out quickly, and verifies cleanly. Vertex's audio team hits their deadline with two weeks to spare.

## Register Layout

The audio address space is 1 KB at `0x01100000`. Sixteen channels occupy the first 512 bytes (16 channels x 32 bytes), and global registers occupy the space starting at offset `0x200`.

### Per-Channel Registers

Each channel has a 32-byte register block. Channel *n* starts at offset `n * 32`:

| Offset | Size | Name | Description |
|---|---|---|---|
| 0x00 | 4 | SAMPLE_ADDR | Pointer to sample data in main RAM |
| 0x04 | 4 | SAMPLE_LEN | Length in frames (low 24 bits) |
| 0x08 | 4 | LOOP_START | Loop point in frames (low 24 bits) |
| 0x0C | 4 | LOOP_LEN | Loop length in frames (low 24 bits) |
| 0x10 | 2 | FREQUENCY | Playback rate, 8.8 fixed-point |
| 0x12 | 1 | VOLUME_L | Left volume (0–255) |
| 0x13 | 1 | VOLUME_R | Right volume (0–255) |
| 0x14 | 1 | CONTROL | Enable, loop, format, key-on |
| 0x15 | 1 | STATUS | Playing, loop hit, end hit (sticky) |
| 0x16–0x1F | 10 | *(reserved)* | Padding to 32-byte boundary |

The CONTROL register packs several fields into one byte:

| Bit | Name | Description |
|---|---|---|
| 0 | ENABLE | Channel active |
| 1 | LOOP | Loop at end of sample |
| 2–3 | FORMAT | 0 = PCM16, 1 = PCM8, 2 = ADPCM |
| 7 | KEY_ON | Trigger playback (auto-clears) |

Writing KEY_ON resets the channel's position to zero, clears sticky status bits, and resets ADPCM decoder state. The bit auto-clears after triggering — the guest writes `ENABLE | LOOP | KEY_ON` in a single byte write and the channel starts immediately.

The STATUS register uses clear-on-read semantics: reading STATUS returns the current bits and clears the sticky flags (LOOP_HIT, END_HIT). This matches how real hardware status registers work — the game polls status in a loop or interrupt handler, and each read acknowledges the events.

### Global Registers

| Offset | Size | Name | Description |
|---|---|---|---|
| 0x200 | 1 | MASTER_VOL_L | Master left volume (0–255) |
| 0x201 | 1 | MASTER_VOL_R | Master right volume (0–255) |
| 0x202 | 2 | IRQ_STATUS | One bit per channel (clear on read) |
| 0x204 | 2 | IRQ_ENABLE | One bit per channel |
| 0x206 | 1 | GLOBAL_CTRL | Bit 0: enable, Bit 1: IRQ enable |

Master volume is applied after channel mixing. At 255/255, the signal passes at unity. Setting master volume to 0 silences all output without touching individual channel state — useful for a game's pause screen or mute toggle.

### Frequency Register

The FREQUENCY register is the most important control for musical playback. It is a 16-bit value in 8.8 fixed-point format: the upper 8 bits are the integer part, the lower 8 bits are the fraction. A value of `0x0100` (256 decimal) means 1.0x playback rate — the mixer advances one sample per output sample at 44.1 kHz.

With a 256-sample single-cycle waveform looped, the base frequency at 1.0x playback is:

```
base_freq = 44100 / 256 = 172.27 Hz
```

To play an arbitrary musical note, scale the frequency register:

```
FREQUENCY = note_hz / 172.27 * 256 = note_hz * 1.486
```

Some precomputed values:

| Note | Frequency | Register |
|---|---|---|
| C4 (middle C) | 261.63 Hz | 0x0185 |
| E4 | 329.63 Hz | 0x01EA |
| G4 | 392.00 Hz | 0x0247 |
| A4 (concert pitch) | 440.00 Hz | 0x028E |
| C5 | 523.25 Hz | 0x030A |

The 8.8 format gives a range from ~0.7 Hz (register 0x0001) to ~11,025 Hz base pitch (register 0xFFFF) — sufficient for any musical purpose and most sound effects. Fractional values enable fine pitch control without audible stepping.

## Sample Formats

### PCM16

Sixteen-bit signed samples stored big-endian in RAM. This is the highest quality format — CD-quality audio with no decoding overhead. Each sample occupies 2 bytes, so a one-second mono clip at 44.1 kHz is 88,200 bytes.

```c
static int16_t
ram_rd16s(const uint8_t *ram, uint32_t addr)
{
    return (int16_t)((ram[addr] << 8) | ram[addr + 1]);
}
```

### PCM8

Eight-bit signed samples, scaled to the 16-bit range by left-shifting 8 bits. Half the memory of PCM16 with an 8-bit dynamic range. Good enough for explosions, ambient noise, and UI sounds — anything where dynamic range matters less than channel count.

```c
static int16_t
ram_rd8s(const uint8_t *ram, uint32_t addr)
{
    return (int16_t)((int8_t)ram[addr]) << 8;
}
```

### IMA ADPCM

Four-bit adaptive differential PCM — the IMA/DVI standard used by the PlayStation SPU, Windows WAV files, and dozens of other audio systems. Each sample compresses to 4 bits (one nibble), giving a 4:1 compression ratio over PCM16. Audio quality is surprisingly good for the bitrate — speech, music, and sound effects all survive ADPCM compression without obvious artifacts.

The decoder maintains two pieces of state per channel: a predictor (the current sample value) and a step index into the 89-entry step table. Each nibble encodes a signed delta relative to the current predictor, using the current step size to scale the delta:

```c
static int16_t
adpcm_decode_nibble(triton_audio_chan *ch, uint8_t nibble)
{
    int step = ima_step_table[ch->adpcm_index];
    int diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) diff = -diff;

    int pred = ch->adpcm_pred + diff;
    if (pred > 32767)  pred = 32767;
    if (pred < -32768) pred = -32768;
    ch->adpcm_pred = (int16_t)pred;

    ch->adpcm_index += ima_index_table[nibble & 0xF];
    if (ch->adpcm_index < 0)  ch->adpcm_index = 0;
    if (ch->adpcm_index > 88) ch->adpcm_index = 88;

    return ch->adpcm_pred;
}
```

The bit-twiddling looks unusual but is the standard IMA algorithm: each of the lower 3 bits of the nibble conditionally adds a power-of-two fraction of the step size, and bit 3 controls sign. The step table and index table are the same 89-entry and 16-entry arrays used by every IMA ADPCM implementation since the standard was published in 1992.

ADPCM has one critical constraint: **decoding is sequential.** Each nibble depends on the previous predictor and step index. When the frequency register causes the mixer to skip ahead (at playback rates above 1.0x), all intermediate nibbles must still be decoded to maintain correct state:

```c
for (uint32_t s = 0; s < steps; s++) {
    if (fmt == AUDIO_FMT_ADPCM) {
        uint8_t nib = adpcm_fetch_nibble(au->ram, sample_addr,
                                         ch->position);
        adpcm_decode_nibble(ch, nib);
    }
    ch->position++;
}
```

Skipping nibbles would corrupt the predictor, producing loud pops and garbage audio. This means ADPCM channels at high playback rates cost more CPU time than PCM channels — a tradeoff that game programmers learn to manage by reserving ADPCM for speech and music (which play near 1.0x) and using PCM for pitched sound effects.

## The Mixing Algorithm

The core of the mixer is a nested loop: for each output frame, iterate over all 16 channels, fetch and scale each active channel's sample, accumulate into stereo int32 accumulators, apply master volume, clip, and write:

```c
for (int f = 0; f < nframes; f++) {
    int32_t left_acc = 0, right_acc = 0;

    for (int c = 0; c < AUDIO_NUM_CHANNELS; c++) {
        /* skip inactive channels */
        int16_t sample = fetch_sample(au, c);

        left_acc  += ((int32_t)sample * vol_l) >> 8;
        right_acc += ((int32_t)sample * vol_r) >> 8;

        /* advance position by frequency register */
    }

    int32_t left  = (left_acc * master_l) >> 8;
    int32_t right = (right_acc * master_r) >> 8;

    /* clip to int16 range */
    if (out) {
        out[f * 2]     = (int16_t)left;
        out[f * 2 + 1] = (int16_t)right;
    }
}
```

The volume calculation uses integer arithmetic throughout — no floating-point in the audio path. Per-channel volume divides by 256 (`>> 8`), master volume divides by 256 again. At maximum volume (255), this preserves 99.6% of the signal amplitude. With 16 channels at full volume, the accumulator can exceed the int16 range — clipping is intentional and expected. Game programmers manage headroom by setting channel volumes appropriately, just as they would on real hardware.

The `if (out)` check enables headless mode: when `out` is NULL, the mixer still advances channel positions, triggers loop/end events, and sets IRQ pending bits, but discards the audio output. This means the emulator's behavior is identical whether or not SDL3 is available — guest programs that depend on audio interrupts for timing work correctly in headless mode.

Nearest-sample interpolation (no linear interpolation) is a deliberate choice. Real late-90s audio hardware did not interpolate between samples — the PlayStation SPU uses Gaussian interpolation only because its sound RAM is separate from the CPU bus, and the filter was built into the DAC path. The Triton's DMA-from-main-RAM architecture has no room for an interpolation filter in the read path. The result is a slightly grittier sound that is period-correct.

## Pitch Control

The frequency register drives a fractional position accumulator. Each output sample, the mixer adds the frequency register value to an 8-bit fractional counter. When the fraction overflows 256, the integer position advances:

```c
uint16_t freq = reg_rd16(au->regs, base + ACHAN_FREQUENCY);
ch->frac += freq;
uint32_t steps = ch->frac >> 8;
ch->frac &= 0xFF;

if (steps > 0)
    advance_channel(au, c, steps);
```

At `FREQUENCY = 0x0100` (1.0x), the position advances exactly one sample per output sample. At `0x0200` (2.0x), it advances two samples per output — doubling the pitch. At `0x0080` (0.5x), it advances one sample every other output — halving the pitch.

This scheme is identical to how the Amiga's Paula chip handles sample rates, and how the PlayStation SPU implements pitch shifting. The fractional accumulator provides glitch-free frequency changes — the guest can update the frequency register mid-playback without pops or discontinuities, enabling vibrato, pitch bends, and Doppler effects.

## SDL3 Audio Integration

### The Callback

SDL3 audio uses a push model: a callback fires from a dedicated audio thread whenever the device's buffer needs more data. The callback receives a byte count, allocates a buffer, mixes into it, and pushes the result to the audio stream:

```c
static void SDLCALL
audio_sdl_callback(void *userdata, SDL_AudioStream *stream,
                   int additional_amount, int total_amount)
{
    triton_sys *s = userdata;
    int nframes = additional_amount / (int)(2 * sizeof(int16_t));

    int16_t stack_buf[2048];
    int16_t *buf = (nframes <= 1024) ? stack_buf
                                     : (int16_t *)malloc((size_t)nframes * 4);

    triton_audio_mix(s->audio, buf, nframes);
    SDL_PutAudioStreamData(stream, buf, nframes * (int)(2 * sizeof(int16_t)));

    if (buf != stack_buf)
        free(buf);
}
```

The stack buffer handles typical requests (≤1024 frames ≈ 23 ms) without heap allocation. SDL3's audio subsystem typically requests 512–2048 frames per callback, so the stack path covers the common case.

### Thread Safety

The SDL3 audio callback runs on a separate thread. The CPU thread writes channel registers; the audio thread reads them for mixing. We deliberately avoid a mutex:

- **Byte-level writes are atomic** on all targets SDL3 supports.
- **KEY_ON is always the last write** — the guest configures SAMPLE_ADDR, SAMPLE_LEN, FREQUENCY, and VOLUME before writing CONTROL with KEY_ON set. By the time the mixer sees KEY_ON, all configuration registers are stable.
- **Worst case is a torn multi-byte read** — the mixer might read a partially-updated frequency or volume for one output frame. At 44.1 kHz, a single frame is 22 microseconds. A torn read produces an imperceptible glitch, not a crash.

This is the same approach used by real hardware: the mixer circuit reads register latches on every sample clock, and if the CPU happens to be mid-write, the mixer gets whatever bits are stable. No latch. No handshake. No FIFO.

### Interrupt Delivery

Audio interrupts cannot fire from the audio callback — the ColdFire exception mechanism modifies CPU state, which is not safe to touch from a non-CPU thread. Instead, the mixer sets pending bits:

```c
/* In the mixer (audio thread): */
au->irq_pending |= (1u << ch_idx);

/* In the main loop (CPU thread): */
void triton_audio_check_irq(triton_audio *au)
{
    uint16_t fired = au->irq_pending & enable;
    if (fired && au->raise_irq)
        au->raise_irq(au->irq_ctx);
    au->irq_pending &= ~fired;
}
```

The main loop calls `triton_audio_check_irq()` after each batch of CPU instructions. This adds at most one frame of latency (16.6 ms at 60 Hz) between the audio event and the interrupt — indistinguishable from real hardware interrupt latency.

The interrupt vector is autovector 4 (IPL 5), delivered via `cf_exception(&cpu, CF_VEC_AUTOVECTOR(4))` — vector 29 in the ColdFire vector table. A game's audio interrupt handler would typically refill sample buffers, advance a music sequencer, or trigger the next note in a sound effect chain.

## Bus Integration

Wiring the audio mixer into the system emulator follows the same pattern as every other peripheral in `triton.c`. Six bus functions (read8/16/32, write8/16/32) each get an address-range check that routes to the audio register interface:

```c
if (addr >= TRITON_AUDIO_BASE && addr < TRITON_AUDIO_END)
    return triton_audio_read(sys->audio, addr - TRITON_AUDIO_BASE);
```

For multi-byte reads and writes, each byte is routed individually — the register file is byte-addressable, just like the GPU register file from Part 3. A 32-bit write to SAMPLE_ADDR becomes four sequential byte writes to offsets 0x00 through 0x03, which the register file stores as big-endian bytes.

In `main()`, the audio state is initialized alongside the GPU:

```c
static triton_audio audio_state;
sys.audio = &audio_state;
triton_audio_init(sys.audio, sys.ram, TRITON_RAM_SIZE,
                  audio_irq_callback, &sys);
```

The `ram` pointer gives the mixer direct access to main RAM for DMA sample reads. No copying, no intermediate buffers — the mixer reads sample bytes directly from the same RAM array that the CPU writes to.

## Linking with Libgcc

Part 3's cube demo included a 25-line `fp_mul` function that manually decomposed a 32-bit multiply into 16-bit halves to avoid the `__muldi3` symbol — a 64-bit multiply helper that lives in libgcc, which is not available in freestanding builds by default.

The fix is simple: add `-lgcc` to the link command *after* the object files. Link order matters — the linker resolves symbols left-to-right, so `-lgcc` must appear after the objects that reference it:

```makefile
M68K_LDSCRIPT = -T app_link.ld
M68K_LDLIBS   = -lgcc

%.elf: %.c common.o
    $(M68K_CC) $(M68K_CFLAGS) $(M68K_LDSCRIPT) -o $@ $< common.o $(M68K_LDLIBS)
```

With libgcc linked, the 25-line `fp_mul` collapses to one:

```c
static int
fp_mul(int a, int b)
{
    return (int)((long long)a * b >> FP_SHIFT);
}
```

This is not just cleaner — it is more correct. The manual decomposition truncated intermediate results, producing rounding errors in extreme cases. The 64-bit multiply is exact.

The common library (`common.c`) also gains `memcpy`, `memset`, `memcmp`, `strlen`, `put_int`, and `put_hex` — utility functions that were either missing or duplicated across guest programs. With libgcc providing the compiler builtins and `common.c` providing the standard library subset, guest programs can focus on application logic instead of reimplementing infrastructure.

## The Sound Demo

The sound demo generates waveforms in RAM and plays a C major arpeggio through four mixer channels, demonstrating looping, stereo panning, format mixing (PCM16 sine + PCM8 square), and visual feedback via the framebuffer.

### Waveform Generation

Two single-cycle waveforms are generated at runtime and written to upper RAM:

```c
#define SINE_ADDR       0x00700000
#define SINE_LEN        256

#define SQUARE_ADDR     0x00700400
#define SQUARE_LEN      256

/* Copy sine table (Q15, ±32767) to RAM as PCM16 samples */
volatile short *sine = (volatile short *)SINE_ADDR;
for (i = 0; i < SINE_LEN; i++)
    sine[i] = sin_table[i];

/* Generate square wave as PCM8 samples */
volatile signed char *square = (volatile signed char *)SQUARE_ADDR;
for (i = 0; i < SQUARE_LEN; i++)
    square[i] = (i < SQUARE_LEN / 2) ? 100 : -100;
```

The sine table is the same 256-entry Q15 lookup table from the cube demo. Stored in the guest program's `.rodata` section, copied to a RAM address that the audio DMA can reach. The square wave demonstrates PCM8 format — 8-bit signed samples at a modest amplitude to keep its contribution to the mix under control.

### Channel Setup

Each channel is configured with a helper function that writes all registers and triggers KEY_ON in a single sequence:

```c
static void
setup_channel(int ch, unsigned int sample_addr, unsigned int sample_len,
              unsigned short freq, unsigned char vol_l, unsigned char vol_r,
              unsigned char fmt)
{
    int base = CH(ch);
    audio_wr32(base + ACHAN_SAMPLE_ADDR, sample_addr);
    audio_wr32(base + ACHAN_SAMPLE_LEN,  sample_len);
    audio_wr32(base + ACHAN_LOOP_START,  0);
    audio_wr32(base + ACHAN_LOOP_LEN,    sample_len);
    audio_wr16(base + ACHAN_FREQUENCY,   freq);
    audio_wr8(base + ACHAN_VOLUME_L,     vol_l);
    audio_wr8(base + ACHAN_VOLUME_R,     vol_r);
    audio_wr8(base + ACHAN_CONTROL, fmt | CTRL_ENABLE | CTRL_LOOP | CTRL_KEY_ON);
}
```

The write order matters: SAMPLE_ADDR and SAMPLE_LEN must be set before CONTROL, because KEY_ON (in the CONTROL write) resets the position counter and starts playback immediately. If the guest wrote KEY_ON before SAMPLE_ADDR, the mixer would start reading from address 0x00000000 — the first bytes of guest RAM, which are the exception vector table. The resulting audio would be... memorable.

### The Arpeggio

The demo builds a chord one note at a time, with delay loops between each addition:

```c
/* C4 alone, panned left */
setup_channel(0, SINE_ADDR, SINE_LEN, NOTE_C4, 96, 32, CTRL_FMT_PCM16);
delay(BEAT * 4);

/* Add E4, panned center-left */
setup_channel(1, SINE_ADDR, SINE_LEN, NOTE_E4, 72, 56, CTRL_FMT_PCM16);
delay(BEAT * 4);

/* Add G4, panned center-right */
setup_channel(2, SINE_ADDR, SINE_LEN, NOTE_G4, 56, 72, CTRL_FMT_PCM16);
delay(BEAT * 4);

/* Add C5 square wave, panned right */
setup_channel(3, SQUARE_ADDR, SQUARE_LEN, NOTE_C5, 32, 96, CTRL_FMT_PCM8);
delay(BEAT * 8);
```

The per-channel volumes are set so the combined mix stays under 0 dBFS — with four simultaneous channels, each runs at roughly one-quarter to one-third of maximum to avoid clipping. The stereo panning sweeps from left (channel 0) to right (channel 3), giving the chord a spatial spread.

After the full chord holds, the demo fades out by stopping channels one at a time in reverse order: drop the C5 square, then G4, E4, and finally C4. Each removal is audible as the sound field narrows and the chord dissolves.

### Results

<video controls width="640" height="480">
<source src="videos/sound_demo.mp4" type="video/mp4">
</video>

```
$ ./triton-headless examples/sound.elf
triton: loaded 12336 bytes from examples/sound.elf at 0x00001000
triton: CPU reset, PC=0x01200400 SP=0x00800000
sound: Triton audio demo
sound: waveforms generated at 0x00700000 (sine) and 0x00700400 (square)
sound: audio engine enabled
sound: beat 1 - C4
sound: beat 2 - C4 + E4
sound: beat 3 - C4 + E4 + G4
sound: beat 4 - full chord + C5 square
sound: fade out
sound: demo complete
triton: halted after 518400863 instructions
```

518 million ColdFire instructions — almost entirely busy-wait delay loops. The actual audio setup is a few hundred instructions per channel. In a real game, those delay loops would be gameplay logic, physics, AI, and rendering. The audio mixer runs concurrently on the SDL3 thread, consuming zero CPU time from the guest's perspective.

## Testing

The test suite covers 18 cases with 36 assertions:

| Test | Validates |
|---|---|
| `test_silent_output` | Zero output with no active channels |
| `test_global_disable` | GLOBAL_CTRL bit 0 silences all output |
| `test_pcm16_playback` | PCM16 sample reads from RAM |
| `test_pcm8_playback` | PCM8 scaling (8-bit → 16-bit) |
| `test_volume_scaling` | Per-channel L/R volume attenuation |
| `test_master_volume` | Global volume scaling |
| `test_frequency_2x` | Double-speed pitch (0x0200) |
| `test_frequency_half` | Half-speed pitch (0x0080) |
| `test_end_of_sample` | PLAYING clears, END_HIT sets |
| `test_loop` | Position wraps, LOOP_HIT sets |
| `test_key_on` | Position reset, status clear |
| `test_status_clear_on_read` | Sticky bits clear after read |
| `test_irq_pending` | Pending bits set on loop/end |
| `test_multichannel_mix` | Two channels sum correctly |
| `test_clipping` | Accumulator clips to ±32767 |
| `test_null_output` | State advances with NULL buffer |
| `test_adpcm_playback` | ADPCM decode produces non-zero output |
| `test_key_on_adpcm_reset` | KEY_ON clears ADPCM decoder state |

```
$ make test
./test_glide
=== Glide Rasterizer Test Suite ===
=== Results: 60/60 passed ===
./test_audio
=== Triton Audio Mixer Test Suite ===
=== Results: 36/36 passed ===
```

All 96 assertions pass. Valgrind reports zero leaks, zero errors on both test suites and the headless emulator.

## Conclusion

The audio mixer adds 479 lines of host-side code (361 mixer + 118 header) to the Triton system emulator. The guest-side sound demo is 365 lines. The expanded common library adds 119 lines. Together with the libgcc integration and Makefile changes, Part 4 contributes approximately 1,000 lines of new code — smaller than the CPU emulator (2,221 lines) or the rasterizer (1,541 lines), reflecting the relative simplicity of a sample-playback mixer compared to a pixel pipeline.

What the mixer implements: 16 channels of DMA-from-RAM sample playback, three audio formats, per-channel pitch control and stereo panning, loop points with sticky status bits, clear-on-read semantics, and interrupt generation. SDL3 audio output with a callback-based push model. Headless state advancement for testing and CI.

What it intentionally omits: hardware reverb (the PS1 SPU's reverb unit is a fascinating piece of DSP engineering, but it adds hundreds of lines of delay-line management for a feature most games barely used), hardware ADPCM *encoding* (encoding is an offline tool, not a runtime operation), and linear interpolation (period-correct nearest-sample playback).

The libgcc bonus collapses the cube demo's manual fixed-point workaround from 25 lines to 1 and provides a common library that future guest programs can build on — `memcpy`, `memset`, integer formatting, and the compiler builtins that freestanding C needs but does not include by default.

The system now has a CPU, a memory bus, a monitor ROM, a GPU rasterizer, and an audio mixer. What it lacks — and what Part 5 will address — is storage. The monitor ROM loads programs from a staging area in RAM because there is no disk driver. The SCSI controller at `0x01110000` returns zeros. The MMC card slot at `0x01120000` does nothing. A console that cannot read a disc is a console that cannot ship a game.

Vertex Technologies would have called the audio mixer their most trouble-free subsystem. It taped out on schedule, passed verification in one spin, and never needed a silicon revision. The sound chip worked perfectly. The problem was never the hardware.

### Sources

- IMA ADPCM Reference Algorithm, Interactive Multimedia Association, 1992
- *PlayStation SPU Technical Reference*, Sony Computer Entertainment, 1996
- Rodrigo Copetti, [Console architecture analyses](https://www.copetti.org/writings/consoles/)
- SDL3 Audio Stream API documentation, libsdl.org
