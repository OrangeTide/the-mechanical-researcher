/* test_emac.c : EMAC differential test against QEMU */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

/* The EMAC unit was the one part of this emulator that no independent
 * implementation had ever checked. The instruction suite covers it, but
 * that suite compiles the EMAC group out under -DQEMU_USERMODE because it
 * hand-assembles opcodes QEMU might reject. So EMAC expectations were
 * hand-written against a hand-written implementation, which is how a bug
 * that made every mac.l return zero survived.
 *
 * Nothing here is hand-assembled. GNU as accepts every EMAC mnemonic for
 * -mcpu=5475, so the assembler chooses the encodings and the two models
 * are compared on identical instruction streams.
 *
 * One source, two builds. With -DQEMU_USERMODE it prints the recorded
 * state; otherwise it stores into emac_results for emac_dump to read back
 * after running it on the emulator. The two outputs are diffed.
 */

typedef unsigned int uint32_t;

/****************************************************************
 * Result recording
 *
 * Each case records three words: the low 32 bits of the accumulator, the
 * accumulator extension pair, and MACSR. Between them these cover the
 * full 48-bit value and every flag the unit sets.
 ****************************************************************/

#define MAX_RESULTS 256

#ifdef QEMU_USERMODE

static void
put(const char *s, uint32_t n)
{
    register int d0 __asm__("d0") = 4;          /* SYS_write */
    register int d1 __asm__("d1") = 1;
    register const char *d2 __asm__("d2") = s;
    register uint32_t d3 __asm__("d3") = n;

    __asm__ volatile("trap #0"
                     : : "d"(d0), "d"(d1), "d"(d2), "d"(d3) : "memory");
}

/* One write per line. Splitting a line across several syscalls left the
 * separators fighting with the register-constrained operands, so the line
 * is assembled first and handed over in one piece. */
static char *
hex8(char *p, uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    int i;

    for (i = 7; i >= 0; i--)
        p[i] = hex[(v >> ((7 - i) * 4)) & 0xf];
    return p + 8;
}

static void
record(const char *name, uint32_t acc, uint32_t ext, uint32_t macsr)
{
    char line[32];
    char *p = line;

    (void)name;
    p = hex8(p, acc);
    *p++ = ' ';
    p = hex8(p, ext);
    *p++ = ' ';
    p = hex8(p, macsr);
    *p++ = '\n';
    put(line, (uint32_t)(p - line));
}

#else   /* bare metal: store for emac_dump to read back */

volatile uint32_t emac_results[MAX_RESULTS * 3]
    __attribute__((section(".results")));
volatile uint32_t emac_count __attribute__((section(".results"))) = 0;

static void
record(const char *name, uint32_t acc, uint32_t ext, uint32_t macsr)
{
    (void)name;
    if (emac_count < MAX_RESULTS) {
        emac_results[emac_count * 3 + 0] = acc;
        emac_results[emac_count * 3 + 1] = ext;
        emac_results[emac_count * 3 + 2] = macsr;
        emac_count++;
    }
}

#endif

/****************************************************************
 * Primitives
 ****************************************************************/

#define SET_MACSR(v) __asm__ volatile( \
    "move.l %0, %%d0\n\t" "move.l %%d0, %%macsr" : : "g"(v) : "d0")

#define SET_ACC0(v) __asm__ volatile( \
    "move.l %0, %%d0\n\t" "move.l %%d0, %%acc0" : : "g"(v) : "d0")

#define SET_EXT01(v) __asm__ volatile( \
    "move.l %0, %%d0\n\t" "move.l %%d0, %%accext01" : : "g"(v) : "d0")

static uint32_t
get_acc0(void)
{
    uint32_t v;

    __asm__ volatile("move.l %%acc0, %0" : "=d"(v));
    return v;
}

static uint32_t
get_ext01(void)
{
    uint32_t v;

    __asm__ volatile("move.l %%accext01, %0" : "=d"(v));
    return v;
}

static uint32_t
get_macsr(void)
{
    uint32_t v;

    __asm__ volatile("move.l %%macsr, %0" : "=d"(v));
    return v;
}

static void
snapshot(const char *name)
{
    record(name, get_acc0(), get_ext01(), get_macsr());
}

/* MACSR bits */
#define OMC 0x80        /* saturate on overflow */
#define SU  0x40        /* S/U: set selects unsigned, clear selects signed */
#define SIGNED   0
#define UNSIGNED SU
#define FI  0x20        /* fractional */
#define RT  0x10        /* round */

/****************************************************************
 * Cases
 ****************************************************************/

/* Accumulate a large product repeatedly, so the 48-bit accumulator
 * overflows. With OMC set the result must pin at the 48-bit limit; with
 * it clear the result wraps and only the flags record the overflow. */
static void
saturation_positive(int omc)
{
    int i;

    SET_MACSR((omc ? OMC : 0) | SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    for (i = 0; i < 6; i++) {
        __asm__ volatile(
            "move.l #0x40000000, %%d2\n\t"
            "move.l #0x00010000, %%d3\n\t"
            "mac.l %%d2, %%d3, %%acc0\n\t"
            : : : "d2", "d3");
        snapshot(omc ? "sat+omc" : "sat+wrap");
    }
}

/* The same downwards, so the accumulator runs past the negative limit. */
static void
saturation_negative(int omc)
{
    int i;

    SET_MACSR((omc ? OMC : 0) | SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    for (i = 0; i < 6; i++) {
        __asm__ volatile(
            "move.l #0x40000000, %%d2\n\t"
            "move.l #0x00010000, %%d3\n\t"
            "msac.l %%d2, %%d3, %%acc0\n\t"
            : : : "d2", "d3");
        snapshot(omc ? "sat-omc" : "sat-wrap");
    }
}

/* Unsigned mode: the same magnitudes mean something different when the
 * operands are not sign-extended. */
static void
unsigned_mode(int omc)
{
    int i;

    SET_MACSR((omc ? OMC : 0) | UNSIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    for (i = 0; i < 6; i++) {
        __asm__ volatile(
            "move.l #0xC0000000, %%d2\n\t"
            "move.l #0x00010000, %%d3\n\t"
            "mac.l %%d2, %%d3, %%acc0\n\t"
            : : : "d2", "d3");
        snapshot(omc ? "uns+omc" : "uns+wrap");
    }
}

/* Scale factors shift the product before it is accumulated, which moves
 * where the overflow boundary falls. */
static void
scaled(void)
{
    int i;

    SET_MACSR(OMC | SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    for (i = 0; i < 4; i++) {
        __asm__ volatile(
            "move.l #0x20000000, %%d2\n\t"
            "move.l #0x00020000, %%d3\n\t"
            "mac.l %%d2, %%d3, <<, %%acc0\n\t"
            : : : "d2", "d3");
        snapshot("scale<<");
    }

    SET_MACSR(OMC | SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    for (i = 0; i < 4; i++) {
        __asm__ volatile(
            "move.l #0x40000000, %%d2\n\t"
            "move.l #0x00040000, %%d3\n\t"
            "mac.l %%d2, %%d3, >>, %%acc0\n\t"
            : : : "d2", "d3");
        snapshot("scale>>");
    }
}

/* Seed the accumulator near a limit and step over it, which reaches the
 * boundary in one operation rather than by repetition. */
static void
boundary(void)
{
    SET_MACSR(OMC | SIGNED);
    SET_ACC0(0x7fffffff);
    SET_EXT01(0x00007fff);              /* accumulator near the 48-bit max */
    __asm__ volatile(
        "move.l #2, %%d2\n\t"
        "move.l #2, %%d3\n\t"
        "mac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d2", "d3");
    snapshot("edge+");

    SET_MACSR(OMC | SIGNED);
    SET_ACC0(0);
    SET_EXT01(0x00008000);              /* near the 48-bit min */
    __asm__ volatile(
        "move.l #2, %%d2\n\t"
        "move.l #2, %%d3\n\t"
        "msac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d2", "d3");
    snapshot("edge-");
}

/* Small values, where nothing overflows and the flags should simply
 * follow the result. Split by case so the ones qemu models can be
 * compared against it and the ones it does not can still be run. */
static void
positive_product(void)
{
    SET_MACSR(SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    __asm__ volatile(
        "move.l #3, %%d2\n\t"
        "move.l #5, %%d3\n\t"
        "mac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d2", "d3");
    snapshot("pos");
}

/* Needs msac.l, which qemu treats as an add. */
static void
negative_product(void)
{
    SET_MACSR(SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    __asm__ volatile(
        "move.l #3, %%d2\n\t"
        "move.l #5, %%d3\n\t"
        "msac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d2", "d3");
    snapshot("neg");
}

/* Also msac.l: 15 - 3*5 should leave zero with Z set. */
static void
zero_result(void)
{
    SET_MACSR(SIGNED);
    SET_ACC0(15);
    SET_EXT01(0);
    __asm__ volatile(
        "move.l #3, %%d2\n\t"
        "move.l #5, %%d3\n\t"
        "msac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d2", "d3");
    snapshot("zero");
}

/* MASK must not reach a register operand. This is the bug that started
 * all of this, and it is one qemu can arbitrate. */
static void
masked_operands(void)
{
    SET_MACSR(SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    __asm__ volatile(
        "move.l #0xFFFF0000, %%d0\n\t"
        "move.l %%d0, %%mask\n\t"
        "move.l #3, %%d2\n\t"
        "move.l #5, %%d3\n\t"
        "mac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d0", "d2", "d3");
    snapshot("masked");
}

/* Repeated accumulation that stays inside 32 bits, so the running sum is
 * checked without depending on the extension words. */
static void
accumulate_small(void)
{
    int i;

    SET_MACSR(SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    for (i = 0; i < 5; i++) {
        __asm__ volatile(
            "move.l #1234, %%d2\n\t"
            "move.l #5678, %%d3\n\t"
            "mac.l %%d2, %%d3, %%acc0\n\t"
            : : : "d2", "d3");
        snapshot("accum");
    }
}

/* All four accumulators, to check the index decode and that the
 * per-accumulator overflow flags land in the right bit. */
static void
accumulators(void)
{
    SET_MACSR(OMC | SIGNED);
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.l %%d0, %%acc0\n\t"
        "move.l %%d0, %%acc1\n\t"
        "move.l %%d0, %%acc2\n\t"
        "move.l %%d0, %%acc3\n\t"
        "move.l #7, %%d2\n\t"
        "move.l #11, %%d3\n\t"
        "mac.l %%d2, %%d3, %%acc0\n\t"
        "mac.l %%d2, %%d3, %%acc1\n\t"
        "mac.l %%d2, %%d3, %%acc1\n\t"
        "mac.l %%d2, %%d3, %%acc2\n\t"
        "mac.l %%d2, %%d3, %%acc2\n\t"
        "mac.l %%d2, %%d3, %%acc2\n\t"
        : : : "d0", "d2", "d3");

    {
        uint32_t a0, a1, a2, a3, m;

        __asm__ volatile("move.l %%acc0, %0" : "=d"(a0));
        __asm__ volatile("move.l %%acc1, %0" : "=d"(a1));
        __asm__ volatile("move.l %%acc2, %0" : "=d"(a2));
        __asm__ volatile("move.l %%acc3, %0" : "=d"(a3));
        m = get_macsr();
        record("acc0", a0, 0, m);
        record("acc1", a1, 0, m);
        record("acc2", a2, 0, m);
        record("acc3", a3, 0, m);
    }
}

/* movclr reads an accumulator and clears it in one instruction. */
static void
move_clear(void)
{
    uint32_t before, after;

    SET_MACSR(SIGNED);
    SET_ACC0(0);
    SET_EXT01(0);
    __asm__ volatile(
        "move.l #6, %%d2\n\t"
        "move.l #7, %%d3\n\t"
        "mac.l %%d2, %%d3, %%acc0\n\t"
        : : : "d2", "d3");
    __asm__ volatile("movclr.l %%acc0, %0" : "=d"(before));
    after = get_acc0();
    record("movclr", before, after, get_macsr());
}

/****************************************************************
 * Entry
 ****************************************************************/

/* Cases qemu-m68k models faithfully, and so can arbitrate.
 *
 * Its cfv4e EMAC is partial. Measured against -cpu cfv4e: mac.l and its
 * accumulation are right, but msac.l adds where it should subtract, and
 * a product that overflows 32 bits leaves accext01 at zero instead of
 * carrying into the 48-bit accumulator. Anything depending on either is
 * therefore excluded from the comparison rather than being compared
 * against a model that does not implement it. */
static void
run_comparable(void)
{
    positive_product();
    masked_operands();
    accumulate_small();
    accumulators();
    move_clear();
}

/* Everything, including what only this emulator implements. Run as a
 * regression baseline against a recorded golden file, not against qemu. */
static void
run_all(void)
{
    run_comparable();
    negative_product();
    zero_result();
    saturation_positive(1);
    saturation_positive(0);
    saturation_negative(1);
    saturation_negative(0);
    unsigned_mode(1);
    unsigned_mode(0);
    scaled();
    boundary();
}

#ifdef QEMU_USERMODE

void
_start(void)
{
#ifdef EMAC_COMPARABLE
    run_comparable();
#else
    run_all();
#endif
    {
        register int d0 __asm__("d0") = 1;      /* SYS_exit */
        register int d1 __asm__("d1") = 0;

        __asm__ volatile("trap #0" : : "d"(d0), "d"(d1));
    }
    for (;;)
        ;
}

#else

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
#ifdef EMAC_COMPARABLE
    run_comparable();
#else
    run_all();
#endif
    __asm__ volatile("trap #0");
    for (;;)
        ;
}

#endif
