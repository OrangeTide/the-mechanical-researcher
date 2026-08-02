/* rv32_wasm.c : WebAssembly entry points for the RV32 emulator */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

/* A freestanding wasm32 module: no libc, no imports, no host calls from
 * inside the interpreter. The page allocates the guest's memory out of the
 * module's own linear memory and reaches it through the same bus callbacks
 * a native embedding uses, so nothing about the interpreter changes
 * between the two targets.
 *
 * Guest programs call into the host with ecall. Rather than calling out to
 * JavaScript, which would mean an import and a boundary crossing per call,
 * the handler writes into a small command block that the page reads after
 * each slice of execution. */

#include <stddef.h>
#include "../rv32.h"

#define GUEST_MEM_SIZE  (1u << 20)      /* 1 MiB of guest address space */
#define DRAW_MAX        4096            /* draw commands per frame */

/****************************************************************
 * Freestanding runtime
 *
 * clang lowers struct assignment and array initialisation to calls to
 * these, so a module built with -nostdlib has to define them itself.
 ****************************************************************/

void *
memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
    return dst;
}

void *
memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;

    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

size_t
strlen(const char *s)
{
    const char *p = s;

    while (*p)
        p++;
    return (size_t)(p - s);
}

/****************************************************************
 * Machine state
 ****************************************************************/

static rv_cpu cpu;
static uint8_t guest_mem[GUEST_MEM_SIZE];

/* Commands the guest has produced since the last frame. Each entry is
 * x, y, size and colour, packed so the page can read them as one array. */
static int32_t draw_cmds[DRAW_MAX * 4];
static uint32_t draw_count;

static uint32_t console[1024];
static uint32_t console_len;

/****************************************************************
 * Memory bus
 ****************************************************************/

static uint32_t
mem_r8(void *ctx, uint32_t addr)
{
    (void)ctx;
    return addr < GUEST_MEM_SIZE ? guest_mem[addr] : 0;
}

static uint32_t
mem_r16(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 1 >= GUEST_MEM_SIZE)
        return 0;
    return (uint32_t)guest_mem[addr] | ((uint32_t)guest_mem[addr + 1] << 8);
}

static uint32_t
mem_r32(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 3 >= GUEST_MEM_SIZE)
        return 0;
    return (uint32_t)guest_mem[addr] | ((uint32_t)guest_mem[addr + 1] << 8) |
           ((uint32_t)guest_mem[addr + 2] << 16) |
           ((uint32_t)guest_mem[addr + 3] << 24);
}

static void
mem_w8(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr < GUEST_MEM_SIZE)
        guest_mem[addr] = (uint8_t)val;
}

static void
mem_w16(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 1 < GUEST_MEM_SIZE) {
        guest_mem[addr] = (uint8_t)val;
        guest_mem[addr + 1] = (uint8_t)(val >> 8);
    }
}

static void
mem_w32(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 3 < GUEST_MEM_SIZE) {
        guest_mem[addr] = (uint8_t)val;
        guest_mem[addr + 1] = (uint8_t)(val >> 8);
        guest_mem[addr + 2] = (uint8_t)(val >> 16);
        guest_mem[addr + 3] = (uint8_t)(val >> 24);
    }
}

/** Refuse any access outside the guest's own memory. The guest cannot name
 * a host address in the first place, but a sandbox should say no rather
 * than silently read zero. */
static int
mem_probe(void *ctx, uint32_t addr, int size, int is_write)
{
    (void)ctx;
    (void)is_write;
    return addr + (uint32_t)size > GUEST_MEM_SIZE;
}

/****************************************************************
 * Environment calls
 *
 * a7 selects the service, a0 onwards carry the arguments, exactly as a
 * Linux system call would. A guest written in C reaches these through an
 * inline asm wrapper.
 ****************************************************************/

#define SYS_EXIT        93
#define SYS_DRAW        1024
#define SYS_PUTCHAR     1025
#define SYS_FRAME       1026

static int frame_done;

static int
do_ecall(rv_cpu *c, void *ctx)
{
    (void)ctx;

    switch (c->x[17]) {
    case SYS_EXIT:
        rv_halt(c);
        return 0;

    case SYS_DRAW:
        if (draw_count < DRAW_MAX) {
            draw_cmds[draw_count * 4 + 0] = (int32_t)c->x[10];
            draw_cmds[draw_count * 4 + 1] = (int32_t)c->x[11];
            draw_cmds[draw_count * 4 + 2] = (int32_t)c->x[12];
            draw_cmds[draw_count * 4 + 3] = (int32_t)c->x[13];
            draw_count++;
        }
        return 0;

    case SYS_PUTCHAR:
        if (console_len < 1024)
            console[console_len++] = c->x[10];
        return 0;

    case SYS_FRAME:
        /* Yield back to the page until the next animation frame */
        frame_done = 1;
        return 0;

    default:
        c->x[10] = (uint32_t)-38;       /* -ENOSYS */
        return 0;
    }
}

/****************************************************************
 * Exports
 ****************************************************************/

#define EXPORT __attribute__((visibility("default")))

EXPORT uint8_t *
rvw_memory(void)
{
    return guest_mem;
}

EXPORT uint32_t
rvw_memory_size(void)
{
    return GUEST_MEM_SIZE;
}

EXPORT int32_t *
rvw_draw_cmds(void)
{
    return draw_cmds;
}

EXPORT uint32_t
rvw_draw_count(void)
{
    return draw_count;
}

EXPORT uint32_t *
rvw_console(void)
{
    return console;
}

EXPORT uint32_t
rvw_console_len(void)
{
    return console_len;
}

EXPORT void
rvw_reset(uint32_t entry)
{
    rv_init(&cpu, mem_r8, mem_r16, mem_r32, mem_w8, mem_w16, mem_w32, 0);
    rv_set_ecall(&cpu, do_ecall, 0);
    rv_set_probe(&cpu, mem_probe);
    rv_reset(&cpu, entry);
    draw_count = 0;
    console_len = 0;
    frame_done = 0;
}

/** Run until the guest asks to yield, halts, or the budget runs out.
 *
 * The budget is what keeps a misbehaving guest from freezing the page: a
 * script that loops forever simply gets cut off at the end of its slice
 * and the frame is drawn anyway. */
EXPORT uint32_t
rvw_run_frame(uint32_t budget)
{
    uint32_t executed = 0;

    draw_count = 0;
    frame_done = 0;
    while (executed < budget && !frame_done) {
        if (rv_step(&cpu) < 0)
            break;
        executed++;
    }
    return executed;
}

EXPORT uint32_t
rvw_halted(void)
{
    return (uint32_t)cpu.halted;
}

EXPORT uint32_t
rvw_pc(void)
{
    return cpu.pc;
}

EXPORT uint64_t
rvw_cycles(void)
{
    return cpu.cycles;
}
