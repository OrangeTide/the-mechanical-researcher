/*
 * triton.c -- Triton game console system emulator
 *
 * Wraps the ColdFire V4e CPU emulator in a complete system with memory-mapped
 * peripherals, a monitor ROM in NOR flash, and an optional SDL3 display.
 *
 * Build:
 *   gcc -O2 -o triton-headless triton.c coldfire.c -lm
 *   gcc -O2 -DTRITON_SDL3 $(pkg-config --cflags --libs sdl3) \
 *       -o triton triton.c coldfire.c -lm
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
    if (addr >= TRITON_FLASH_BASE && addr < TRITON_FLASH_END)
        return rd8(sys->flash, addr - TRITON_FLASH_BASE);
    /* Stubs: audio, SCSI, MMC, input, timer, DMA */
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
    /* NOR flash: read-only, silently ignore writes */
    /* Stubs: silently ignore */
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
    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
#endif /* TRITON_SDL3 */

/* ---- Main -------------------------------------------------------------- */

static triton_sys sys;

int
main(int argc, char **argv)
{
    memset(&sys, 0, sizeof(sys));

    /* Install monitor ROM into NOR flash */
    if (monitor_rom_size > TRITON_FLASH_SIZE) {
        fprintf(stderr, "triton: monitor ROM too large\n");
        return 1;
    }
    memcpy(sys.flash, monitor_rom_data, monitor_rom_size);
    printf("triton: monitor ROM installed (%u bytes at 0x%08X)\n",
           monitor_rom_size, TRITON_FLASH_BASE);

    /* Load guest program: from file or embedded hello */
    if (argc > 1) {
        if (load_elf_file(&sys, argv[1]) < 0)
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

    /* SoC hardwires VBR to NOR flash base on reset */
    sys.cpu.vbr = TRITON_FLASH_BASE;
    cf_reset(&sys.cpu);

    printf("triton: CPU reset, PC=0x%08X SP=0x%08X\n",
           sys.cpu.pc, sys.cpu.a[7]);

    sys.running = 1;

#ifdef TRITON_SDL3
    triton_display disp;
    memset(&disp, 0, sizeof(disp));

    if (!sys.headless) {
        if (display_init(&disp) < 0) {
            fprintf(stderr, "triton: falling back to headless mode\n");
            sys.headless = 1;
        }
    }
#else
    sys.headless = 1;
#endif

    /* Main emulation loop */
    if (sys.headless) {
        /* Headless: run until halt */
        while (sys.running) {
            int n = cf_run(&sys.cpu, CYCLES_PER_FRAME);
            if (n == 0 || sys.cpu.halted)
                sys.running = 0;
        }
    } else {
#ifdef TRITON_SDL3
        while (sys.running) {
            int n = cf_run(&sys.cpu, CYCLES_PER_FRAME);
            if (n == 0 || sys.cpu.halted)
                sys.running = 0;

            if (display_poll(&disp) < 0)
                sys.running = 0;

            display_update(&disp, sys.vram);
            SDL_Delay(16);
        }
        display_shutdown(&disp);
#endif
    }

    printf("\ntriton: halted after %lu instructions\n",
           (unsigned long)sys.cpu.cycles);
    return 0;
}
