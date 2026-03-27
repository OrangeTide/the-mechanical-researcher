/*
 * triton_audio.h -- Triton 16-channel PCM audio mixer
 *
 * Memory-mapped register file at 0x01100000 (1 KB).
 * 16 channels × 32 bytes + global registers.
 * Supports PCM16, PCM8, and IMA ADPCM sample formats.
 */

#ifndef TRITON_AUDIO_H
#define TRITON_AUDIO_H

#include <stdint.h>

/* ---- Constants --------------------------------------------------------- */

#define AUDIO_NUM_CHANNELS  16
#define AUDIO_CHAN_SIZE      32      /* bytes per channel register block */
#define AUDIO_REG_SIZE      0x400   /* total register space (1 KB) */
#define AUDIO_SAMPLE_RATE   44100

/* ---- Per-channel register offsets (within 32-byte block) --------------- */

#define ACHAN_SAMPLE_ADDR   0x00    /* 32-bit: pointer to sample in RAM */
#define ACHAN_SAMPLE_LEN    0x04    /* 32-bit: length in frames (low 24) */
#define ACHAN_LOOP_START    0x08    /* 32-bit: loop point (low 24) */
#define ACHAN_LOOP_LEN      0x0C    /* 32-bit: loop length (low 24) */
#define ACHAN_FREQUENCY     0x10    /* 16-bit: 8.8 fixed-point rate */
#define ACHAN_VOLUME_L      0x12    /* 8-bit: left volume 0-255 */
#define ACHAN_VOLUME_R      0x13    /* 8-bit: right volume 0-255 */
#define ACHAN_CONTROL       0x14    /* 8-bit: control flags */
#define ACHAN_STATUS        0x15    /* 8-bit: status (sticky, clear on read) */

/* Channel control bits */
#define ACHAN_CTRL_ENABLE       (1 << 0)
#define ACHAN_CTRL_LOOP         (1 << 1)
#define ACHAN_CTRL_FMT_MASK     (3 << 2)
#define ACHAN_CTRL_FMT_SHIFT    2
#define ACHAN_CTRL_KEY_ON       (1 << 7)

/* Channel status bits */
#define ACHAN_STAT_PLAYING      (1 << 0)
#define ACHAN_STAT_LOOP_HIT     (1 << 1)
#define ACHAN_STAT_END_HIT      (1 << 2)

/* Sample format codes (bits 2-3 of CONTROL) */
#define AUDIO_FMT_PCM16     0
#define AUDIO_FMT_PCM8      1
#define AUDIO_FMT_ADPCM     2

/* ---- Global register offsets ------------------------------------------- */

#define AUDIO_MASTER_VOL_L  0x200
#define AUDIO_MASTER_VOL_R  0x201
#define AUDIO_IRQ_STATUS    0x202   /* 16-bit: one bit per channel */
#define AUDIO_IRQ_ENABLE    0x204   /* 16-bit: one bit per channel */
#define AUDIO_GLOBAL_CTRL   0x206   /* 8-bit */

/* Global control bits */
#define AUDIO_GCTRL_ENABLE  (1 << 0)
#define AUDIO_GCTRL_IRQ_EN  (1 << 1)

/* ---- Per-channel runtime state ----------------------------------------- */

typedef struct {
    uint32_t position;      /* current sample position (integer part) */
    uint32_t frac;          /* fractional accumulator for pitch (8 frac bits) */
    int16_t  adpcm_pred;    /* IMA ADPCM predictor */
    int      adpcm_index;   /* IMA ADPCM step index (0-88) */
    uint8_t  status;        /* live status bits (PLAYING, sticky flags) */
} triton_audio_chan;

/* ---- Audio state ------------------------------------------------------- */

typedef struct triton_audio {
    /* Register file: raw big-endian bytes matching HW layout */
    uint8_t regs[AUDIO_REG_SIZE];

    /* Per-channel runtime state (not register-mapped) */
    triton_audio_chan chan[AUDIO_NUM_CHANNELS];

    /* IRQ pending bits (set by mixer, checked by CPU thread) */
    uint16_t irq_pending;

    /* Back-pointer to system RAM for sample DMA */
    const uint8_t *ram;
    uint32_t ram_size;

    /* Interrupt delivery callback (called from CPU thread only) */
    void (*raise_irq)(void *ctx);
    void *irq_ctx;
} triton_audio;

/* ---- Public API -------------------------------------------------------- */

/** Initialize audio state. Call once at startup. */
void triton_audio_init(triton_audio *au, const uint8_t *ram, uint32_t ram_size,
                       void (*raise_irq)(void *), void *irq_ctx);

/** Read a register byte. Handles STATUS clear-on-read. */
uint32_t triton_audio_read(triton_audio *au, uint32_t offset);

/** Write a register byte. Handles KEY_ON trigger. */
void triton_audio_write(triton_audio *au, uint32_t offset, uint32_t val);

/**
 * Mix nframes of stereo S16 audio into out[].
 * If out is NULL, state advances but output is discarded (headless mode).
 * Called from SDL3 audio thread or main loop.
 */
void triton_audio_mix(triton_audio *au, int16_t *out, int nframes);

/**
 * Check for pending audio interrupts and deliver them.
 * Must be called from the CPU thread (not the audio callback).
 */
void triton_audio_check_irq(triton_audio *au);

#endif /* TRITON_AUDIO_H */
