/* triton_audio.c : Triton 16-channel PCM audio mixer */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#include "triton_audio.h"

#include <string.h>

/* ---- IMA ADPCM tables -------------------------------------------------- */

static const int16_t ima_step_table[89] = {
        7,     8,     9,    10,    11,    12,    13,    14,
       16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66,
       73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
     3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
     7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

/* ---- Helpers ----------------------------------------------------------- */

/** Read big-endian 16-bit from register file. */
static uint16_t
reg_rd16(const uint8_t *regs, uint32_t off)
{
    return (uint16_t)((regs[off] << 8) | regs[off + 1]);
}

/** Read big-endian 32-bit from register file. */
static uint32_t
reg_rd32(const uint8_t *regs, uint32_t off)
{
    return ((uint32_t)regs[off] << 24) | ((uint32_t)regs[off + 1] << 16) |
           ((uint32_t)regs[off + 2] << 8) | regs[off + 3];
}

/** Write big-endian 16-bit to register file. */
static void
reg_wr16(uint8_t *regs, uint32_t off, uint16_t val)
{
    regs[off]     = (uint8_t)(val >> 8);
    regs[off + 1] = (uint8_t)(val);
}

/** Read big-endian 16-bit signed sample from RAM. */
static int16_t
ram_rd16s(const uint8_t *ram, uint32_t addr)
{
    return (int16_t)((ram[addr] << 8) | ram[addr + 1]);
}

/** Read 8-bit signed sample from RAM. */
static int16_t
ram_rd8s(const uint8_t *ram, uint32_t addr)
{
    return (int16_t)((int8_t)ram[addr]) << 8;  /* scale to 16-bit range */
}

/* ---- ADPCM decoder ----------------------------------------------------- */

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

/** Fetch one ADPCM nibble from RAM at nibble position pos. */
static uint8_t
adpcm_fetch_nibble(const uint8_t *ram, uint32_t base_addr, uint32_t pos)
{
    uint32_t byte_addr = base_addr + pos / 2;
    uint8_t byte = ram[byte_addr];
    /* High nibble first (even positions), low nibble second */
    return (pos & 1) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
}

/* ---- Channel sample fetch ---------------------------------------------- */

/**
 * Fetch the current sample value for a channel.
 * For ADPCM, advances the decoder state if needed.
 */
static int16_t
fetch_sample(triton_audio *au, int ch_idx)
{
    triton_audio_chan *ch = &au->chan[ch_idx];
    uint32_t base = (uint32_t)ch_idx * AUDIO_CHAN_SIZE;
    uint32_t sample_addr = reg_rd32(au->regs, base + ACHAN_SAMPLE_ADDR);
    int fmt = (au->regs[base + ACHAN_CONTROL] >> ACHAN_CTRL_FMT_SHIFT) & 3;

    switch (fmt) {
    case AUDIO_FMT_PCM16: {
        uint32_t addr = sample_addr + ch->position * 2;
        if (addr + 1 >= au->ram_size)
            return 0;
        return ram_rd16s(au->ram, addr);
    }
    case AUDIO_FMT_PCM8: {
        uint32_t addr = sample_addr + ch->position;
        if (addr >= au->ram_size)
            return 0;
        return ram_rd8s(au->ram, addr);
    }
    case AUDIO_FMT_ADPCM:
        /* ADPCM predictor is already at the current position */
        return ch->adpcm_pred;
    default:
        return 0;
    }
}

/**
 * Advance a channel's position by 'steps' samples.
 * For ADPCM, decodes intermediate nibbles to maintain predictor state.
 * Returns non-zero if end-of-sample or loop was hit.
 */
static int
advance_channel(triton_audio *au, int ch_idx, uint32_t steps)
{
    triton_audio_chan *ch = &au->chan[ch_idx];
    uint32_t base = (uint32_t)ch_idx * AUDIO_CHAN_SIZE;
    uint32_t sample_len = reg_rd32(au->regs, base + ACHAN_SAMPLE_LEN) & 0x00FFFFFF;
    uint32_t loop_start = reg_rd32(au->regs, base + ACHAN_LOOP_START) & 0x00FFFFFF;
    uint32_t loop_len   = reg_rd32(au->regs, base + ACHAN_LOOP_LEN)  & 0x00FFFFFF;
    uint8_t  ctrl       = au->regs[base + ACHAN_CONTROL];
    int      fmt        = (ctrl >> ACHAN_CTRL_FMT_SHIFT) & 3;
    int      loop_en    = ctrl & ACHAN_CTRL_LOOP;
    int      hit = 0;

    for (uint32_t s = 0; s < steps; s++) {
        /* For ADPCM, decode the next nibble before advancing */
        if (fmt == AUDIO_FMT_ADPCM) {
            uint32_t sample_addr = reg_rd32(au->regs, base + ACHAN_SAMPLE_ADDR);
            uint32_t nib_addr = sample_addr + ch->position / 2;
            if (nib_addr < au->ram_size) {
                uint8_t nib = adpcm_fetch_nibble(au->ram, sample_addr,
                                                 ch->position);
                adpcm_decode_nibble(ch, nib);
            }
        }

        ch->position++;

        if (sample_len > 0 && ch->position >= sample_len) {
            if (loop_en && loop_len > 0) {
                ch->position = loop_start;
                ch->status |= ACHAN_STAT_LOOP_HIT;
                au->irq_pending |= (1u << ch_idx);
                hit = 1;
                /* Note: ADPCM loop doesn't reset decoder state.
                 * For clean loops, samples should be authored accordingly. */
            } else {
                ch->position = sample_len;
                ch->status &= ~ACHAN_STAT_PLAYING;
                ch->status |= ACHAN_STAT_END_HIT;
                au->irq_pending |= (1u << ch_idx);
                hit = 1;
                break;
            }
        }
    }

    return hit;
}

/* ---- Public API -------------------------------------------------------- */

void
triton_audio_init(triton_audio *au, const uint8_t *ram, uint32_t ram_size,
                  void (*raise_irq)(void *), void *irq_ctx)
{
    memset(au, 0, sizeof(*au));
    au->ram = ram;
    au->ram_size = ram_size;
    au->raise_irq = raise_irq;
    au->irq_ctx = irq_ctx;

    /* Default master volume to max */
    au->regs[AUDIO_MASTER_VOL_L] = 255;
    au->regs[AUDIO_MASTER_VOL_R] = 255;
}

uint32_t
triton_audio_read(triton_audio *au, uint32_t offset)
{
    if (offset >= AUDIO_REG_SIZE)
        return 0;

    /* STATUS register: return live status, clear sticky bits on read */
    if (offset >= ACHAN_STATUS && offset < AUDIO_NUM_CHANNELS * AUDIO_CHAN_SIZE) {
        uint32_t within_chan = offset % AUDIO_CHAN_SIZE;
        if (within_chan == ACHAN_STATUS) {
            int ch_idx = (int)(offset / AUDIO_CHAN_SIZE);
            uint8_t val = au->chan[ch_idx].status;
            au->chan[ch_idx].status &= ACHAN_STAT_PLAYING;  /* clear sticky bits */
            return val;
        }
    }

    /* IRQ_STATUS: clear on read */
    if (offset == AUDIO_IRQ_STATUS || offset == AUDIO_IRQ_STATUS + 1) {
        uint8_t val = au->regs[offset];
        au->regs[offset] = 0;
        return val;
    }

    return au->regs[offset];
}

void
triton_audio_write(triton_audio *au, uint32_t offset, uint32_t val)
{
    if (offset >= AUDIO_REG_SIZE)
        return;

    /* STATUS is read-only */
    if (offset < AUDIO_NUM_CHANNELS * AUDIO_CHAN_SIZE) {
        uint32_t within_chan = offset % AUDIO_CHAN_SIZE;
        if (within_chan == ACHAN_STATUS)
            return;
    }

    au->regs[offset] = (uint8_t)val;

    /* Handle KEY_ON trigger */
    if (offset < AUDIO_NUM_CHANNELS * AUDIO_CHAN_SIZE) {
        uint32_t within_chan = offset % AUDIO_CHAN_SIZE;
        if (within_chan == ACHAN_CONTROL && (val & ACHAN_CTRL_KEY_ON)) {
            int ch_idx = (int)(offset / AUDIO_CHAN_SIZE);
            triton_audio_chan *ch = &au->chan[ch_idx];

            /* Reset playback position */
            ch->position = 0;
            ch->frac = 0;

            /* Reset ADPCM state */
            ch->adpcm_pred = 0;
            ch->adpcm_index = 0;

            /* Mark as playing, clear sticky flags */
            ch->status = ACHAN_STAT_PLAYING;

            /* Auto-clear KEY_ON bit */
            au->regs[offset] &= ~ACHAN_CTRL_KEY_ON;
        }
    }
}

void
triton_audio_mix(triton_audio *au, int16_t *out, int nframes)
{
    uint8_t gctrl = au->regs[AUDIO_GLOBAL_CTRL];
    int global_en = gctrl & AUDIO_GCTRL_ENABLE;
    int master_l = au->regs[AUDIO_MASTER_VOL_L];
    int master_r = au->regs[AUDIO_MASTER_VOL_R];

    for (int f = 0; f < nframes; f++) {
        int32_t left_acc = 0;
        int32_t right_acc = 0;

        if (global_en) {
            for (int c = 0; c < AUDIO_NUM_CHANNELS; c++) {
                triton_audio_chan *ch = &au->chan[c];
                if (!(ch->status & ACHAN_STAT_PLAYING))
                    continue;

                uint32_t base = (uint32_t)c * AUDIO_CHAN_SIZE;
                uint8_t ctrl = au->regs[base + ACHAN_CONTROL];
                if (!(ctrl & ACHAN_CTRL_ENABLE))
                    continue;

                int16_t sample = fetch_sample(au, c);
                int vol_l = au->regs[base + ACHAN_VOLUME_L];
                int vol_r = au->regs[base + ACHAN_VOLUME_R];

                left_acc  += ((int32_t)sample * vol_l) >> 8;
                right_acc += ((int32_t)sample * vol_r) >> 8;

                /* Advance position by frequency (8.8 fixed-point) */
                uint16_t freq = reg_rd16(au->regs, base + ACHAN_FREQUENCY);
                ch->frac += freq;
                uint32_t steps = ch->frac >> 8;
                ch->frac &= 0xFF;

                if (steps > 0)
                    advance_channel(au, c, steps);
            }
        }

        /* Apply master volume */
        int32_t left  = (left_acc  * master_l) >> 8;
        int32_t right = (right_acc * master_r) >> 8;

        /* Clip to int16 range */
        if (left > 32767)  left = 32767;
        if (left < -32768) left = -32768;
        if (right > 32767)  right = 32767;
        if (right < -32768) right = -32768;

        if (out) {
            out[f * 2 + 0] = (int16_t)left;
            out[f * 2 + 1] = (int16_t)right;
        }
    }
}

void
triton_audio_check_irq(triton_audio *au)
{
    uint8_t gctrl = au->regs[AUDIO_GLOBAL_CTRL];
    if (!(gctrl & AUDIO_GCTRL_IRQ_EN))
        return;

    uint16_t pending = au->irq_pending;
    if (!pending)
        return;

    uint16_t enable = reg_rd16(au->regs, AUDIO_IRQ_ENABLE);
    uint16_t fired = pending & enable;
    if (!fired)
        return;

    /* Update IRQ_STATUS register */
    uint16_t status = reg_rd16(au->regs, AUDIO_IRQ_STATUS);
    status |= fired;
    reg_wr16(au->regs, AUDIO_IRQ_STATUS, status);

    /* Clear acknowledged pending bits */
    au->irq_pending &= ~fired;

    /* Deliver interrupt */
    if (au->raise_irq)
        au->raise_irq(au->irq_ctx);
}
