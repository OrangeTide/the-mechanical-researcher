/* bench.c : identical workload for the RV32 and ColdFire emulators */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

/* One source file, two targets. Comparing two emulators is only meaningful
 * if the guests are doing the same work, so the same C is cross-compiled
 * for ColdFire V4e and for RV32IMFC and run on the respective emulator.
 * Only the entry and exit sequences differ, because the two have different
 * ways of saying "the program is finished". */

typedef unsigned int u32;
typedef int i32;

/****************************************************************
 * Workload
 ****************************************************************/

/* Recursion, call and return, conditional branches */
static u32
fib(u32 n)
{
    if (n <= 1)
        return n;
    return fib(n - 1) + fib(n - 2);
}

/* Hardware divide and remainder in a tight loop */
static u32
gcd_sum(u32 rounds)
{
    u32 acc = 0;
    u32 i;

    for (i = 1; i <= rounds; i++) {
        u32 a = i * 7919u;
        u32 b = i * 104729u;

        while (b != 0) {
            u32 t = a % b;
            a = b;
            b = t;
        }
        acc += a;
    }
    return acc;
}

/* Shifts and bitwise operations, which every architecture does well */
static u32
bitmix(u32 rounds)
{
    u32 h = 0x811c9dc5u;
    u32 i;

    for (i = 0; i < rounds; i++) {
        h ^= i;
        h = (h << 13) | (h >> 19);
        h += h << 3;
        h ^= h >> 7;
        h &= 0x7fffffffu;
    }
    return h;
}

/* Single-precision arithmetic: add, multiply, divide and a comparison in
 * every iteration. Newton's method rather than a library square root, so
 * that the two targets execute the same operations rather than whatever
 * their runtime happens to provide. */
static u32
float_work(u32 rounds)
{
    float acc = 0.0f;
    u32 i;

    for (i = 1; i <= rounds; i++) {
        float x = (float)i;
        float g = x;
        int k;

        for (k = 0; k < 6; k++)
            g = (g + x / g) * 0.5f;
        acc += g;
        if (acc > 1.0e30f)
            acc = 1.0f;
    }
    return (u32)acc;
}

static u32 result_fib, result_gcd, result_bits, result_float;

/* Building with -DBENCH_PART=n runs a single phase, which is how the
 * per-workload instruction counts in the article were measured. Without
 * it every phase runs. */
#ifndef BENCH_PART
#define BENCH_PART 0
#endif

static void
run_workload(void)
{
#if BENCH_PART == 0 || BENCH_PART == 1
    result_fib = fib(21);
#endif
#if BENCH_PART == 0 || BENCH_PART == 2
    result_gcd = gcd_sum(400);
#endif
#if BENCH_PART == 0 || BENCH_PART == 3
    result_bits = bitmix(20000);
#endif
#if BENCH_PART == 0 || BENCH_PART == 4
    result_float = float_work(2000);
#endif
}

/****************************************************************
 * Target-specific entry and exit
 ****************************************************************/

#ifdef __riscv

volatile u32 bench_results[4] __attribute__((section(".results")));

int
main(void)
{
    run_workload();
    bench_results[0] = result_fib;
    bench_results[1] = result_gcd;
    bench_results[2] = result_bits;
    bench_results[3] = result_float;
    return 0;
}

#else   /* ColdFire */

volatile u32 bench_results[4] __attribute__((section(".results")));

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    run_workload();
    bench_results[0] = result_fib;
    bench_results[1] = result_gcd;
    bench_results[2] = result_bits;
    bench_results[3] = result_float;

    __asm__ volatile("trap #0");
    for (;;)
        ;
}

#endif
