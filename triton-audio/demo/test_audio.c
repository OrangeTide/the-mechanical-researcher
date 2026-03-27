/* test_audio.c -- Host-side test suite for the Triton audio mixer.
 *
 * Tests the audio mixer in isolation: no ColdFire CPU needed.
 * Sets up triton_audio directly with fake RAM and verifies output samples.
 *
 * Build:  gcc -Wall -Wextra -O2 -o test_audio test_audio.c triton_audio.c -lm
 * Run:    ./test_audio
 */

#include "triton_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE    (1 * 1024 * 1024)   /* 1 MB is enough for tests */

static uint8_t *ram;
static triton_audio au;

static int tests_run;
static int tests_passed;
static int tests_failed;

/* IRQ tracking */
static int irq_fired;
static void test_irq_callback(void *ctx) { (void)ctx; irq_fired++; }

/* ---- Helpers ----------------------------------------------------------- */

static void
check(const char *name, int cond)
{
    tests_run++;
    if (cond) {
        tests_passed++;
    } else {
        tests_failed++;
        printf("  FAIL: %s\n", name);
    }
}

static void
reset(void)
{
    memset(ram, 0, RAM_SIZE);
    irq_fired = 0;
    triton_audio_init(&au, ram, RAM_SIZE, test_irq_callback, NULL);
}

/** Write a big-endian 32-bit value to RAM. */
static void
ram_wr32(uint32_t addr, uint32_t val)
{
    ram[addr]     = (uint8_t)(val >> 24);
    ram[addr + 1] = (uint8_t)(val >> 16);
    ram[addr + 2] = (uint8_t)(val >> 8);
    ram[addr + 3] = (uint8_t)(val);
}

/** Write a big-endian 16-bit signed sample to RAM. */
static void
ram_wr16s(uint32_t addr, int16_t val)
{
    ram[addr]     = (uint8_t)((uint16_t)val >> 8);
    ram[addr + 1] = (uint8_t)((uint16_t)val);
}

/** Write an 8-bit signed sample to RAM. */
static void
ram_wr8s(uint32_t addr, int8_t val)
{
    ram[addr] = (uint8_t)val;
}

/** Write a register byte via the audio write function. */
static void
wreg(uint32_t offset, uint8_t val)
{
    triton_audio_write(&au, offset, val);
}

/** Write a big-endian 16-bit register. */
static void
wreg16(uint32_t offset, uint16_t val)
{
    triton_audio_write(&au, offset, (val >> 8) & 0xFF);
    triton_audio_write(&au, offset + 1, val & 0xFF);
}

/** Write a big-endian 32-bit register. */
static void
wreg32(uint32_t offset, uint32_t val)
{
    triton_audio_write(&au, offset, (val >> 24) & 0xFF);
    triton_audio_write(&au, offset + 1, (val >> 16) & 0xFF);
    triton_audio_write(&au, offset + 2, (val >> 8) & 0xFF);
    triton_audio_write(&au, offset + 3, val & 0xFF);
}

/** Configure channel n with basic settings. */
static void
setup_channel(int ch, uint32_t sample_addr, uint32_t sample_len,
              uint16_t freq, uint8_t vol_l, uint8_t vol_r, int fmt)
{
    uint32_t base = (uint32_t)ch * AUDIO_CHAN_SIZE;
    wreg32(base + ACHAN_SAMPLE_ADDR, sample_addr);
    wreg32(base + ACHAN_SAMPLE_LEN, sample_len);
    wreg32(base + ACHAN_LOOP_START, 0);
    wreg32(base + ACHAN_LOOP_LEN, 0);
    wreg16(base + ACHAN_FREQUENCY, freq);
    wreg(base + ACHAN_VOLUME_L, vol_l);
    wreg(base + ACHAN_VOLUME_R, vol_r);
    /* Set format and enable, then key-on */
    uint8_t ctrl = ACHAN_CTRL_ENABLE | ((fmt & 3) << ACHAN_CTRL_FMT_SHIFT);
    wreg(base + ACHAN_CONTROL, ctrl | ACHAN_CTRL_KEY_ON);
}

/* ---- Tests ------------------------------------------------------------- */

static void
test_silent_output(void)
{
    printf("test_silent_output\n");
    reset();

    /* Enable global, but no channels active */
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    int16_t buf[128];
    memset(buf, 0xAA, sizeof(buf));
    triton_audio_mix(&au, buf, 64);

    int silent = 1;
    for (int i = 0; i < 128; i++) {
        if (buf[i] != 0) { silent = 0; break; }
    }
    check("no channels active: output is silence", silent);
}

static void
test_global_disable(void)
{
    printf("test_global_disable\n");
    reset();

    /* Set up a channel but DON'T set global enable */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);
    /* Global ctrl left at 0 (disabled) */

    int16_t buf[64];
    triton_audio_mix(&au, buf, 32);

    check("global disabled: output is zero", buf[0] == 0 && buf[1] == 0);
}

static void
test_pcm16_playback(void)
{
    printf("test_pcm16_playback\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* Write a constant PCM16 sample: 16384 (half max) */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 16384);

    /* Channel 0: 1x rate, full volume both channels */
    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 4);

    /* vol=255: output = (16384 * 255) >> 8 = 16320 (before master vol) */
    /* master=255: output = (16320 * 255) >> 8 = 16256 */
    check("pcm16: left channel > 15000", buf[0] > 15000);
    check("pcm16: right channel > 15000", buf[1] > 15000);
    check("pcm16: left ~ right", abs(buf[0] - buf[1]) < 100);
}

static void
test_pcm8_playback(void)
{
    printf("test_pcm8_playback\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* Write constant PCM8 samples: 64 (half of 127) */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr8s(addr + (uint32_t)i, 64);

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM8);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 4);

    /* PCM8: 64 << 8 = 16384, then * 255/256 * 255/256 ≈ 16256 */
    check("pcm8: output > 15000", buf[0] > 15000);
}

static void
test_volume_scaling(void)
{
    printf("test_volume_scaling\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 32000);

    /* Full left, zero right */
    setup_channel(0, addr, 100, 0x0100, 255, 0, AUDIO_FMT_PCM16);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 4);

    check("vol: left channel > 30000", buf[0] > 30000);
    check("vol: right channel == 0", buf[1] == 0);
}

static void
test_master_volume(void)
{
    printf("test_master_volume\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 32000);

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    /* Set master volume to ~half */
    wreg(AUDIO_MASTER_VOL_L, 128);
    wreg(AUDIO_MASTER_VOL_R, 128);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 4);

    /* Full volume would give ~31750. Half master ≈ ~15875. */
    check("master vol: output between 14000-17000",
          buf[0] > 14000 && buf[0] < 17000);
}

static void
test_frequency_2x(void)
{
    printf("test_frequency_2x\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* Alternating samples: 20000, -20000, 20000, -20000, ... */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, (i % 2 == 0) ? 20000 : -20000);

    /* 2x frequency: should consume 2 samples per output frame */
    setup_channel(0, addr, 100, 0x0200, 255, 255, AUDIO_FMT_PCM16);

    /* Mix 8 frames, then check channel position advanced by ~16 */
    int16_t buf[32];
    triton_audio_mix(&au, buf, 8);

    /* Position should be around 16 (8 frames × 2 samples/frame) */
    check("2x freq: position advanced correctly", au.chan[0].position >= 14);
}

static void
test_frequency_half(void)
{
    printf("test_frequency_half\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* Constant 10000 */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    /* 0.5x frequency */
    setup_channel(0, addr, 100, 0x0080, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[128];
    triton_audio_mix(&au, buf, 16);

    /* Position should be ~8 (16 frames × 0.5 samples/frame) */
    check("half freq: position ~8", au.chan[0].position >= 6 && au.chan[0].position <= 10);
}

static void
test_end_of_sample(void)
{
    printf("test_end_of_sample\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 10; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    /* 10-sample buffer, 1x rate */
    setup_channel(0, addr, 10, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[128];
    triton_audio_mix(&au, buf, 20);

    /* Should have stopped playing and set END_HIT */
    check("end: not playing", !(au.chan[0].status & ACHAN_STAT_PLAYING));
    check("end: END_HIT set", au.chan[0].status & ACHAN_STAT_END_HIT);

    /* Output after end should be zero */
    check("end: output silent after end", buf[30] == 0 && buf[31] == 0);
}

static void
test_loop(void)
{
    printf("test_loop\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 20; i++)
        ram_wr16s(addr + (uint32_t)i * 2, (int16_t)(1000 * (i + 1)));

    /* 20-sample buffer, loop from sample 5 with length 10 */
    uint32_t base = 0 * AUDIO_CHAN_SIZE;
    wreg32(base + ACHAN_SAMPLE_ADDR, addr);
    wreg32(base + ACHAN_SAMPLE_LEN, 20);
    wreg32(base + ACHAN_LOOP_START, 5);
    wreg32(base + ACHAN_LOOP_LEN, 10);
    wreg16(base + ACHAN_FREQUENCY, 0x0100);
    wreg(base + ACHAN_VOLUME_L, 255);
    wreg(base + ACHAN_VOLUME_R, 255);
    wreg(base + ACHAN_CONTROL,
         ACHAN_CTRL_ENABLE | ACHAN_CTRL_LOOP | ACHAN_CTRL_KEY_ON);

    int16_t buf[128];
    triton_audio_mix(&au, buf, 30);

    /* After playing 20 samples it should loop back to 5, still playing */
    check("loop: still playing", au.chan[0].status & ACHAN_STAT_PLAYING);
    check("loop: LOOP_HIT set", au.chan[0].status & ACHAN_STAT_LOOP_HIT);
}

static void
test_key_on(void)
{
    printf("test_key_on\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    /* Advance some */
    int16_t buf[64];
    triton_audio_mix(&au, buf, 20);
    check("key_on: position > 0 before re-trigger", au.chan[0].position > 0);

    /* Re-trigger with KEY_ON */
    wreg(0 * AUDIO_CHAN_SIZE + ACHAN_CONTROL,
         ACHAN_CTRL_ENABLE | ACHAN_CTRL_KEY_ON);

    check("key_on: position reset to 0", au.chan[0].position == 0);
    check("key_on: frac reset to 0", au.chan[0].frac == 0);
    check("key_on: KEY_ON bit auto-cleared",
          !(au.regs[ACHAN_CONTROL] & ACHAN_CTRL_KEY_ON));
}

static void
test_status_clear_on_read(void)
{
    printf("test_status_clear_on_read\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 5; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    setup_channel(0, addr, 5, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 10);

    /* END_HIT should be set */
    uint32_t status1 = triton_audio_read(&au, ACHAN_STATUS);
    check("status: END_HIT in first read", status1 & ACHAN_STAT_END_HIT);

    /* Second read should have sticky bits cleared */
    uint32_t status2 = triton_audio_read(&au, ACHAN_STATUS);
    check("status: END_HIT cleared after read", !(status2 & ACHAN_STAT_END_HIT));
}

static void
test_irq_pending(void)
{
    printf("test_irq_pending\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE | AUDIO_GCTRL_IRQ_EN);
    wreg16(AUDIO_IRQ_ENABLE, 0x0001);  /* enable IRQ for channel 0 */

    uint32_t addr = 0x10000;
    for (int i = 0; i < 5; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    setup_channel(0, addr, 5, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 10);

    check("irq: pending set", au.irq_pending & 1);

    triton_audio_check_irq(&au);
    check("irq: callback fired", irq_fired == 1);
    check("irq: pending cleared after check", !(au.irq_pending & 1));
}

static void
test_multichannel_mix(void)
{
    printf("test_multichannel_mix\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* Channel 0: constant 10000, channel 1: constant 5000 */
    uint32_t addr0 = 0x10000, addr1 = 0x20000;
    for (int i = 0; i < 100; i++) {
        ram_wr16s(addr0 + (uint32_t)i * 2, 10000);
        ram_wr16s(addr1 + (uint32_t)i * 2, 5000);
    }

    setup_channel(0, addr0, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);
    setup_channel(1, addr1, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 4);

    /* Sum should be approximately (10000 + 5000) * (255/256)^2 ≈ 14883 */
    check("multichan: combined output > 14000", buf[0] > 14000);
    check("multichan: combined output < 16000", buf[0] < 16000);
}

static void
test_clipping(void)
{
    printf("test_clipping\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* 16 channels all playing 32000 at max volume — will overflow */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 32000);

    for (int c = 0; c < 16; c++)
        setup_channel(c, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 4);

    /* Should clip to 32767, not wrap around negative */
    check("clip: output == 32767", buf[0] == 32767);
    check("clip: no negative wrap", buf[0] > 0);
}

static void
test_null_output(void)
{
    printf("test_null_output (headless advance)\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 100; i++)
        ram_wr16s(addr + (uint32_t)i * 2, 10000);

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_PCM16);

    /* Mix with NULL output — should still advance position */
    triton_audio_mix(&au, NULL, 20);

    check("null output: position advanced", au.chan[0].position >= 18);
}

static void
test_adpcm_playback(void)
{
    printf("test_adpcm_playback\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    /* Encode a simple ramp: nibbles that step upward.
     * Nibble 4 = +step, should produce increasing predictor values.
     * Pack nibbles: high nibble first, low nibble second per byte. */
    uint32_t addr = 0x10000;
    for (int i = 0; i < 50; i++) {
        /* Both nibbles = 4 (positive, magnitude 4 → diff += step) */
        ram[addr + (uint32_t)i] = 0x44;
    }

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_ADPCM);

    int16_t buf[128];
    triton_audio_mix(&au, buf, 20);

    /* After decoding ascending nibbles, output should be positive and increasing */
    check("adpcm: first sample is non-negative", buf[0] >= 0);
    /* After several steps the predictor should have grown */
    check("adpcm: output grows", buf[30] > buf[0] || buf[20] > buf[0]);
}

static void
test_key_on_adpcm_reset(void)
{
    printf("test_key_on_adpcm_reset\n");
    reset();
    wreg(AUDIO_GLOBAL_CTRL, AUDIO_GCTRL_ENABLE);

    uint32_t addr = 0x10000;
    for (int i = 0; i < 50; i++)
        ram[addr + (uint32_t)i] = 0x77;  /* large magnitude nibbles */

    setup_channel(0, addr, 100, 0x0100, 255, 255, AUDIO_FMT_ADPCM);

    int16_t buf[64];
    triton_audio_mix(&au, buf, 20);

    /* ADPCM state should be non-zero now */
    check("adpcm: predictor non-zero before reset",
          au.chan[0].adpcm_pred != 0 || au.chan[0].adpcm_index != 0);

    /* Re-trigger with KEY_ON */
    wreg(0 * AUDIO_CHAN_SIZE + ACHAN_CONTROL,
         ACHAN_CTRL_ENABLE | (AUDIO_FMT_ADPCM << ACHAN_CTRL_FMT_SHIFT) |
         ACHAN_CTRL_KEY_ON);

    check("adpcm reset: predictor = 0", au.chan[0].adpcm_pred == 0);
    check("adpcm reset: index = 0", au.chan[0].adpcm_index == 0);
    check("adpcm reset: position = 0", au.chan[0].position == 0);
}

/* ---- Main -------------------------------------------------------------- */

int
main(void)
{
    ram = calloc(1, RAM_SIZE);
    if (!ram) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    printf("=== Triton Audio Mixer Test Suite ===\n\n");

    test_silent_output();
    test_global_disable();
    test_pcm16_playback();
    test_pcm8_playback();
    test_volume_scaling();
    test_master_volume();
    test_frequency_2x();
    test_frequency_half();
    test_end_of_sample();
    test_loop();
    test_key_on();
    test_status_clear_on_read();
    test_irq_pending();
    test_multichannel_mix();
    test_clipping();
    test_null_output();
    test_adpcm_playback();
    test_key_on_adpcm_reset();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    free(ram);
    return tests_failed > 0 ? 1 : 0;
}
