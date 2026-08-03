/* emac_dump.c : runs test_emac.elf and prints what the EMAC produced */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

/* The counterpart to running the same program under qemu-m68k. Both print
 * one line per recorded state, so the two can simply be diffed. Only the
 * numbers are printed, not the case names, because the bare-metal build
 * has no console to carry a name through. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coldfire.h"
#include "elf_loader.h"

#define MEM_SIZE  (16 * 1024 * 1024)
#define MAX_STEPS 100000000

static uint8_t *mem;

/****************************************************************
 * Big-endian memory bus
 ****************************************************************/

static uint32_t
r8(void *ctx, uint32_t a)
{
    (void)ctx;
    return a < MEM_SIZE ? mem[a] : 0;
}

static uint32_t
r16(void *ctx, uint32_t a)
{
    (void)ctx;
    if (a + 1 >= MEM_SIZE)
        return 0;
    return ((uint32_t)mem[a] << 8) | mem[a + 1];
}

static uint32_t
r32(void *ctx, uint32_t a)
{
    (void)ctx;
    if (a + 3 >= MEM_SIZE)
        return 0;
    return ((uint32_t)mem[a] << 24) | ((uint32_t)mem[a + 1] << 16) |
           ((uint32_t)mem[a + 2] << 8) | mem[a + 3];
}

static void
w8(void *ctx, uint32_t a, uint32_t v)
{
    (void)ctx;
    if (a < MEM_SIZE)
        mem[a] = (uint8_t)v;
}

static void
w16(void *ctx, uint32_t a, uint32_t v)
{
    (void)ctx;
    if (a + 1 < MEM_SIZE) {
        mem[a] = (uint8_t)(v >> 8);
        mem[a + 1] = (uint8_t)v;
    }
}

static void
w32(void *ctx, uint32_t a, uint32_t v)
{
    (void)ctx;
    if (a + 3 < MEM_SIZE) {
        mem[a] = (uint8_t)(v >> 24);
        mem[a + 1] = (uint8_t)(v >> 16);
        mem[a + 2] = (uint8_t)(v >> 8);
        mem[a + 3] = (uint8_t)v;
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
 * Big-endian ELF symbol lookup
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
 * Main
 ****************************************************************/

int
main(int argc, char **argv)
{
    cf_cpu cpu;
    uint32_t entry, results, count_addr, count, i;
    long steps = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <test_emac.elf>\n", argv[0]);
        return 2;
    }

    mem = calloc(1, MEM_SIZE);
    if (!mem)
        return 1;

    entry = elf_load(argv[1], poke, NULL);
    if (!entry) {
        free(mem);
        return 1;
    }

    results = elf_symbol_be(argv[1], "emac_results");
    count_addr = elf_symbol_be(argv[1], "emac_count");
    if (!results || !count_addr) {
        fprintf(stderr, "emac_dump: %s has no result symbols\n", argv[1]);
        free(mem);
        return 1;
    }

    /* Reset vector and a HALT behind TRAP #0, as the other runners use */
    w32(NULL, 0x00, MEM_SIZE);
    w32(NULL, 0x04, entry);
    w32(NULL, 32 * 4, 0x00000200);
    w16(NULL, 0x200, 0x4AC8);

    cf_init(&cpu, r8, r16, r32, w8, w16, w32, NULL);
    cf_reset(&cpu);
    while (steps < MAX_STEPS) {
        if (cf_step(&cpu) < 0)
            break;
        steps++;
    }

    count = r32(NULL, count_addr);
    for (i = 0; i < count; i++) {
        printf("%08x %08x %08x\n",
               r32(NULL, results + i * 12 + 0),
               r32(NULL, results + i * 12 + 4),
               r32(NULL, results + i * 12 + 8));
    }

    free(mem);
    return 0;
}
