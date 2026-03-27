/*
 * triton.h -- Triton game console system emulator
 *
 * Memory map constants, peripheral register offsets, and system state.
 */

#ifndef TRITON_H
#define TRITON_H

#include <stdint.h>
#include "coldfire.h"

/* --- Memory map --------------------------------------------------------- */

#define TRITON_RAM_BASE     0x00000000
#define TRITON_RAM_SIZE     (8 * 1024 * 1024)   /* 8 MB */
#define TRITON_RAM_END      (TRITON_RAM_BASE + TRITON_RAM_SIZE)

#define TRITON_VRAM_BASE    0x00800000
#define TRITON_VRAM_SIZE    (8 * 1024 * 1024)   /* 8 MB */
#define TRITON_VRAM_END     (TRITON_VRAM_BASE + TRITON_VRAM_SIZE)

#define TRITON_GPU_BASE     0x01000000
#define TRITON_GPU_SIZE     0x00010000           /* 64 KB */
#define TRITON_GPU_END      (TRITON_GPU_BASE + TRITON_GPU_SIZE)

#define TRITON_AUDIO_BASE   0x01100000
#define TRITON_AUDIO_SIZE   0x00000400           /* 1 KB */
#define TRITON_AUDIO_END    (TRITON_AUDIO_BASE + TRITON_AUDIO_SIZE)

#define TRITON_SCSI_BASE    0x01110000
#define TRITON_SCSI_SIZE    0x00000100
#define TRITON_SCSI_END     (TRITON_SCSI_BASE + TRITON_SCSI_SIZE)

#define TRITON_MMC_BASE     0x01120000
#define TRITON_MMC_SIZE     0x00000100
#define TRITON_MMC_END      (TRITON_MMC_BASE + TRITON_MMC_SIZE)

#define TRITON_INPUT_BASE   0x01130000
#define TRITON_INPUT_SIZE   0x00000100
#define TRITON_INPUT_END    (TRITON_INPUT_BASE + TRITON_INPUT_SIZE)

#define TRITON_TIMER_BASE   0x01140000
#define TRITON_TIMER_SIZE   0x00000100
#define TRITON_TIMER_END    (TRITON_TIMER_BASE + TRITON_TIMER_SIZE)

#define TRITON_UART_BASE    0x01150000
#define TRITON_UART_SIZE    0x00000100
#define TRITON_UART_END     (TRITON_UART_BASE + TRITON_UART_SIZE)

#define TRITON_DMA_BASE     0x01160000
#define TRITON_DMA_SIZE     0x00000100
#define TRITON_DMA_END      (TRITON_DMA_BASE + TRITON_DMA_SIZE)

#define TRITON_FLASH_BASE   0x01200000
#define TRITON_FLASH_SIZE   (4 * 1024 * 1024)   /* 4 MB */
#define TRITON_FLASH_END    (TRITON_FLASH_BASE + TRITON_FLASH_SIZE)

/* --- UART registers (offsets from TRITON_UART_BASE) --------------------- */

#define UART_TX_DATA        0x00    /* W: transmit byte */
#define UART_TX_STATUS      0x04    /* R: bit 0 = ready */
#define UART_RX_DATA        0x08    /* R: received byte */
#define UART_RX_STATUS      0x0C    /* R: bit 0 = byte available */

/* --- GPU registers (offsets from TRITON_GPU_BASE) ----------------------- */

#define GPU_STATUS          0x00    /* R: status (not busy, vblank) */
#define GPU_VID_PROC_CFG    0x04    /* R/W: video processor config */
#define GPU_FB_BASE         0x08    /* R/W: framebuffer base in VRAM */

/* GPU status bits */
#define GPU_STATUS_IDLE     (1 << 0)
#define GPU_STATUS_VBLANK   (1 << 1)

/* --- ELF staging address ------------------------------------------------ */

#define TRITON_ELF_STAGE    0x00001000  /* host puts raw ELF here */

/* --- Forward declarations ----------------------------------------------- */

typedef struct glide_state glide_state;

/* --- System state ------------------------------------------------------- */

typedef struct triton_sys {
    cf_cpu cpu;

    uint8_t ram[TRITON_RAM_SIZE];
    uint8_t vram[TRITON_VRAM_SIZE];
    uint8_t flash[TRITON_FLASH_SIZE];

    /* GPU register file (64 KB) */
    uint8_t gpu_regs[TRITON_GPU_SIZE];

    /* GPU rasterizer state (allocated separately due to size) */
    glide_state *gpu;

    /* UART state */
    uint8_t uart_rx_data;
    uint8_t uart_rx_ready;

    /* Runtime flags */
    int running;
    int headless;
} triton_sys;

#endif /* TRITON_H */
