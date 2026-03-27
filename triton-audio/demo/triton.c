/*
 * triton.c -- Triton game console system emulator
 *
 * Wraps the ColdFire V4e CPU emulator in a complete system with memory-mapped
 * peripherals, a monitor ROM in NOR flash, and an optional SDL3 display.
 *
 * Build:
 *   gcc -O2 -o triton-headless triton.c coldfire.c glide_raster.c -lm
 *   gcc -O2 -DTRITON_SDL3 $(pkg-config --cflags --libs sdl3) \
 *       -o triton triton.c coldfire.c glide_raster.c -lm
 *
 * Usage:
 *   ./triton-headless              # run embedded hello program
 *   ./triton-headless program.elf  # load ELF from file
 *   ./triton program.elf           # same, with SDL3 display
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TRITON_SDL3
#include <SDL3/SDL.h>
#endif

#include "triton.h"
#include "triton_audio.h"
#include "glide_raster.h"
#include "monitor_rom.h"
#include "hello_program.h"

/* Display constants */
#define SCREEN_W    640
#define SCREEN_H    480

/* Approximate instructions per frame at 200 MHz, ~60 fps */
#define CYCLES_PER_FRAME    3333333

/* ---- big-endian memory helpers ----------------------------------------- */

static uint32_t
rd8(const uint8_t *mem, uint32_t off)
{
    return mem[off];
}

static uint32_t
rd16(const uint8_t *mem, uint32_t off)
{
    return ((uint32_t)mem[off] << 8) | mem[off + 1];
}

static uint32_t
rd32(const uint8_t *mem, uint32_t off)
{
    return ((uint32_t)mem[off] << 24) | ((uint32_t)mem[off + 1] << 16) |
           ((uint32_t)mem[off + 2] << 8) | mem[off + 3];
}

static void
wr8(uint8_t *mem, uint32_t off, uint32_t val)
{
    mem[off] = val & 0xFF;
}

static void
wr16(uint8_t *mem, uint32_t off, uint32_t val)
{
    mem[off]     = (val >> 8) & 0xFF;
    mem[off + 1] = val & 0xFF;
}

static void
wr32(uint8_t *mem, uint32_t off, uint32_t val)
{
    mem[off]     = (val >> 24) & 0xFF;
    mem[off + 1] = (val >> 16) & 0xFF;
    mem[off + 2] = (val >> 8) & 0xFF;
    mem[off + 3] = val & 0xFF;
}

/* ---- UART peripheral --------------------------------------------------- */

static uint32_t
uart_read(triton_sys *sys, uint32_t offset)
{
    switch (offset) {
    case UART_TX_STATUS:
        return 1;   /* always ready */
    case UART_RX_DATA:
        sys->uart_rx_ready = 0;
        return sys->uart_rx_data;
    case UART_RX_STATUS:
        return sys->uart_rx_ready ? 1 : 0;
    default:
        return 0;
    }
}

static void
uart_write(triton_sys *sys, uint32_t offset, uint32_t val)
{
    (void)sys;
    if (offset == UART_TX_DATA) {
        putchar(val & 0xFF);
        fflush(stdout);
    }
}

/* ---- GPU stub ---------------------------------------------------------- */

static uint32_t
gpu_read(triton_sys *sys, uint32_t offset)
{
    switch (offset) {
    case GPU_STATUS:
        return GPU_STATUS_IDLE | GPU_STATUS_VBLANK;
    case GPU_VID_PROC_CFG:
    case GPU_FB_BASE:
        return rd32(sys->gpu_regs, offset);
    default:
        if (offset + 3 < TRITON_GPU_SIZE)
            return rd32(sys->gpu_regs, offset);
        return 0;
    }
}

static void
gpu_write(triton_sys *sys, uint32_t offset, uint32_t val)
{
    if (offset + 3 < TRITON_GPU_SIZE)
        wr32(sys->gpu_regs, offset, val);
}

/* ---- Memory bus dispatch ----------------------------------------------- */

/*
 * Address decode: route CPU memory accesses to the appropriate region.
 *
 * 0x00000000 - 0x007FFFFF  RAM (8 MB)
 * 0x00800000 - 0x00FFFFFF  VRAM (8 MB)
 * 0x01000000 - 0x0100FFFF  GPU registers
 * 0x01100000 - 0x011003FF  Audio registers (stub)
 * 0x01110000 - 0x011100FF  SCSI registers (stub)
 * 0x01120000 - 0x011200FF  MMC registers (stub)
 * 0x01130000 - 0x011300FF  Input registers (stub)
 * 0x01140000 - 0x011400FF  Timer registers (stub)
 * 0x01150000 - 0x011500FF  UART
 * 0x01160000 - 0x011600FF  DMA registers (stub)
 * 0x01200000 - 0x015FFFFF  NOR flash (4 MB, read-only)
 */

static uint32_t
bus_read8(void *ctx, uint32_t addr)
{
    triton_sys *sys = ctx;

    if (addr < TRITON_RAM_END)
        return rd8(sys->ram, addr);
    if (addr >= TRITON_VRAM_BASE && addr < TRITON_VRAM_END)
        return rd8(sys->vram, addr - TRITON_VRAM_BASE);
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END)
        return uart_read(sys, addr - TRITON_UART_BASE);
    if (addr >= TRITON_GPU_BASE && addr < TRITON_GPU_END)
        return gpu_read(sys, addr - TRITON_GPU_BASE) & 0xFF;
    if (addr >= TRITON_AUDIO_BASE && addr < TRITON_AUDIO_END)
        return triton_audio_read(sys->audio, addr - TRITON_AUDIO_BASE);
    if (addr >= TRITON_FLASH_BASE && addr < TRITON_FLASH_END)
        return rd8(sys->flash, addr - TRITON_FLASH_BASE);
    /* Stubs: SCSI, MMC, input, timer, DMA */
    return 0;
}

static uint32_t
bus_read16(void *ctx, uint32_t addr)
{
    triton_sys *sys = ctx;

    if (addr + 1 < TRITON_RAM_END)
        return rd16(sys->ram, addr);
    if (addr >= TRITON_VRAM_BASE && addr + 1 < TRITON_VRAM_END)
        return rd16(sys->vram, addr - TRITON_VRAM_BASE);
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END)
        return uart_read(sys, addr - TRITON_UART_BASE);
    if (addr >= TRITON_GPU_BASE && addr + 1 < TRITON_GPU_END)
        return gpu_read(sys, addr - TRITON_GPU_BASE) & 0xFFFF;
    if (addr >= TRITON_AUDIO_BASE && addr + 1 < TRITON_AUDIO_END)
        return (triton_audio_read(sys->audio, addr - TRITON_AUDIO_BASE) << 8) |
               triton_audio_read(sys->audio, addr - TRITON_AUDIO_BASE + 1);
    if (addr >= TRITON_FLASH_BASE && addr + 1 < TRITON_FLASH_END)
        return rd16(sys->flash, addr - TRITON_FLASH_BASE);
    return 0;
}

static uint32_t
bus_read32(void *ctx, uint32_t addr)
{
    triton_sys *sys = ctx;

    if (addr + 3 < TRITON_RAM_END)
        return rd32(sys->ram, addr);
    if (addr >= TRITON_VRAM_BASE && addr + 3 < TRITON_VRAM_END)
        return rd32(sys->vram, addr - TRITON_VRAM_BASE);
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END)
        return uart_read(sys, addr - TRITON_UART_BASE);
    if (addr >= TRITON_GPU_BASE && addr + 3 < TRITON_GPU_END)
        return gpu_read(sys, addr - TRITON_GPU_BASE);
    if (addr >= TRITON_AUDIO_BASE && addr + 3 < TRITON_AUDIO_END) {
        uint32_t off = addr - TRITON_AUDIO_BASE;
        return (triton_audio_read(sys->audio, off) << 24) |
               (triton_audio_read(sys->audio, off + 1) << 16) |
               (triton_audio_read(sys->audio, off + 2) << 8) |
               triton_audio_read(sys->audio, off + 3);
    }
    if (addr >= TRITON_FLASH_BASE && addr + 3 < TRITON_FLASH_END)
        return rd32(sys->flash, addr - TRITON_FLASH_BASE);
    return 0;
}

static void
bus_write8(void *ctx, uint32_t addr, uint32_t val)
{
    triton_sys *sys = ctx;

    if (addr < TRITON_RAM_END) {
        wr8(sys->ram, addr, val);
        return;
    }
    if (addr >= TRITON_VRAM_BASE && addr < TRITON_VRAM_END) {
        wr8(sys->vram, addr - TRITON_VRAM_BASE, val);
        return;
    }
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END) {
        uart_write(sys, addr - TRITON_UART_BASE, val);
        return;
    }
    if (addr >= TRITON_GPU_BASE && addr < TRITON_GPU_END) {
        gpu_write(sys, addr - TRITON_GPU_BASE, val);
        return;
    }
    if (addr >= TRITON_AUDIO_BASE && addr < TRITON_AUDIO_END) {
        triton_audio_write(sys->audio, addr - TRITON_AUDIO_BASE, val);
        return;
    }
    /* NOR flash: read-only, silently ignore writes */
}

static void
bus_write16(void *ctx, uint32_t addr, uint32_t val)
{
    triton_sys *sys = ctx;

    if (addr + 1 < TRITON_RAM_END) {
        wr16(sys->ram, addr, val);
        return;
    }
    if (addr >= TRITON_VRAM_BASE && addr + 1 < TRITON_VRAM_END) {
        wr16(sys->vram, addr - TRITON_VRAM_BASE, val);
        return;
    }
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END) {
        uart_write(sys, addr - TRITON_UART_BASE, val);
        return;
    }
    if (addr >= TRITON_GPU_BASE && addr + 1 < TRITON_GPU_END) {
        gpu_write(sys, addr - TRITON_GPU_BASE, val);
        return;
    }
    if (addr >= TRITON_AUDIO_BASE && addr + 1 < TRITON_AUDIO_END) {
        uint32_t off = addr - TRITON_AUDIO_BASE;
        triton_audio_write(sys->audio, off,     (val >> 8) & 0xFF);
        triton_audio_write(sys->audio, off + 1, val & 0xFF);
        return;
    }
}

static void
bus_write32(void *ctx, uint32_t addr, uint32_t val)
{
    triton_sys *sys = ctx;

    if (addr + 3 < TRITON_RAM_END) {
        wr32(sys->ram, addr, val);
        return;
    }
    if (addr >= TRITON_VRAM_BASE && addr + 3 < TRITON_VRAM_END) {
        wr32(sys->vram, addr - TRITON_VRAM_BASE, val);
        return;
    }
    if (addr >= TRITON_UART_BASE && addr < TRITON_UART_END) {
        uart_write(sys, addr - TRITON_UART_BASE, val);
        return;
    }
    if (addr >= TRITON_GPU_BASE && addr + 3 < TRITON_GPU_END) {
        gpu_write(sys, addr - TRITON_GPU_BASE, val);
        return;
    }
    if (addr >= TRITON_AUDIO_BASE && addr + 3 < TRITON_AUDIO_END) {
        uint32_t off = addr - TRITON_AUDIO_BASE;
        triton_audio_write(sys->audio, off,     (val >> 24) & 0xFF);
        triton_audio_write(sys->audio, off + 1, (val >> 16) & 0xFF);
        triton_audio_write(sys->audio, off + 2, (val >> 8) & 0xFF);
        triton_audio_write(sys->audio, off + 3, val & 0xFF);
        return;
    }
}

/* ---- ELF file loader (host side, into guest RAM) ----------------------- */

static int
load_elf_file(triton_sys *sys, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "triton: cannot open %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || (uint32_t)size > TRITON_RAM_SIZE - TRITON_ELF_STAGE) {
        fprintf(stderr, "triton: ELF too large (%ld bytes)\n", size);
        fclose(f);
        return -1;
    }

    size_t n = fread(sys->ram + TRITON_ELF_STAGE, 1, (size_t)size, f);
    fclose(f);

    if ((long)n != size) {
        fprintf(stderr, "triton: short read (%zu of %ld bytes)\n", n, size);
        return -1;
    }

    printf("triton: loaded %ld bytes from %s at 0x%08X\n",
           size, path, TRITON_ELF_STAGE);
    return 0;
}

/* ---- SDL3 display ------------------------------------------------------ */

#ifdef TRITON_SDL3
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
} triton_display;

static int
display_init(triton_display *disp)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "triton: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    disp->window = SDL_CreateWindow("Triton", SCREEN_W, SCREEN_H, 0);
    if (!disp->window) {
        fprintf(stderr, "triton: SDL_CreateWindow failed: %s\n",
                SDL_GetError());
        return -1;
    }

    disp->renderer = SDL_CreateRenderer(disp->window, NULL);
    if (!disp->renderer) {
        fprintf(stderr, "triton: SDL_CreateRenderer failed: %s\n",
                SDL_GetError());
        return -1;
    }

    disp->texture = SDL_CreateTexture(disp->renderer,
                                      SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      SCREEN_W, SCREEN_H);
    if (!disp->texture) {
        fprintf(stderr, "triton: SDL_CreateTexture failed: %s\n",
                SDL_GetError());
        return -1;
    }

    return 0;
}

static void
display_update(triton_display *disp, const uint8_t *vram)
{
    SDL_UpdateTexture(disp->texture, NULL, vram, SCREEN_W * 2);
    SDL_RenderClear(disp->renderer);
    SDL_RenderTexture(disp->renderer, disp->texture, NULL, NULL);
    SDL_RenderPresent(disp->renderer);
}

static int
display_poll(triton_display *disp)
{
    (void)disp;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT)
            return -1;
    }
    return 0;
}

static void
display_shutdown(triton_display *disp)
{
    if (disp->texture)
        SDL_DestroyTexture(disp->texture);
    if (disp->renderer)
        SDL_DestroyRenderer(disp->renderer);
    if (disp->window)
        SDL_DestroyWindow(disp->window);
    SDL_Quit();
}

/* ---- SDL3 audio -------------------------------------------------------- */

static void SDLCALL
audio_sdl_callback(void *userdata, SDL_AudioStream *stream,
                   int additional_amount, int total_amount)
{
    (void)total_amount;
    triton_sys *s = userdata;
    int nframes = additional_amount / (int)(2 * sizeof(int16_t));
    if (nframes <= 0)
        return;

    /* Stack buffer for typical requests, heap for large ones */
    int16_t stack_buf[2048];
    int16_t *buf = (nframes <= 1024) ? stack_buf
                                     : (int16_t *)malloc((size_t)nframes * 4);
    if (!buf)
        return;

    triton_audio_mix(s->audio, buf, nframes);
    SDL_PutAudioStreamData(stream, buf, nframes * (int)(2 * sizeof(int16_t)));

    if (buf != stack_buf)
        free(buf);
}

static SDL_AudioStream *
audio_sdl_init(triton_sys *s)
{
    SDL_AudioSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.freq = AUDIO_SAMPLE_RATE;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
        audio_sdl_callback, s);
    if (!stream) {
        fprintf(stderr, "triton: SDL audio init failed: %s\n",
                SDL_GetError());
        return NULL;
    }
    SDL_ResumeAudioStreamDevice(stream);
    return stream;
}

#endif /* TRITON_SDL3 */

/* ---- Frame dump (TGA) -------------------------------------------------- */

/* Frame numbers to capture, parsed from -F argument */
#define MAX_FRAME_CAPTURES 1024
static int frame_captures[MAX_FRAME_CAPTURES];
static int num_frame_captures;
static int frame_number;    /* incremented on each grBufferSwap */

static int
should_capture_frame(int frame)
{
    int i;
    for (i = 0; i < num_frame_captures; i++) {
        if (frame_captures[i] == frame)
            return 1;
    }
    return 0;
}

/*
 * Parse a frame capture spec: "0,1,101-104" → individual frame numbers.
 * Fills frame_captures[] and sets num_frame_captures.
 */
static void
parse_frame_spec(const char *spec)
{
    const char *p = spec;

    while (*p && num_frame_captures < MAX_FRAME_CAPTURES) {
        char *end;
        long a = strtol(p, &end, 10);
        if (end == p)
            break;
        if (*end == '-') {
            /* Range: a-b */
            p = end + 1;
            long b = strtol(p, &end, 10);
            long i;
            for (i = a; i <= b && num_frame_captures < MAX_FRAME_CAPTURES; i++)
                frame_captures[num_frame_captures++] = (int)i;
        } else {
            frame_captures[num_frame_captures++] = (int)a;
        }
        p = end;
        if (*p == ',')
            p++;
    }
}

/*
 * Save the front buffer as an uncompressed 24-bit TGA file.
 * Converts RGB565 big-endian VRAM to 24-bit BGR (TGA native order).
 */
static void
save_frame_tga(const uint8_t *vram, uint32_t fb_offset, int frame)
{
    char filename[64];
    unsigned char header[18];
    FILE *f;
    int x, y;

    snprintf(filename, sizeof(filename), "fr%06d.tga", frame);

    memset(header, 0, sizeof(header));
    header[2] = 2;                          /* uncompressed true-color */
    header[12] = SCREEN_W & 0xFF;           /* width low */
    header[13] = (SCREEN_W >> 8) & 0xFF;    /* width high */
    header[14] = SCREEN_H & 0xFF;           /* height low */
    header[15] = (SCREEN_H >> 8) & 0xFF;    /* height high */
    header[16] = 24;                        /* bits per pixel */
    header[17] = 0x20;                      /* top-left origin */

    f = fopen(filename, "wb");
    if (!f) {
        perror(filename);
        return;
    }
    fwrite(header, sizeof(header), 1, f);

    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            uint32_t off = fb_offset + ((uint32_t)y * SCREEN_W + (uint32_t)x) * 2;
            uint16_t px = (uint16_t)((vram[off] << 8) | vram[off + 1]);
            int r5 = (px >> 11) & 0x1F;
            int g6 = (px >> 5) & 0x3F;
            int b5 = px & 0x1F;
            unsigned char bgr[3];
            bgr[0] = (unsigned char)((b5 << 3) | (b5 >> 2));
            bgr[1] = (unsigned char)((g6 << 2) | (g6 >> 4));
            bgr[2] = (unsigned char)((r5 << 3) | (r5 >> 2));
            fwrite(bgr, 3, 1, f);
        }
    }
    fclose(f);
    fprintf(stderr, "triton: saved %s\n", filename);
}

/* ---- WAV audio capture ------------------------------------------------- */

static const char *wav_path;           /* -W output file, NULL = disabled */
static int16_t *wav_buf;               /* accumulated stereo samples */
static size_t wav_buf_frames;          /* number of stereo frames stored */
static size_t wav_buf_cap;             /* allocated capacity in frames */

static void
wav_capture_mix(triton_audio *au, int nframes)
{
    /* Grow buffer if needed */
    if (wav_buf_frames + (size_t)nframes > wav_buf_cap) {
        size_t new_cap = wav_buf_cap ? wav_buf_cap * 2 : 65536;
        while (new_cap < wav_buf_frames + (size_t)nframes)
            new_cap *= 2;
        wav_buf = realloc(wav_buf, new_cap * 2 * sizeof(int16_t));
        wav_buf_cap = new_cap;
    }

    triton_audio_mix(au, wav_buf + wav_buf_frames * 2, nframes);
    wav_buf_frames += (size_t)nframes;
}

static void
wav_save(const char *filename)
{
    if (!wav_buf || wav_buf_frames == 0)
        return;

    unsigned nr_samples = (unsigned)(wav_buf_frames * 2); /* stereo */
    unsigned sample_rate = AUDIO_SAMPLE_RATE;
    unsigned channels = 2;
    unsigned bits = 16;
    unsigned data_bytes = nr_samples * bits / 8;
    unsigned char header[44] = {
        'R','I','F','F', 0,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 0,0, 0,0,0,0, 0,0,0,0, 0,0, 0,0,
        'd','a','t','a', 0,0,0,0,
    };

    /* Fill header fields (little-endian) */
    unsigned chunk_sz = sizeof(header) - 8 + data_bytes;
    header[4]  = chunk_sz & 0xFF;
    header[5]  = (chunk_sz >> 8) & 0xFF;
    header[6]  = (chunk_sz >> 16) & 0xFF;
    header[7]  = (chunk_sz >> 24) & 0xFF;
    header[22] = channels & 0xFF;
    header[23] = (channels >> 8) & 0xFF;
    header[24] = sample_rate & 0xFF;
    header[25] = (sample_rate >> 8) & 0xFF;
    header[26] = (sample_rate >> 16) & 0xFF;
    header[27] = (sample_rate >> 24) & 0xFF;
    unsigned byte_rate = sample_rate * channels * bits / 8;
    header[28] = byte_rate & 0xFF;
    header[29] = (byte_rate >> 8) & 0xFF;
    header[30] = (byte_rate >> 16) & 0xFF;
    header[31] = (byte_rate >> 24) & 0xFF;
    unsigned block_align = channels * bits / 8;
    header[32] = block_align & 0xFF;
    header[33] = (block_align >> 8) & 0xFF;
    header[34] = bits & 0xFF;
    header[35] = (bits >> 8) & 0xFF;
    header[40] = data_bytes & 0xFF;
    header[41] = (data_bytes >> 8) & 0xFF;
    header[42] = (data_bytes >> 16) & 0xFF;
    header[43] = (data_bytes >> 24) & 0xFF;

    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror(filename);
        return;
    }
    fwrite(header, sizeof(header), 1, f);

    /* Write samples as little-endian int16 */
    for (unsigned i = 0; i < nr_samples; i++) {
        unsigned char le[2];
        int16_t s = wav_buf[i];
        le[0] = (unsigned char)(s & 0xFF);
        le[1] = (unsigned char)((s >> 8) & 0xFF);
        fwrite(le, 2, 1, f);
    }

    fclose(f);
    fprintf(stderr, "triton: saved %s (stereo %u Hz, %.2f seconds)\n",
            filename, sample_rate,
            (double)wav_buf_frames / (double)sample_rate);
}

/* ---- Glide hypercall handler ------------------------------------------- */

static int
glide_hypercall(cf_cpu *cpu, uint16_t opword, void *ctx)
{
    triton_sys *sys = ctx;
    int rc = glide_dispatch(sys->gpu, cpu, opword);

    /* Check for buffer swap — capture frame if requested */
    if (rc == 0 && sys->gpu->vblank_wait && num_frame_captures > 0) {
        if (should_capture_frame(frame_number))
            save_frame_tga(sys->vram, sys->gpu->front_offset, frame_number);
        frame_number++;
    }

    return rc;
}

/* ---- Audio IRQ callback ------------------------------------------------ */

static void
audio_irq_callback(void *ctx)
{
    triton_sys *sys = ctx;
    cf_exception(&sys->cpu, CF_VEC_AUTOVECTOR(4));  /* IPL 5 */
}

/* ---- Main -------------------------------------------------------------- */

static triton_sys sys;
static glide_state gpu_state;
static triton_audio audio_state;

int
main(int argc, char **argv)
{
    const char *elf_path = NULL;
    int i;

    memset(&sys, 0, sizeof(sys));
    sys.gpu = &gpu_state;
    sys.audio = &audio_state;

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-F") == 0 && i + 1 < argc) {
            parse_frame_spec(argv[++i]);
        } else if (strcmp(argv[i], "-W") == 0 && i + 1 < argc) {
            wav_path = argv[++i];
        } else if (argv[i][0] != '-') {
            elf_path = argv[i];
        } else {
            fprintf(stderr, "Usage: %s [-F frames] [-W audio.wav] [program.elf]\n", argv[0]);
            fprintf(stderr, "  -F frames     Capture frames as TGA (e.g. 0,1,101-104)\n");
            fprintf(stderr, "  -W audio.wav  Capture audio to WAV file (headless)\n");
            return 1;
        }
    }

    /* Install monitor ROM into NOR flash */
    if (monitor_rom_size > TRITON_FLASH_SIZE) {
        fprintf(stderr, "triton: monitor ROM too large\n");
        return 1;
    }
    memcpy(sys.flash, monitor_rom_data, monitor_rom_size);
    printf("triton: monitor ROM installed (%u bytes at 0x%08X)\n",
           monitor_rom_size, TRITON_FLASH_BASE);

    /* Load guest program: from file or embedded hello */
    if (elf_path) {
        if (load_elf_file(&sys, elf_path) < 0)
            return 1;
    } else {
        memcpy(sys.ram + TRITON_ELF_STAGE,
               hello_program_data, hello_program_size);
        printf("triton: embedded hello program staged (%u bytes at 0x%08X)\n",
               hello_program_size, TRITON_ELF_STAGE);
    }

    /* Initialize CPU with bus callbacks */
    cf_init(&sys.cpu,
            bus_read8, bus_read16, bus_read32,
            bus_write8, bus_write16, bus_write32,
            &sys);

    /* Initialize GPU rasterizer and install hypercall handler */
    glide_init(sys.gpu, sys.vram, sys.ram);
    cf_set_hypercall(&sys.cpu, glide_hypercall, &sys);

    /* Initialize audio mixer */
    triton_audio_init(sys.audio, sys.ram, TRITON_RAM_SIZE,
                      audio_irq_callback, &sys);

    /* SoC hardwires VBR to NOR flash base on reset */
    sys.cpu.vbr = TRITON_FLASH_BASE;
    cf_reset(&sys.cpu);

    printf("triton: CPU reset, PC=0x%08X SP=0x%08X\n",
           sys.cpu.pc, sys.cpu.a[7]);

    sys.running = 1;

#ifdef TRITON_SDL3
    triton_display disp;
    memset(&disp, 0, sizeof(disp));

    SDL_AudioStream *audio_stream = NULL;

    if (!sys.headless) {
        if (display_init(&disp) < 0) {
            fprintf(stderr, "triton: falling back to headless mode\n");
            sys.headless = 1;
        } else {
            audio_stream = audio_sdl_init(&sys);
        }
    }
#else
    sys.headless = 1;
#endif

    /* Main emulation loop */
    if (sys.headless) {
        int headless_frame = 0;

        /* Headless: run until halt */
        while (sys.running) {
            int n = cf_run(&sys.cpu, CYCLES_PER_FRAME);
            if (sys.gpu->vblank_wait) {
                /* grBufferSwap halted the CPU — resume after "vblank" */
                sys.gpu->vblank_wait = 0;
                sys.cpu.halted = 0;
            }
            /* Advance audio by one frame (735 samples = 44100/60) */
            if (wav_path)
                wav_capture_mix(sys.audio, 735);
            else
                triton_audio_mix(sys.audio, NULL, 735);
            triton_audio_check_irq(sys.audio);

            /* Capture frame if requested (non-Glide programs) */
            if (num_frame_captures > 0 && !sys.gpu->vblank_wait) {
                if (should_capture_frame(headless_frame))
                    save_frame_tga(sys.vram, 0, headless_frame);
                headless_frame++;
            }

            if (n == 0 || sys.cpu.halted)
                sys.running = 0;
        }
    } else {
#ifdef TRITON_SDL3
        while (sys.running) {
            int n = cf_run(&sys.cpu, CYCLES_PER_FRAME);

            /* Check audio IRQs after each CPU batch */
            triton_audio_check_irq(sys.audio);

            if (sys.gpu->vblank_wait) {
                /* grBufferSwap halted the CPU — show frame and wait */
                display_update(&disp, sys.vram + sys.gpu->front_offset);
                if (display_poll(&disp) < 0)
                    sys.running = 0;
                SDL_Delay(16 * sys.gpu->vblank_wait);
                sys.gpu->vblank_wait = 0;
                sys.cpu.halted = 0;
                continue;
            }

            if (n == 0 || sys.cpu.halted)
                sys.running = 0;

            if (display_poll(&disp) < 0)
                sys.running = 0;

            display_update(&disp, sys.vram + sys.gpu->front_offset);
            SDL_Delay(16);
        }
        if (audio_stream)
            SDL_DestroyAudioStream(audio_stream);
        display_shutdown(&disp);
#endif
    }

    /* Save captured audio if requested */
    if (wav_path) {
        wav_save(wav_path);
        free(wav_buf);
    }

    printf("\ntriton: halted after %lu instructions\n",
           (unsigned long)sys.cpu.cycles);
    return 0;
}
