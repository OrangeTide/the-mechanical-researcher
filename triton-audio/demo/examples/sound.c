/*
 * sound.c -- Audio demo for the Triton console
 *
 * Generates waveforms in RAM and plays a C major chord using the
 * 16-channel PCM audio mixer.  Demonstrates looping, stereo panning,
 * frequency scaling, and visual feedback via framebuffer VU bars.
 * Links with -lgcc for 64-bit multiply support.
 *
 * Build: m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
 *        -T app_link.ld -o sound.elf sound.c common.o -lgcc
 */

#include "common.h"

/* ---- Audio register access --------------------------------------------- */

/*
 * Audio registers are big-endian.  Write multi-byte values MSB first.
 */

static void
audio_wr8(int off, unsigned char val)
{
    AUDIO_BASE[off] = val;
}

static void
audio_wr16(int off, unsigned short val)
{
    AUDIO_BASE[off]     = (unsigned char)(val >> 8);
    AUDIO_BASE[off + 1] = (unsigned char)val;
}

static void
audio_wr32(int off, unsigned int val)
{
    AUDIO_BASE[off]     = (unsigned char)(val >> 24);
    AUDIO_BASE[off + 1] = (unsigned char)(val >> 16);
    AUDIO_BASE[off + 2] = (unsigned char)(val >> 8);
    AUDIO_BASE[off + 3] = (unsigned char)val;
}

static unsigned char
audio_rd8(int off)
{
    return AUDIO_BASE[off];
}

/* ---- Per-channel register offsets (within 32-byte block) --------------- */

#define CH(n)               ((n) * 32)
#define ACHAN_SAMPLE_ADDR   0x00
#define ACHAN_SAMPLE_LEN    0x04
#define ACHAN_LOOP_START    0x08
#define ACHAN_LOOP_LEN      0x0C
#define ACHAN_FREQUENCY     0x10
#define ACHAN_VOLUME_L      0x12
#define ACHAN_VOLUME_R      0x13
#define ACHAN_CONTROL       0x14
#define ACHAN_STATUS        0x15

/* Control bits */
#define CTRL_ENABLE         0x01
#define CTRL_LOOP           0x02
#define CTRL_FMT_PCM16      (0 << 2)
#define CTRL_FMT_PCM8       (1 << 2)
#define CTRL_KEY_ON         0x80

/* Global registers */
#define AUDIO_MASTER_VOL_L  0x200
#define AUDIO_MASTER_VOL_R  0x201
#define AUDIO_GLOBAL_CTRL   0x206
#define GCTRL_ENABLE        0x01

/* ---- Waveform generation ----------------------------------------------- */

/*
 * Sine table: 256 entries, Q15 (±32767).
 * One full cycle.  Used both for generating PCM16 sample data in RAM
 * and for computing note frequencies.
 */
static const short sin_table[256] = {
         0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
      6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
     12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
     18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
     23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
     27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
     30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
     32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
     32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
     32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
     30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
     27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
     23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
     18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
     12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
      6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
         0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
     -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
    -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
     -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

/*
 * Sample data addresses.  These live in the upper region of main RAM,
 * well above where the program itself is loaded (0x00010000+).
 *
 * Sine: 256 samples x 2 bytes = 512 bytes PCM16.
 * Square: 256 samples x 1 byte = 256 bytes PCM8.
 */
#define SINE_ADDR       0x00700000
#define SINE_LEN        256

#define SQUARE_ADDR     0x00700400
#define SQUARE_LEN      256

static void
generate_samples(void)
{
    volatile short *sine = (volatile short *)SINE_ADDR;
    volatile signed char *square = (volatile signed char *)SQUARE_ADDR;
    int i;

    /* Copy sine table to RAM as PCM16 sample data */
    for (i = 0; i < SINE_LEN; i++)
        sine[i] = sin_table[i];

    /* Generate square wave as PCM8 sample data */
    for (i = 0; i < SQUARE_LEN; i++)
        square[i] = (i < SQUARE_LEN / 2) ? 100 : -100;
}

/* ---- Note frequency table ---------------------------------------------- */

/*
 * Frequency register is 8.8 fixed-point.  0x0100 = 1.0x playback rate.
 * With a 256-sample single cycle at 44100 Hz, the base frequency is
 * 44100 / 256 = 172.27 Hz.
 *
 * To play note at F Hz:  register = F / 172.27 * 256 = F * 1.4862
 *
 * Precomputed for one octave of the chromatic scale (C4 through C5):
 */
#define NOTE_C4     389     /* 261.63 Hz */
#define NOTE_D4     437     /* 293.66 Hz */
#define NOTE_E4     490     /* 329.63 Hz */
#define NOTE_F4     519     /* 349.23 Hz */
#define NOTE_G4     583     /* 392.00 Hz */
#define NOTE_A4     654     /* 440.00 Hz */
#define NOTE_B4     734     /* 493.88 Hz */
#define NOTE_C5     778     /* 523.25 Hz */

/* ---- Channel setup helpers --------------------------------------------- */

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

static void
stop_channel(int ch)
{
    audio_wr8(CH(ch) + ACHAN_CONTROL, 0);
}

/* ---- Visual feedback --------------------------------------------------- */

/* RGB565 colors */
#define BG_COLOR    0x1082      /* dark gray */
#define BAR_C       0xF800      /* red */
#define BAR_E       0x07E0      /* green */
#define BAR_G       0x001F      /* blue */
#define BAR_SQ      0xFFE0      /* yellow */
#define BAR_DIM     0x4208      /* dim gray */
#define WHITE       0xFFFF

/*
 * Draw a horizontal bar at (x, y) with width w and height h.
 */
static void
draw_bar(int x, int y, int w, int h, unsigned short color)
{
    fill_rect(x, y, w, h, color);
}

/*
 * Draw the VU meter display: 4 labeled rows for the active channels.
 */
static void
draw_vu_display(int active_mask, const char *label)
{
    int bar_x = 120, bar_y = 160, bar_h = 40, gap = 60;
    int widths[4] = { 300, 250, 280, 200 };
    unsigned short colors[4] = { BAR_C, BAR_E, BAR_G, BAR_SQ };
    const char *names[4] = { "C4 sine", "E4 sine", "G4 sine", "C5 square" };
    int i;

    /* Clear display area */
    fill_rect(0, 140, SCREEN_W, 300, BG_COLOR);

    for (i = 0; i < 4; i++) {
        int y = bar_y + i * gap;
        unsigned short c = (active_mask & (1 << i)) ? colors[i] : BAR_DIM;
        int w = (active_mask & (1 << i)) ? widths[i] : 40;

        draw_bar(bar_x, y, w, bar_h, c);

        /* Channel status via UART */
        if (active_mask & (1 << i)) {
            puts_uart("  ch ");
            put_int(i);
            puts_uart(": ");
            puts_uart(names[i]);
            puts_uart(" [playing]\r\n");
        }
    }

    puts_uart("-- ");
    puts_uart(label);
    puts_uart(" --\r\n");
}

/* ---- Timing ------------------------------------------------------------ */

/*
 * Busy-wait delay.  Each iteration is roughly one NOP cycle.
 * At ~33 MHz emulated clock, 1M iterations ≈ 30 ms.
 */
static void
delay(int n)
{
    volatile int i;

    for (i = 0; i < n; i++)
        ;
}

#define BEAT        3000000     /* roughly 100 ms at emulated clock speed */

/* ---- Entry point ------------------------------------------------------- */

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    puts_uart("sound: Triton audio demo\r\n");

    /* Clear screen */
    fill_rect(0, 0, SCREEN_W, SCREEN_H, BG_COLOR);

    /* Generate waveform data in RAM */
    generate_samples();
    puts_uart("sound: waveforms generated at ");
    put_hex(SINE_ADDR);
    puts_uart(" (sine) and ");
    put_hex(SQUARE_ADDR);
    puts_uart(" (square)\r\n");

    /* Enable global audio (master volume slightly below max for headroom) */
    audio_wr8(AUDIO_MASTER_VOL_L, 200);
    audio_wr8(AUDIO_MASTER_VOL_R, 200);
    audio_wr8(AUDIO_GLOBAL_CTRL, GCTRL_ENABLE);
    puts_uart("sound: audio engine enabled\r\n");

    /* ---- Rising arpeggio ---- */

    /*
     * Channel volumes are set so the combined mix stays under 0 dBFS.
     * With 4 simultaneous channels of full-amplitude sine waves,
     * each channel needs to be at roughly 1/4 max volume (64) to
     * avoid clipping.  We use slightly higher and rely on the
     * stereo panning to keep the per-ear sum reasonable.
     */

    /* Beat 1: C4 alone, panned left */
    puts_uart("\r\nsound: beat 1 - C4\r\n");
    setup_channel(0, SINE_ADDR, SINE_LEN, NOTE_C4, 96, 32, CTRL_FMT_PCM16);
    draw_vu_display(0x01, "beat 1: C4");
    delay(BEAT * 4);

    /* Beat 2: add E4, panned center-left */
    puts_uart("\r\nsound: beat 2 - C4 + E4\r\n");
    setup_channel(1, SINE_ADDR, SINE_LEN, NOTE_E4, 72, 56, CTRL_FMT_PCM16);
    draw_vu_display(0x03, "beat 2: C4 + E4");
    delay(BEAT * 4);

    /* Beat 3: add G4, panned center-right */
    puts_uart("\r\nsound: beat 3 - C4 + E4 + G4\r\n");
    setup_channel(2, SINE_ADDR, SINE_LEN, NOTE_G4, 56, 72, CTRL_FMT_PCM16);
    draw_vu_display(0x07, "beat 3: C4 + E4 + G4");
    delay(BEAT * 4);

    /* Beat 4: add C5 square wave on top, panned right */
    puts_uart("\r\nsound: beat 4 - full chord + C5 square\r\n");
    setup_channel(3, SQUARE_ADDR, SQUARE_LEN, NOTE_C5, 32, 96, CTRL_FMT_PCM8);
    draw_vu_display(0x0F, "beat 4: full chord + C5 square");
    delay(BEAT * 8);

    /* ---- Fade out ---- */
    puts_uart("\r\nsound: fade out\r\n");
    stop_channel(3);
    draw_vu_display(0x07, "fade: drop C5");
    delay(BEAT * 2);

    stop_channel(2);
    draw_vu_display(0x03, "fade: drop G4");
    delay(BEAT * 2);

    stop_channel(1);
    draw_vu_display(0x01, "fade: drop E4");
    delay(BEAT * 2);

    stop_channel(0);
    draw_vu_display(0x00, "fade: silence");
    delay(BEAT * 2);

    /* ---- Check final status ---- */
    puts_uart("\r\nsound: final channel status:\r\n");
    {
        int ch;

        for (ch = 0; ch < 4; ch++) {
            unsigned char st = audio_rd8(CH(ch) + ACHAN_STATUS);

            puts_uart("  ch ");
            put_int(ch);
            puts_uart(": status=");
            put_hex(st);
            puts_uart(st & 0x01 ? " PLAYING" : " stopped");
            puts_uart("\r\n");
        }
    }

    puts_uart("\r\nsound: demo complete\r\n");

    __asm__ volatile("trap #0");
    for (;;)
        ;
}
