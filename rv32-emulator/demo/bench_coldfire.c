/* bench_coldfire.c : times the ColdFire V4e emulator on the shared benchmark */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

/* The counterpart to bench_rv32.c. It runs the same workload, compiled
 * from the same bench.c, on the ColdFire V4e emulator from the earlier
 * article. Both runners use the emulator's own step function in a plain
 * loop with no tracing, so the number reported is the interpreter's
 * throughput and not the harness around it.
 *
 * This file reaches into a sibling article's directory for coldfire.c and
 * its big-endian ELF loader, which is why it is built by an explicit make
 * target rather than by the default build. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coldfire.h"
#include "elf_loader.h"

#define MEM_SIZE  (16 * 1024 * 1024)
#define MAX_STEPS 2000000000

static uint8_t *mem;

/****************************************************************
 * Big-endian memory bus
 ****************************************************************/

static uint32_t
mem_read8(void *ctx, uint32_t addr)
{
    (void)ctx;
    return addr < MEM_SIZE ? mem[addr] : 0;
}

static uint32_t
mem_read16(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 1 >= MEM_SIZE)
        return 0;
    return ((uint32_t)mem[addr] << 8) | mem[addr + 1];
}

static uint32_t
mem_read32(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 3 >= MEM_SIZE)
        return 0;
    return ((uint32_t)mem[addr] << 24) | ((uint32_t)mem[addr + 1] << 16) |
           ((uint32_t)mem[addr + 2] << 8) | mem[addr + 3];
}

static void
mem_write8(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        mem[addr] = (uint8_t)val;
}

static void
mem_write16(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 1 < MEM_SIZE) {
        mem[addr] = (uint8_t)(val >> 8);
        mem[addr + 1] = (uint8_t)val;
    }
}

static void
mem_write32(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 3 < MEM_SIZE) {
        mem[addr] = (uint8_t)(val >> 24);
        mem[addr + 1] = (uint8_t)(val >> 16);
        mem[addr + 2] = (uint8_t)(val >> 8);
        mem[addr + 3] = (uint8_t)val;
    }
}

static void
poke(void *ctx, uint32_t addr, uint8_t byte)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        mem[addr] = byte;
}

/****************************************************************
 * Symbol lookup
 *
 * The ColdFire ELF loader in the earlier article does not expose one, and
 * the results have to be read back to confirm that both emulators
 * computed the same answers rather than merely running for a while.
 ****************************************************************/

static uint16_t
be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t
be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t
elf_symbol_be(const char *path, const char *name)
{
    FILE *f;
    uint8_t *buf;
    long fsize;
    uint32_t shoff, result = 0;
    uint16_t shnum, shentsize;
    int i;

    f = fopen(path, "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(fsize);
    if (!buf || fread(buf, 1, fsize, f) != (size_t)fsize) {
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);

    shoff     = be32(buf + 32);
    shentsize = be16(buf + 46);
    shnum     = be16(buf + 48);

    for (i = 0; i < shnum && !result; i++) {
        const uint8_t *sh = buf + shoff + (uint32_t)i * shentsize;
        uint32_t off, size, link, entsize, k;
        const uint8_t *strtab;

        if (be32(sh + 4) != 2)          /* SHT_SYMTAB */
            continue;
        off     = be32(sh + 16);
        size    = be32(sh + 20);
        link    = be32(sh + 24);
        entsize = be32(sh + 36);
        if (entsize == 0 || link >= shnum)
            continue;

        strtab = buf + be32(buf + shoff + link * shentsize + 16);
        for (k = 0; k + entsize <= size; k += entsize) {
            const uint8_t *sym = buf + off + k;

            if (strcmp((const char *)strtab + be32(sym + 0), name) == 0) {
                result = be32(sym + 4);
                break;
            }
        }
    }

    free(buf);
    return result;
}

/****************************************************************
 * Timing
 ****************************************************************/

static double
now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int
main(int argc, char **argv)
{
    cf_cpu cpu;
    uint32_t entry;
    double t0, t1;
    long steps = 0;
    int reps = 1, r;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <bench.elf> [reps]\n", argv[0]);
        return 2;
    }
    if (argc > 2)
        reps = atoi(argv[2]);

    mem = calloc(1, MEM_SIZE);
    if (!mem)
        return 1;

    entry = elf_load(argv[1], poke, NULL);
    if (!entry) {
        free(mem);
        return 1;
    }

    /* Reset vector and a HALT instruction behind TRAP #0, the same
     * arrangement the ColdFire article's own harness uses. */
    mem_write32(NULL, 0x00, MEM_SIZE);
    mem_write32(NULL, 0x04, entry);
    mem_write32(NULL, 32 * 4, 0x00000200);
    mem_write16(NULL, 0x200, 0x4AC8);

    cf_init(&cpu, mem_read8, mem_read16, mem_read32,
            mem_write8, mem_write16, mem_write32, NULL);

    t0 = now_seconds();
    for (r = 0; r < reps; r++) {
        cf_reset(&cpu);
        while (steps < MAX_STEPS) {
            if (cf_step(&cpu) < 0)
                break;
            steps++;
        }
    }
    t1 = now_seconds();

    {
        uint32_t addr = elf_symbol_be(argv[1], "bench_results");

        if (addr) {
            int i;

            printf("results:");
            for (i = 0; i < 4; i++)
                printf(" %u", mem_read32(NULL, addr + (uint32_t)i * 4));
            printf("\n");
        }
    }

    printf("coldfire  %ld instructions in %.3f s = %.1f MIPS\n", steps,
           t1 - t0, (double)steps / (t1 - t0) / 1e6);

    free(mem);
    return 0;
}
