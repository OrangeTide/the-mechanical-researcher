/* bench_rv32.c : times the RV32 emulator on the shared benchmark */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rv32.h"
#include "machine.h"
#include "elf_loader.h"

#define MAX_STEPS 2000000000

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
    rv_cpu cpu;
    machine m;
    uint32_t entry, addr;
    double t0, t1;
    long steps = 0;
    int reps = 1, r;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <bench.elf> [reps]\n", argv[0]);
        return 2;
    }
    if (argc > 2)
        reps = atoi(argv[2]);

    if (machine_init(&m, &cpu))
        return 1;
    m.quiet = 1;

    entry = elf_load(argv[1], machine_poke, &m);
    if (!entry) {
        machine_free(&m);
        return 1;
    }

    t0 = now_seconds();
    for (r = 0; r < reps; r++) {
        rv_reset(&cpu, entry);
        m.exited = 0;
        while (steps < MAX_STEPS) {
            if (rv_step(&cpu) < 0)
                break;
            steps++;
        }
    }
    t1 = now_seconds();

    addr = elf_symbol(argv[1], "bench_results");
    if (addr) {
        int i;

        printf("results:");
        for (i = 0; i < 4; i++) {
            uint32_t v = (uint32_t)m.mem[addr + i * 4] |
                         ((uint32_t)m.mem[addr + i * 4 + 1] << 8) |
                         ((uint32_t)m.mem[addr + i * 4 + 2] << 16) |
                         ((uint32_t)m.mem[addr + i * 4 + 3] << 24);

            printf(" %u", v);
        }
        printf("\n");
    }

    printf("rv32      %ld instructions in %.3f s = %.1f MIPS\n", steps,
           t1 - t0, (double)steps / (t1 - t0) / 1e6);

    machine_free(&m);
    return 0;
}
