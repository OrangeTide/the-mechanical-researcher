/* test_harness.c : test runner for the ColdFire V4e emulator */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coldfire.h"
#include "elf_loader.h"

/****************************************************************
 * Flat memory model — 16 MB
 ****************************************************************/

#define MEM_SIZE (16 * 1024 * 1024)

static uint8_t mem[MEM_SIZE];

static uint32_t
mem_read8(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        return mem[addr];
    fprintf(stderr, "read8: out of bounds 0x%08x\n", addr);
    return 0;
}

static uint32_t
mem_read16(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 1 < MEM_SIZE)
        return ((uint32_t)mem[addr] << 8) | mem[addr + 1];
    fprintf(stderr, "read16: out of bounds 0x%08x\n", addr);
    return 0;
}

static uint32_t
mem_read32(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 3 < MEM_SIZE)
        return ((uint32_t)mem[addr] << 24) | ((uint32_t)mem[addr + 1] << 16) |
               ((uint32_t)mem[addr + 2] << 8) | mem[addr + 3];
    fprintf(stderr, "read32: out of bounds 0x%08x\n", addr);
    return 0;
}

static void
mem_write8(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        mem[addr] = val & 0xFF;
    else
        fprintf(stderr, "write8: out of bounds 0x%08x\n", addr);
}

static void
mem_write16(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 1 < MEM_SIZE) {
        mem[addr]     = (val >> 8) & 0xFF;
        mem[addr + 1] = val & 0xFF;
    } else {
        fprintf(stderr, "write16: out of bounds 0x%08x\n", addr);
    }
}

static void
mem_write32(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 3 < MEM_SIZE) {
        mem[addr]     = (val >> 24) & 0xFF;
        mem[addr + 1] = (val >> 16) & 0xFF;
        mem[addr + 2] = (val >> 8) & 0xFF;
        mem[addr + 3] = val & 0xFF;
    } else {
        fprintf(stderr, "write32: out of bounds 0x%08x\n", addr);
    }
}

/****************************************************************
 * ELF loader callback
 ****************************************************************/

static void
elf_write_byte(void *ctx, uint32_t addr, uint8_t byte)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        mem[addr] = byte;
}

/****************************************************************
 * Vector table setup
 ****************************************************************/

static void
setup_vectors(uint32_t entry)
{
    /* Vector 0: Initial SSP — top of RAM */
    mem_write32(NULL, 0x00, MEM_SIZE);

    /* Vector 1: Initial PC — ELF entry point */
    mem_write32(NULL, 0x04, entry);

    /* TRAP #0 (vector 32) — points to a HALT instruction at 0x200 */
    mem_write32(NULL, 32 * 4, 0x00000200);
    mem_write16(NULL, 0x200, 0x4AC8);  /* HALT opcode */
}

/****************************************************************
 * Symbol lookup from results section
 ****************************************************************/

struct test_case {
    const char *name;
    const char *symbol;     /* ELF symbol name */
    uint32_t addr;          /* filled in after symbol lookup */
    uint32_t expected;
};

/****************************************************************
 * ELF symbol table lookup
 ****************************************************************/

/* Read big-endian uint16/uint32 from buffer */
static uint16_t
be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t
be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/* Look up symbol addresses in the ELF file.
 * Fills in tests[].addr for each test whose symbol is found.
 * Returns number of symbols resolved. */
static int
resolve_symbols(const char *path, struct test_case *tests, int ntests)
{
    FILE *f;
    uint8_t ehdr[52];
    uint32_t shoff, shentsize, shnum;
    uint8_t *shdrs = NULL;
    uint8_t *symtab = NULL, *strtab = NULL;
    uint32_t symtab_off = 0, symtab_sz = 0, symtab_entsz = 0;
    uint32_t strtab_off = 0, strtab_sz = 0;
    uint32_t symtab_link = 0;
    int resolved = 0;
    uint16_t i;

    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fread(ehdr, 1, 52, f) != 52) {
        fclose(f);
        return 0;
    }

    shoff     = be32(ehdr + 32);
    shentsize = be16(ehdr + 46);
    shnum     = be16(ehdr + 48);

    if (shoff == 0 || shnum == 0) {
        fclose(f);
        return 0;
    }

    /* Read all section headers */
    shdrs = malloc(shnum * shentsize);
    if (!shdrs) {
        fclose(f);
        return 0;
    }
    fseek(f, shoff, SEEK_SET);
    if (fread(shdrs, shentsize, shnum, f) != shnum) {
        free(shdrs);
        fclose(f);
        return 0;
    }

    /* Find SHT_SYMTAB (type 2) */
    for (i = 0; i < shnum; i++) {
        const uint8_t *sh = shdrs + (uint32_t)i * shentsize;
        if (be32(sh + 4) == 2) { /* SHT_SYMTAB */
            symtab_off  = be32(sh + 16);
            symtab_sz   = be32(sh + 20);
            symtab_entsz = be32(sh + 36);
            symtab_link = be32(sh + 24); /* linked strtab section */
            break;
        }
    }

    if (symtab_off == 0 || symtab_entsz == 0) {
        free(shdrs);
        fclose(f);
        return 0;
    }

    /* Read the linked string table */
    {
        const uint8_t *strsh = shdrs + symtab_link * shentsize;
        strtab_off = be32(strsh + 16);
        strtab_sz  = be32(strsh + 20);
    }

    symtab = malloc(symtab_sz);
    strtab = malloc(strtab_sz);
    if (!symtab || !strtab) {
        free(symtab);
        free(strtab);
        free(shdrs);
        fclose(f);
        return 0;
    }

    fseek(f, symtab_off, SEEK_SET);
    if (fread(symtab, 1, symtab_sz, f) != symtab_sz) {
        free(symtab);
        free(strtab);
        free(shdrs);
        fclose(f);
        return 0;
    }
    fseek(f, strtab_off, SEEK_SET);
    if (fread(strtab, 1, strtab_sz, f) != strtab_sz) {
        free(symtab);
        free(strtab);
        free(shdrs);
        fclose(f);
        return 0;
    }
    fclose(f);

    /* Look up each test symbol */
    {
        uint32_t nsyms = symtab_sz / symtab_entsz;
        uint32_t si;
        int ti;

        for (si = 0; si < nsyms; si++) {
            const uint8_t *sym = symtab + si * symtab_entsz;
            uint32_t name_idx = be32(sym + 0);
            uint32_t value    = be32(sym + 4);

            if (name_idx == 0 || name_idx >= strtab_sz)
                continue;

            for (ti = 0; ti < ntests; ti++) {
                if (tests[ti].addr == 0 &&
                    strcmp((const char *)strtab + name_idx,
                           tests[ti].symbol) == 0) {
                    tests[ti].addr = value;
                    resolved++;
                    break;
                }
            }
        }
    }

    free(symtab);
    free(strtab);
    free(shdrs);
    return resolved;
}

/****************************************************************
 * Main
 ****************************************************************/

int
main(int argc, char **argv)
{
    cf_cpu cpu;
    uint32_t entry;
    int executed, passed, failed, i, resolved;
    const char *elf_path;

    struct test_case tests[] = {
        { "fibonacci(10)", "result_fib",    0, 55 },
        { "gcd(252, 105)", "result_gcd",    0, 21 },
        { "sum_to(100)",   "result_sum",    0, 5050 },
        { "bit_test(0xAB)","result_bits",   0, 0x0A55 },
        { "sqrt(2)*1000",  "result_sqrt_i", 0, 1414 },
    };
    int ntests = sizeof(tests) / sizeof(tests[0]);

    if (argc < 2) {
        fprintf(stderr, "usage: %s <test_program.elf>\n", argv[0]);
        return 1;
    }
    elf_path = argv[1];

    /* Clear memory */
    memset(mem, 0, MEM_SIZE);

    /* Load ELF */
    entry = elf_load(elf_path, elf_write_byte, NULL);
    if (entry == 0) {
        fprintf(stderr, "Failed to load ELF: %s\n", elf_path);
        return 1;
    }
    printf("Loaded ELF: entry=0x%08x\n", entry);

    /* Resolve result symbol addresses from ELF */
    resolved = resolve_symbols(elf_path, tests, ntests);
    printf("Resolved %d/%d result symbols\n", resolved, ntests);
    for (i = 0; i < ntests; i++) {
        if (tests[i].addr == 0)
            fprintf(stderr, "Warning: symbol '%s' not found\n",
                    tests[i].symbol);
        else
            printf("  %s @ 0x%08x\n", tests[i].symbol, tests[i].addr);
    }

    /* Set up vector table */
    setup_vectors(entry);

    /* Initialize and reset CPU */
    cf_init(&cpu, mem_read8, mem_read16, mem_read32,
            mem_write8, mem_write16, mem_write32, NULL);
    cf_reset(&cpu);

    printf("PC=0x%08x  SP=0x%08x\n", cf_get_pc(&cpu), cf_get_a(&cpu, 7));

    /* Run up to 10 million instructions */
    printf("Running...\n");
    executed = cf_run(&cpu, 10000000);
    printf("Executed %d instructions (%llu cycles)\n",
           executed, (unsigned long long)cpu.cycles);
    printf("Final PC=0x%08x  SR=0x%04x\n", cf_get_pc(&cpu), cf_get_sr(&cpu));

    /* Check results */
    printf("\n--- Test Results ---\n");
    passed = 0;
    failed = 0;
    for (i = 0; i < ntests; i++) {
        uint32_t actual = mem_read32(NULL, tests[i].addr);
        int ok = (actual == tests[i].expected);
        printf("  %-20s @ 0x%08x: got %-10u expected %-10u %s\n",
               tests[i].name, tests[i].addr,
               actual, tests[i].expected,
               ok ? "PASS" : "FAIL");
        if (ok)
            passed++;
        else
            failed++;
    }
    printf("\n%d/%d tests passed", passed, ntests);
    if (failed > 0)
        printf(", %d FAILED", failed);
    printf("\n");

    return failed > 0 ? 1 : 0;
}
