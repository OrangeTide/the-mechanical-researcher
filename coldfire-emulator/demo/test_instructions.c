/* test_instructions.c — comprehensive ColdFire V4e instruction test
 * Tests every instruction the assembler can generate for -mcpu=5475.
 *
 * Build (emulator):
 *   m68k-linux-gnu-gcc -mcpu=5475 -O0 -nostdlib -static -ffreestanding \
 *       -T link.ld -o test_instructions.elf test_instructions.c
 * Build (QEMU):
 *   m68k-linux-gnu-gcc -mcpu=5475 -O0 -nostdlib -static -ffreestanding \
 *       -DQEMU_USERMODE -o test_instructions_qemu test_instructions.c
 *
 * Run (emulator):  ./test_harness test_instructions.elf
 * Run (QEMU):      qemu-m68k -cpu cfv4e ./test_instructions_qemu
 *
 * Notes:
 *   ROR/ROL and EXG are implemented in the emulator but not in the
 *   ColdFire V4e ISA — GAS rejects them with -mcpu=5475. They are
 *   tested only via hand-assembled opcode words (see test_legacy).
 *
 * Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain
 * Written with AI assistance (Claude, Anthropic)
 */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long long uint64_t;

/* ── Output infrastructure ────────────────────────────────── */

#ifdef QEMU_USERMODE

static int
sys_write(int fd, const char *buf, int len)
{
    register int d0 __asm__("d0") = 4;
    register int d1 __asm__("d1") = fd;
    register const char *d2 __asm__("d2") = buf;
    register int d3 __asm__("d3") = len;
    __asm__ volatile("trap #0"
        : "+d"(d0) : "d"(d1), "d"(d2), "d"(d3) : "memory");
    return d0;
}

static void __attribute__((noreturn))
sys_exit(int code)
{
    register int d0 __asm__("d0") = 1;
    register int d1 __asm__("d1") = code;
    __asm__ volatile("trap #0" : : "d"(d0), "d"(d1));
    __builtin_unreachable();
}

static int
my_strlen(const char *s)
{
    int n = 0;
    while (*s++) n++;
    return n;
}

static void print_str(const char *s) { sys_write(1, s, my_strlen(s)); }

static void
print_uint(uint32_t v)
{
    char buf[12];
    int i = 0;
    if (v == 0) { sys_write(1, "0", 1); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    { int a = 0, b = i - 1;
      while (a < b) { char t = buf[a]; buf[a] = buf[b]; buf[b] = t; a++; b--; } }
    sys_write(1, buf, i);
}

#endif /* QEMU_USERMODE */

/* ── Test infrastructure ──────────────────────────────────── */

static uint32_t g_pass, g_fail;
#ifdef QEMU_USERMODE
static uint32_t g_gp, g_gf;
static const char *g_gn;
#define CHECK(expr) do { if (expr) { g_pass++; g_gp++; } \
    else { g_fail++; g_gf++; } } while (0)
#define GROUP_BEGIN(n) do { g_gp = 0; g_gf = 0; g_gn = n; } while(0)
#define GROUP_END() do { print_str("  "); print_str(g_gn); print_str(": "); \
    print_uint(g_gp); print_str("/"); print_uint(g_gp + g_gf); \
    print_str(g_gf ? " FAIL" : " ok"); print_str("\n"); } while(0)
#else
#define CHECK(expr) do { if (expr) g_pass++; else g_fail++; } while (0)
#define GROUP_BEGIN(n) ((void)0)
#define GROUP_END() ((void)0)
#endif

/* Scratch memory for addressing mode tests */
static volatile uint32_t scratch[16];

/* Results — read by test_harness via ELF symbol lookup */
volatile uint32_t result_pass  __attribute__((section(".results"))) = 0;
volatile uint32_t result_fail  __attribute__((section(".results"))) = 0;


/* ── Data movement: MOVE.B/W/L, MOVEA ────────────────────── */

static void
test_move(void)
{
    uint32_t r;

    GROUP_BEGIN("move");

    /* MOVE.L #imm, Dn */
    __asm__ volatile(
        "move.l #0x12345678, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x12345678);

    /* MOVE.L Dn, Dn */
    __asm__ volatile(
        "move.l #0xAABBCCDD, %%d0\n\t"
        "move.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xAABBCCDD);

    /* MOVE.L Dn, (An) — store to memory */
    scratch[0] = 0;
    __asm__ volatile(
        "move.l #0xDEADBEEF, %%d0\n\t"
        "lea %0, %%a0\n\t"
        "move.l %%d0, (%%a0)"
        : "=m"(scratch[0]) : : "d0", "a0");
    CHECK(scratch[0] == 0xDEADBEEF);

    /* MOVE.L (An), Dn — load from memory */
    scratch[0] = 0xCAFEBABE;
    __asm__ volatile(
        "lea %1, %%a0\n\t"
        "move.l (%%a0), %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : "m"(scratch[0]) : "d0", "a0");
    CHECK(r == 0xCAFEBABE);

    /* MOVE.L (An)+, Dn — post-increment */
    scratch[0] = 0x11111111;
    scratch[1] = 0x22222222;
    __asm__ volatile(
        "lea %2, %%a0\n\t"
        "move.l (%%a0)+, %%d0\n\t"
        "move.l (%%a0)+, %%d1\n\t"
        "move.l %%d0, %0\n\t"
        "move.l %%d1, %1"
        : "=m"(r), "=m"(scratch[4])
        : "m"(scratch[0]) : "d0", "d1", "a0");
    CHECK(r == 0x11111111);
    CHECK(scratch[4] == 0x22222222);

    /* MOVE.L Dn, -(An) — pre-decrement */
    scratch[2] = 0;
    __asm__ volatile(
        "lea %0, %%a0\n\t"
        "addq.l #4, %%a0\n\t"
        "move.l #0x99887766, %%d0\n\t"
        "move.l %%d0, -(%%a0)"
        : "=m"(scratch[2]) : : "d0", "a0");
    CHECK(scratch[2] == 0x99887766);

    /* MOVE.L (d16, An), Dn — displacement */
    scratch[0] = 0xAAAAAAAA;
    scratch[3] = 0xBBBBBBBB;
    __asm__ volatile(
        "lea %1, %%a0\n\t"
        "move.l 12(%%a0), %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : "m"(scratch[0]) : "d0", "a0");
    CHECK(r == 0xBBBBBBBB);

    /* MOVE.W #imm, Dn */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.w #0x1234, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK((r & 0xFFFF) == 0x1234);

    /* MOVE.B #imm, Dn */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.b #0xAB, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK((r & 0xFF) == 0xAB);

    /* MOVEA.L Dn, An */
    __asm__ volatile(
        "move.l #0x00020000, %%d0\n\t"
        "movea.l %%d0, %%a0\n\t"
        "move.l %%a0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1", "a0");
    CHECK(r == 0x00020000);

    /* MOVEA.L #imm, An */
    __asm__ volatile(
        "movea.l #0x00030000, %%a0\n\t"
        "move.l %%a0, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "a0");
    CHECK(r == 0x00030000);

    /* MOVE.L An, Dn */
    __asm__ volatile(
        "movea.l #0x00040000, %%a1\n\t"
        "move.l %%a1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "a1");
    CHECK(r == 0x00040000);

    GROUP_END();
}

/* ── MOVEQ, MVS, MVZ, MOV3Q ──────────────────────────────── */

static void
test_moveq_mvs_mvz(void)
{
    uint32_t r;

    GROUP_BEGIN("moveq/mvs/mvz/mov3q");

    /* MOVEQ positive */
    __asm__ volatile("moveq #42, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 42);

    /* MOVEQ negative — sign-extends to 32 bits */
    __asm__ volatile("moveq #-1, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFFFFFF);

    /* MOVEQ zero */
    __asm__ volatile("moveq #0, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0);

    /* MOVEQ max positive (127) */
    __asm__ volatile("moveq #127, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 127);

    /* MOVEQ min negative (-128) */
    __asm__ volatile("moveq #-128, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFFFF80);

    /* MVZ.W — zero-extend word to long */
    __asm__ volatile(
        "move.l #0xFFFF8000, %%d0\n\t"
        "mvz.w %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x00008000);

    /* MVZ.B — zero-extend byte to long */
    __asm__ volatile(
        "move.l #0xFFFFFF80, %%d0\n\t"
        "mvz.b %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x00000080);

    /* MVS.W — sign-extend word to long (negative) */
    __asm__ volatile(
        "move.l #0x00008000, %%d0\n\t"
        "mvs.w %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xFFFF8000);

    /* MVS.B — sign-extend byte to long (negative) */
    __asm__ volatile(
        "move.l #0x00000080, %%d0\n\t"
        "mvs.b %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xFFFFFF80);

    /* MVS.W — positive (no sign extension) */
    __asm__ volatile(
        "move.l #0xFFFF0042, %%d0\n\t"
        "mvs.w %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x00000042);

    /* MOV3Q #imm, Dn — small immediate (-1, 1–7) */
    __asm__ volatile("mov3q.l #1, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 1);

    __asm__ volatile("mov3q.l #7, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 7);

    __asm__ volatile("mov3q.l #-1, %%d0\n\t" "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFFFFFF);

    GROUP_END();
}

/* ── Immediate: ORI, ANDI, SUBI, ADDI, EORI ──────────────── */

static void
test_immediate(void)
{
    uint32_t r;

    GROUP_BEGIN("immediate");

    /* ORI.L */
    __asm__ volatile(
        "move.l #0x00FF0000, %%d0\n\t"
        "ori.l #0x0000FF00, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x00FFFF00);

    /* ORI.L with overlap */
    __asm__ volatile(
        "move.l #0xFF00FF00, %%d0\n\t"
        "ori.l #0x00FF00FF, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFFFFFF);

    /* ANDI.L */
    __asm__ volatile(
        "move.l #0xFFFF0000, %%d0\n\t"
        "andi.l #0xFF00FF00, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFF000000);

    /* ANDI.L mask low byte */
    __asm__ volatile(
        "move.l #0x12345678, %%d0\n\t"
        "andi.l #0x000000FF, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x00000078);

    /* SUBI.L */
    __asm__ volatile(
        "move.l #100, %%d0\n\t"
        "subi.l #30, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 70);

    /* ADDI.L */
    __asm__ volatile(
        "move.l #100, %%d0\n\t"
        "addi.l #200, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 300);

    /* EORI.L */
    __asm__ volatile(
        "move.l #0xFF00FF00, %%d0\n\t"
        "eori.l #0xFFFF0000, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x00FFFF00);

    /* EORI.L self-cancel */
    __asm__ volatile(
        "move.l #0xAAAAAAAA, %%d0\n\t"
        "eori.l #0xAAAAAAAA, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0);

    GROUP_END();
}

/* ── Arithmetic: ADD, SUB, ADDA, SUBA, ADDQ, SUBQ, etc. ─── */

static void
test_arithmetic(void)
{
    uint32_t r;

    GROUP_BEGIN("arithmetic");

    /* ADD.L Dn, Dn */
    __asm__ volatile(
        "move.l #100, %%d0\n\t"
        "move.l #200, %%d1\n\t"
        "add.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 300);

    /* ADD.L overflow wraps */
    __asm__ volatile(
        "move.l #0xFFFFFFFF, %%d0\n\t"
        "move.l #1, %%d1\n\t"
        "add.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0);

    /* ADDA.L Dn, An */
    __asm__ volatile(
        "movea.l #0x00010000, %%a0\n\t"
        "move.l #0x100, %%d0\n\t"
        "adda.l %%d0, %%a0\n\t"
        "move.l %%a0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1", "a0");
    CHECK(r == 0x00010100);

    /* ADDQ.L */
    __asm__ volatile(
        "move.l #10, %%d0\n\t"
        "addq.l #8, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 18);

    /* ADDQ.L #1 */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "addq.l #1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 1);

    /* SUB.L Dn, Dn */
    __asm__ volatile(
        "move.l #500, %%d0\n\t"
        "move.l #200, %%d1\n\t"
        "sub.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 300);

    /* SUB.L underflow wraps */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.l #1, %%d1\n\t"
        "sub.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xFFFFFFFF);

    /* SUBA.L */
    __asm__ volatile(
        "movea.l #0x00010100, %%a0\n\t"
        "move.l #0x100, %%d0\n\t"
        "suba.l %%d0, %%a0\n\t"
        "move.l %%a0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1", "a0");
    CHECK(r == 0x00010000);

    /* SUBQ.L */
    __asm__ volatile(
        "move.l #18, %%d0\n\t"
        "subq.l #8, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 10);

    /* NEG.L */
    __asm__ volatile(
        "move.l #42, %%d0\n\t"
        "neg.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == (uint32_t)-42);

    /* NEG.L zero */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "neg.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0);

    /* NEGX.L — negate with extend */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "sub.l %%d0, %%d0\n\t"   /* clear X flag */
        "move.l #10, %%d1\n\t"
        "negx.l %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == (uint32_t)-10);

    /* ADDX.L — add with extend */
    __asm__ volatile(
        "move.l #0xFFFFFFFF, %%d0\n\t"
        "addq.l #1, %%d0\n\t"   /* wraps to 0, sets X=1 */
        "move.l #5, %%d0\n\t"
        "move.l #10, %%d1\n\t"
        "addx.l %%d0, %%d1\n\t" /* 10 + 5 + X(1) = 16 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 16);

    /* SUBX.L — subtract with extend */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "subq.l #1, %%d0\n\t"   /* borrow, sets X=1 */
        "move.l #3, %%d0\n\t"
        "move.l #10, %%d1\n\t"
        "subx.l %%d0, %%d1\n\t" /* 10 - 3 - X(1) = 6 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 6);

    GROUP_END();
}

/* ── Logic: AND, OR, EOR, NOT ─────────────────────────────── */

static void
test_logic(void)
{
    uint32_t r;

    GROUP_BEGIN("logic");

    /* AND.L */
    __asm__ volatile(
        "move.l #0xFF00FF00, %%d0\n\t"
        "move.l #0x0F0F0F0F, %%d1\n\t"
        "and.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x0F000F00);

    /* AND.L zero */
    __asm__ volatile(
        "move.l #0xFFFFFFFF, %%d0\n\t"
        "move.l #0, %%d1\n\t"
        "and.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0);

    /* OR.L */
    __asm__ volatile(
        "move.l #0xFF000000, %%d0\n\t"
        "move.l #0x00FF0000, %%d1\n\t"
        "or.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xFFFF0000);

    /* OR.L identity */
    __asm__ volatile(
        "move.l #0x12345678, %%d0\n\t"
        "move.l #0, %%d1\n\t"
        "or.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x12345678);

    /* EOR.L */
    __asm__ volatile(
        "move.l #0xFF00FF00, %%d0\n\t"
        "move.l #0x0F0F0F0F, %%d1\n\t"
        "eor.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xF00FF00F);

    /* EOR.L self = 0 */
    __asm__ volatile(
        "move.l #0xDEADBEEF, %%d0\n\t"
        "eor.l %%d0, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0);

    /* NOT.L */
    __asm__ volatile(
        "move.l #0x00FF00FF, %%d0\n\t"
        "not.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFF00FF00);

    /* NOT.L zero */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "not.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFFFFFF);

    GROUP_END();
}

/* ── Compare: CMP, CMPA, TST, CMPI (tested via branches) ── */

static void
test_compare(void)
{
    uint32_t r;

    GROUP_BEGIN("compare");

    /* CMP.L equal → BEQ */
    __asm__ volatile(
        "move.l #42, %%d0\n\t"
        "move.l #42, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* CMP.L not equal → BNE */
    __asm__ volatile(
        "move.l #42, %%d0\n\t"
        "move.l #99, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* CMP.L greater → BGT */
    __asm__ volatile(
        "move.l #10, %%d0\n\t"
        "move.l #20, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "bgt 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* CMP.L less → BLT */
    __asm__ volatile(
        "move.l #20, %%d0\n\t"
        "move.l #10, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "blt 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* CMPA.L equal */
    __asm__ volatile(
        "movea.l #0x10000, %%a0\n\t"
        "move.l #0x10000, %%d0\n\t"
        "cmpa.l %%d0, %%a0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1", "a0");
    CHECK(r == 1);

    /* TST.L zero → BEQ */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "tst.l %%d0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* TST.L nonzero → BNE */
    __asm__ volatile(
        "move.l #1, %%d0\n\t"
        "tst.l %%d0\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* TST.L negative → BMI */
    __asm__ volatile(
        "move.l #0x80000000, %%d0\n\t"
        "tst.l %%d0\n\t"
        "bmi 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* CMPI.L */
    __asm__ volatile(
        "move.l #100, %%d0\n\t"
        "cmpi.l #100, %%d0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    GROUP_END();
}

/* ── Multiply and divide ──────────────────────────────────── */

static void
test_muldiv(void)
{
    uint32_t r;

    GROUP_BEGIN("mul/div");

    /* MULU.L unsigned 32×32→32 */
    __asm__ volatile(
        "move.l #100, %%d0\n\t"
        "move.l #200, %%d1\n\t"
        "mulu.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 20000);

    /* MULS.L signed */
    __asm__ volatile(
        "move.l #-10, %%d0\n\t"
        "move.l #5, %%d1\n\t"
        "muls.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == (uint32_t)-50);

    /* MULU.L zero */
    __asm__ volatile(
        "move.l #12345, %%d0\n\t"
        "move.l #0, %%d1\n\t"
        "mulu.l %%d0, %%d1\n\t"
        "move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0);

    /* DIVU.L unsigned 32÷32→32 */
    __asm__ volatile(
        "move.l #100, %%d0\n\t"
        "move.l #7, %%d1\n\t"
        "divu.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 14);

    /* DIVS.L signed */
    __asm__ volatile(
        "move.l #-100, %%d0\n\t"
        "move.l #7, %%d1\n\t"
        "divs.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == (uint32_t)-14);

    /* DIVU.L exact */
    __asm__ volatile(
        "move.l #256, %%d0\n\t"
        "move.l #16, %%d1\n\t"
        "divu.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 16);

    /* REMU.L — use C to generate the instruction */
    {
        volatile uint32_t a = 100, b = 7;
        r = a % b;
    }
    CHECK(r == 2);

    /* REMS.L — use C to generate */
    {
        volatile int32_t a = -100, b = 7;
        r = (uint32_t)(a % b);
    }
    CHECK(r == (uint32_t)-2);

    GROUP_END();
}

/* ── Shifts ───────────────────────────────────────────────── */

static void
test_shift(void)
{
    uint32_t r;

    GROUP_BEGIN("shift");

    /* LSL.L #imm */
    __asm__ volatile(
        "move.l #1, %%d0\n\t"
        "lsl.l #4, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 16);

    /* LSL.L by register */
    __asm__ volatile(
        "move.l #0xFF, %%d0\n\t"
        "move.l #8, %%d1\n\t"
        "lsl.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xFF00);

    /* LSR.L #imm */
    __asm__ volatile(
        "move.l #0xFF000000, %%d0\n\t"
        "lsr.l #8, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x00FF0000);

    /* LSR.L shift out all bits */
    __asm__ volatile(
        "move.l #0x0000FFFF, %%d0\n\t"
        "move.l #16, %%d1\n\t"
        "lsr.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0);

    /* ASL.L #imm */
    __asm__ volatile(
        "move.l #0x10, %%d0\n\t"
        "asl.l #2, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x40);

    /* ASR.L #imm — preserves sign */
    __asm__ volatile(
        "move.l #0x80000000, %%d0\n\t"
        "asr.l #4, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xF8000000);

    /* ASR.L positive — zero fill */
    __asm__ volatile(
        "move.l #0x40000000, %%d0\n\t"
        "asr.l #4, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x04000000);

    /* LSL.L #0 — no shift */
    __asm__ volatile(
        "move.l #0xABCD1234, %%d0\n\t"
        "move.l #0, %%d1\n\t"
        "lsl.l %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0xABCD1234);

    GROUP_END();
}

/* ── Bit operations: BTST, BCHG, BCLR, BSET ──────────────── */

static void
test_bit(void)
{
    uint32_t r;

    GROUP_BEGIN("bit ops");

    /* BTST #imm — bit 0 set → Z clear → BNE taken */
    __asm__ volatile(
        "move.l #0x01, %%d0\n\t"
        "btst #0, %%d0\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* BTST #imm — bit 1 clear → Z set → BEQ taken */
    __asm__ volatile(
        "move.l #0x01, %%d0\n\t"
        "btst #1, %%d0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* BSET #imm */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "bset #7, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x80);

    /* BCLR #imm */
    __asm__ volatile(
        "move.l #0xFF, %%d0\n\t"
        "bclr #3, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xF7);

    /* BCHG #imm — toggle on */
    __asm__ volatile(
        "move.l #0x00, %%d0\n\t"
        "bchg #4, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x10);

    /* BCHG #imm — toggle off */
    __asm__ volatile(
        "move.l #0x10, %%d0\n\t"
        "bchg #4, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x00);

    /* BTST Dn — bit 31 */
    __asm__ volatile(
        "move.l #0x80000000, %%d0\n\t"
        "move.l #31, %%d1\n\t"
        "btst %%d1, %%d0\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* BSET Dn */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.l #15, %%d1\n\t"
        "bset %%d1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x8000);

    GROUP_END();
}

/* ── Branches: Bcc, BRA, BSR/JSR/RTS ─────────────────────── */

static void
test_branch(void)
{
    uint32_t r;

    GROUP_BEGIN("branch");

    /* BRA */
    __asm__ volatile(
        "bra 0f\n\t"
        "moveq #0, %%d0\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d0\n"
        "1: move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 1);

    /* BEQ taken */
    __asm__ volatile(
        "moveq #0, %%d0\n\t"
        "tst.l %%d0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* BEQ not taken */
    __asm__ volatile(
        "moveq #1, %%d0\n\t"
        "tst.l %%d0\n\t"
        "beq 0f\n\t"
        "moveq #1, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #0, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* BNE taken */
    __asm__ volatile(
        "moveq #1, %%d0\n\t"
        "tst.l %%d0\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* BGE — equal case */
    __asm__ volatile(
        "move.l #10, %%d0\n\t"
        "move.l #10, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "bge 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* BLE — less case */
    __asm__ volatile(
        "move.l #20, %%d0\n\t"
        "move.l #10, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "ble 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* BCS — carry set (unsigned <) */
    __asm__ volatile(
        "move.l #10, %%d0\n\t"
        "move.l #5, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "bcs 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* BCC — carry clear (unsigned >=) */
    __asm__ volatile(
        "move.l #5, %%d0\n\t"
        "move.l #10, %%d1\n\t"
        "cmp.l %%d0, %%d1\n\t"
        "bcc 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* BPL — positive */
    __asm__ volatile(
        "moveq #1, %%d0\n\t"
        "tst.l %%d0\n\t"
        "bpl 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* BSR/RTS */
    __asm__ volatile(
        "bsr 0f\n\t"
        "move.l %%d0, %0\n\t"
        "bra 1f\n"
        "0: moveq #42, %%d0\n\t"
        "rts\n"
        "1:"
        : "=m"(r) : : "d0");
    CHECK(r == 42);

    /* JSR/RTS */
    __asm__ volatile(
        "lea 0f(%%pc), %%a0\n\t"
        "jsr (%%a0)\n\t"
        "move.l %%d0, %0\n\t"
        "bra 1f\n"
        "0: moveq #99, %%d0\n\t"
        "rts\n"
        "1:"
        : "=m"(r) : : "d0", "a0");
    CHECK(r == 99);

    GROUP_END();
}

/* ── Misc: CLR, EXT, SWAP, LEA, PEA, LINK/UNLK, Scc, etc. ─ */

static void
test_misc(void)
{
    uint32_t r;

    GROUP_BEGIN("misc");

    /* CLR.L */
    __asm__ volatile(
        "move.l #0xFFFFFFFF, %%d0\n\t"
        "clr.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0);

    /* EXT.W — sign-extend byte to word */
    __asm__ volatile(
        "move.l #0x00000080, %%d0\n\t"
        "ext.w %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK((r & 0xFFFF) == 0xFF80);

    /* EXT.W — positive byte */
    __asm__ volatile(
        "move.l #0x0000007F, %%d0\n\t"
        "ext.w %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK((r & 0xFFFF) == 0x007F);

    /* EXT.L — sign-extend word to long */
    __asm__ volatile(
        "move.l #0x00008000, %%d0\n\t"
        "ext.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFF8000);

    /* EXTB.L — sign-extend byte to long (ColdFire) */
    __asm__ volatile(
        "move.l #0x000000FE, %%d0\n\t"
        "extb.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xFFFFFFFE);

    /* EXTB.L — positive byte */
    __asm__ volatile(
        "move.l #0x00000042, %%d0\n\t"
        "extb.l %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x00000042);

    /* SWAP */
    __asm__ volatile(
        "move.l #0x12345678, %%d0\n\t"
        "swap %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x56781234);

    /* LEA (d16,An), An */
    __asm__ volatile(
        "movea.l #0x10000, %%a0\n\t"
        "lea 0x100(%%a0), %%a1\n\t"
        "move.l %%a1, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "a0", "a1");
    CHECK(r == 0x10100);

    /* LEA (d16,PC), An — verify we get a valid address */
    __asm__ volatile(
        "lea 0f(%%pc), %%a0\n\t"
        "move.l %%a0, %%d0\n\t"
        "bra 1f\n"
        "0: .long 0\n"
        "1: tst.l %%d0\n\t"
        "bne 2f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 3f\n"
        "2: moveq #1, %%d1\n"
        "3: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1", "a0");
    CHECK(r == 1);

    /* PEA */
    __asm__ volatile(
        "movea.l #0x12340000, %%a0\n\t"
        "pea (%%a0)\n\t"
        "move.l (%%sp)+, %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "a0");
    CHECK(r == 0x12340000);

    /* LINK/UNLK */
    __asm__ volatile(
        "link %%a5, #-8\n\t"
        "move.l %%a5, %%d0\n\t"
        "unlk %%a5\n\t"
        "tst.l %%d0\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1", "a5");
    CHECK(r == 1);

    /* Scc: SEQ — set byte if Z */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.l #0, %%d1\n\t"
        "tst.l %%d1\n\t"
        "seq %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK((r & 0xFF) == 0xFF);

    /* Scc: SNE — set byte if !Z (true) */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        "move.l #1, %%d1\n\t"
        "tst.l %%d1\n\t"
        "sne %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK((r & 0xFF) == 0xFF);

    /* Scc: SNE — false case */
    __asm__ volatile(
        "move.l #0xFF, %%d0\n\t"
        "move.l #0, %%d1\n\t"
        "tst.l %%d1\n\t"
        "sne %%d0\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK((r & 0xFF) == 0x00);

    /* NOP */
    __asm__ volatile(
        "moveq #1, %%d0\n\t"
        "nop\n\t"
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 1);

    /* MOVEM.L — save/restore via stack */
    __asm__ volatile(
        "move.l #0x11, %%d2\n\t"
        "move.l #0x22, %%d3\n\t"
        "move.l #0x33, %%d4\n\t"
        "lea -12(%%sp), %%sp\n\t"
        "movem.l %%d2/%%d3/%%d4, (%%sp)\n\t"
        "moveq #0, %%d2\n\t"
        "moveq #0, %%d3\n\t"
        "moveq #0, %%d4\n\t"
        "movem.l (%%sp), %%d2/%%d3/%%d4\n\t"
        "lea 12(%%sp), %%sp\n\t"
        "add.l %%d3, %%d2\n\t"
        "add.l %%d4, %%d2\n\t"
        "move.l %%d2, %0"
        : "=m"(r) : : "d2", "d3", "d4");
    CHECK(r == 0x66);  /* 0x11 + 0x22 + 0x33 */

    GROUP_END();
}

/* ── FPU ──────────────────────────────────────────────────── */

static const double const_zero = 0.0;
static const double const_two  = 2.0;
static const double const_three = 3.0;
static const double const_five = 5.0;
static const double const_neg_one = -1.0;
static const double const_half = 0.5;
static const double const_ten  = 10.0;
static const double const_thousand = 1000.0;

static void
test_fpu(void)
{
    uint32_t r;
    double rd;

    GROUP_BEGIN("fpu");

    /* FDADD — 2+3=5 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fdadd.d %%fp1, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_two), "m"(const_three) : "fp0", "fp1");
    CHECK(r == 5);

    /* FDSUB — 5-3=2 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fdsub.d %%fp1, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_five), "m"(const_three) : "fp0", "fp1");
    CHECK(r == 2);

    /* FDMUL — 5*2=10 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fdmul.d %%fp1, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_five), "m"(const_two) : "fp0", "fp1");
    CHECK(r == 10);

    /* FDDIV — 10/2=5 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fddiv.d %%fp1, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_ten), "m"(const_two) : "fp0", "fp1");
    CHECK(r == 5);

    /* FNEG — negate 5 → -5 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fneg.d %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_five) : "fp0");
    CHECK(r == (uint32_t)-5);

    /* FABS — |(-1)| = 1 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fabs.d %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_neg_one) : "fp0");
    CHECK(r == 1);

    /* FSQRT — sqrt(2)*1000 ≈ 1414 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fsqrt.d %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fdmul.d %%fp1, %%fp0\n\t"
        "fintrz.d %%fp0, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_two), "m"(const_thousand) : "fp0", "fp1");
    CHECK(r == 1414);

    /* FINTRZ — truncate 3.5 → 3 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fdadd.d %%fp1, %%fp0\n\t"
        "fintrz.d %%fp0, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_three), "m"(const_half) : "fp0", "fp1");
    CHECK(r == 3);

    /* FCMP + FBGT — 5 > 2 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fcmp.d %%fp1, %%fp0\n\t"
        "fbgt 0f\n\t"
        "moveq #0, %%d0\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d0\n"
        "1: move.l %%d0, %0"
        : "=m"(r) : "m"(const_five), "m"(const_two) : "fp0", "fp1", "d0");
    CHECK(r == 1);

    /* FCMP + FBEQ — 3 == 3 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fcmp.d %%fp1, %%fp0\n\t"
        "fbeq 0f\n\t"
        "moveq #0, %%d0\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d0\n"
        "1: move.l %%d0, %0"
        : "=m"(r) : "m"(const_three), "m"(const_three) : "fp0", "fp1", "d0");
    CHECK(r == 1);

    /* FMOVE.L Dn → FPn → Dn round-trip */
    __asm__ volatile(
        "move.l #42, %%d0\n\t"
        "fmove.l %%d0, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : : "d0", "fp0");
    CHECK(r == 42);

    /* FDMUL — 0.5 * 10 = 5 */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %2, %%fp1\n\t"
        "fdmul.d %%fp1, %%fp0\n\t"
        "fmove.l %%fp0, %0"
        : "=m"(r) : "m"(const_half), "m"(const_ten) : "fp0", "fp1");
    CHECK(r == 5);

    /* FMOVE.D store/reload — verify 3.0 round-trips */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "fmove.d %%fp0, %0"
        : "=m"(rd) : "m"(const_three) : "fp0");
    {
        uint32_t *p = (uint32_t *)&rd;
        /* big-endian: p[0] = high word, p[1] = low word */
        CHECK(p[0] == 0x40080000 && p[1] == 0x00000000);
    }

    /* FTST + FBEQ — test zero */
    __asm__ volatile(
        "fmove.d %1, %%fp0\n\t"
        "ftst.d %%fp0\n\t"
        "fbeq 0f\n\t"
        "moveq #0, %%d0\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d0\n"
        "1: move.l %%d0, %0"
        : "=m"(r) : "m"(const_zero) : "fp0", "d0");
    CHECK(r == 1);

    /* FMOVEM — save/restore fp2-fp3 via manual stack adjust */
    __asm__ volatile(
        "fmove.d %1, %%fp2\n\t"
        "fmove.d %2, %%fp3\n\t"
        "lea -24(%%sp), %%sp\n\t"
        "fmovem %%fp2/%%fp3, (%%sp)\n\t"
        "fmove.d %3, %%fp2\n\t"
        "fmove.d %3, %%fp3\n\t"
        "fmovem (%%sp), %%fp2/%%fp3\n\t"
        "lea 24(%%sp), %%sp\n\t"
        "fdadd.d %%fp3, %%fp2\n\t"
        "fmove.l %%fp2, %0"
        : "=m"(r)
        : "m"(const_two), "m"(const_three), "m"(const_zero)
        : "fp2", "fp3");
    CHECK(r == 5);

    GROUP_END();
}

/* ── CCR flag tests ───────────────────────────────────────── */

static void
test_ccr(void)
{
    uint32_t r;

    GROUP_BEGIN("ccr flags");

    /* ADD sets C on unsigned overflow */
    __asm__ volatile(
        "move.l #0xFFFFFFFF, %%d0\n\t"
        "addq.l #1, %%d0\n\t"
        "bcs 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* ADD sets V on signed overflow */
    __asm__ volatile(
        "move.l #0x7FFFFFFF, %%d0\n\t"
        "addq.l #1, %%d0\n\t"
        "bvs 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* ADD sets N on negative result */
    __asm__ volatile(
        "move.l #0x7FFFFFFF, %%d0\n\t"
        "addq.l #1, %%d0\n\t"
        "bmi 0f\n\t"
        "moveq #0, %%d1\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d1\n"
        "1: move.l %%d1, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 1);

    /* SUB sets Z on zero result */
    __asm__ volatile(
        "move.l #42, %%d0\n\t"
        "move.l #42, %%d1\n\t"
        "sub.l %%d1, %%d0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* SUB clears Z on nonzero */
    __asm__ volatile(
        "move.l #42, %%d0\n\t"
        "moveq #1, %%d1\n\t"
        "sub.l %%d1, %%d0\n\t"
        "bne 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* AND sets Z */
    __asm__ volatile(
        "move.l #0xFF00, %%d0\n\t"
        "move.l #0x00FF, %%d1\n\t"
        "and.l %%d1, %%d0\n\t"
        "beq 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    /* AND sets N */
    __asm__ volatile(
        "move.l #0x80000000, %%d0\n\t"
        "move.l #0xF0000000, %%d1\n\t"
        "and.l %%d1, %%d0\n\t"
        "bmi 0f\n\t"
        "moveq #0, %%d2\n\t"
        "bra 1f\n"
        "0: moveq #1, %%d2\n"
        "1: move.l %%d2, %0"
        : "=m"(r) : : "d0", "d1", "d2");
    CHECK(r == 1);

    GROUP_END();
}

/* ── Hand-assembled legacy instructions ───────────────────── */
/* ROR and EXG are implemented in the emulator but not in the
 * ColdFire V4e ISA — GAS rejects them. Test via raw opcode words. */

static void
test_legacy(void)
{
    uint32_t r;

    GROUP_BEGIN("legacy (ror/exg)");

    /* EXG D0, D1 — opcode: 0xC141 */
    __asm__ volatile(
        "move.l #111, %%d0\n\t"
        "move.l #222, %%d1\n\t"
        ".short 0xC141\n\t"     /* exg d0, d1 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 222);

    /* EXG D0, A0 — opcode: 0xC188 */
    __asm__ volatile(
        "move.l #0xAAAA, %%d0\n\t"
        "movea.l #0xBBBB, %%a0\n\t"
        ".short 0xC188\n\t"     /* exg d0, a0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "a0");
    CHECK(r == 0xBBBB);

    /* ROR.L #1, D0 — opcode: 0xE298 */
    __asm__ volatile(
        "move.l #0x00000001, %%d0\n\t"
        ".short 0xE298\n\t"     /* ror.l #1, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x80000000);

    /* ROR.L D1, D0 — opcode: 0xE2B8 (d1 shifts d0 right) */
    __asm__ volatile(
        "move.l #0x12345678, %%d0\n\t"
        "move.l #8, %%d1\n\t"
        ".short 0xE2B8\n\t"     /* ror.l d1, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0", "d1");
    CHECK(r == 0x78123456);

    GROUP_END();
}

/* ── ISA_C instructions: FF1, BYTEREV ────────────────────── */
/* GAS rejects these with -mcpu=5475, so we hand-assemble them. */

static void
test_isac(void)
{
    uint32_t r;

    GROUP_BEGIN("isac (ff1/byterev)");

    /* FF1 D0: find first 1 in 0x80000000 → result 0 */
    __asm__ volatile(
        "move.l #0x80000000, %%d0\n\t"
        ".short 0x04C0\n\t"     /* ff1 d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0);

    /* FF1 D1: find first 1 in 0x00010000 → result 15 */
    __asm__ volatile(
        "move.l #0x00010000, %%d1\n\t"
        ".short 0x04C1\n\t"     /* ff1 d1 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d1");
    CHECK(r == 15);

    /* FF1 D2: find first 1 in 0x00000001 → result 31 */
    __asm__ volatile(
        "move.l #0x00000001, %%d2\n\t"
        ".short 0x04C2\n\t"     /* ff1 d2 */
        "move.l %%d2, %0"
        : "=m"(r) : : "d2");
    CHECK(r == 31);

    /* FF1 D3: find first 1 in 0x00000000 → result 32 */
    __asm__ volatile(
        "move.l #0, %%d3\n\t"
        ".short 0x04C3\n\t"     /* ff1 d3 */
        "move.l %%d3, %0"
        : "=m"(r) : : "d3");
    CHECK(r == 32);

    /* FF1 D7: test different register */
    __asm__ volatile(
        "move.l #0x0F000000, %%d7\n\t"
        ".short 0x04C7\n\t"     /* ff1 d7 */
        "move.l %%d7, %0"
        : "=m"(r) : : "d7");
    CHECK(r == 4);

    /* BYTEREV D0: 0x12345678 → 0x78563412 */
    __asm__ volatile(
        "move.l #0x12345678, %%d0\n\t"
        ".short 0x02C0\n\t"     /* byterev d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0x78563412);

    /* BYTEREV D1: 0x00000001 → 0x01000000 */
    __asm__ volatile(
        "move.l #0x00000001, %%d1\n\t"
        ".short 0x02C1\n\t"     /* byterev d1 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d1");
    CHECK(r == 0x01000000);

    /* BYTEREV D7: 0xFF000000 → 0x000000FF */
    __asm__ volatile(
        "move.l #0xFF000000, %%d7\n\t"
        ".short 0x02C7\n\t"     /* byterev d7 */
        "move.l %%d7, %0"
        : "=m"(r) : : "d7");
    CHECK(r == 0x000000FF);

    /* BYTEREV double: swap and swap back = original */
    __asm__ volatile(
        "move.l #0xDEADBEEF, %%d0\n\t"
        ".short 0x02C0\n\t"     /* byterev d0 */
        ".short 0x02C0\n\t"     /* byterev d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 0xDEADBEEF);

    GROUP_END();
}

/* ── EMAC instructions: MAC.L, MSAC.L, register moves ───── */
/* These use hand-assembled opcodes because the assembler
 * mnemonics for EMAC are difficult to get right with GAS. */

static void
test_emac(void)
{
    uint32_t r;

    GROUP_BEGIN("emac");

    /* MOVE D0 → MACSR: set signed mode (SU=1) */
    /* Encoding: 1010 100 1 0 0 000 000 = 0xA900 (MACSR, dir=0, Dn=D0) */
    __asm__ volatile(
        "move.l #0x40, %%d0\n\t"    /* SU bit = 0x40 */
        ".short 0xA900\n\t"         /* move d0, macsr */
        : : : "d0");

    /* MOVE MACSR → D1: read it back */
    /* Encoding: 1010 100 1 1 0 000 001 = 0xA981 */
    __asm__ volatile(
        ".short 0xA981\n\t"         /* move macsr, d1 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d1");
    CHECK(r == 0x40);

    /* MOVE D0 → MASK register */
    /* Encoding: 1010 110 1 0 0 000 000 = 0xAD00 */
    __asm__ volatile(
        "move.l #0xFFFF0000, %%d0\n\t"
        ".short 0xAD00\n\t"         /* move d0, mask */
        : : : "d0");

    /* MOVE MASK → D1: read it back */
    /* Encoding: 1010 110 1 1 0 000 001 = 0xAD81 */
    __asm__ volatile(
        ".short 0xAD81\n\t"         /* move mask, d1 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d1");
    CHECK(r == 0xFFFF0000);

    /* Clear MACSR for fresh MAC test */
    __asm__ volatile(
        "move.l #0x40, %%d0\n\t"    /* SU=1, signed mode */
        ".short 0xA900\n\t"         /* move d0, macsr */
        : : : "d0");

    /* MOVE D0 → ACC0: set accumulator to 0 */
    /* Encoding: 1010 000 1 0 0 000 000 = 0xA100 */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        ".short 0xA100\n\t"         /* move d0, acc0 */
        : : : "d0");

    /* MAC.L D0, D1, ACC0: 3 * 5 = 15
     * opword:  1010 000 0 0 0 000 001 = 0xA001 ... no, that's a hypercall.
     * MAC.L encoding: 1010 Rx(2:0) a ACC[0] Ul Ry(2:0)
     *   Rx=D0(000), a=0(data), ACC[0]=0, Ul=0, Ry=D1(001)
     *   → 1010 000 0 0 0 000 001 = 0xA001
     * But 0xA001 conflicts with hypercall. Use different registers.
     * MAC.L D2, D3, ACC0:
     *   Rx=D2(010), a=0, ACC[0]=0, Ul=0, Ry=D3(011)
     *   → 1010 010 0 0 0 000 011 = 0xA403
     * Extension: 0000 1 SF(00) sub(0) 000 ACC[1](0) 0000
     *   → 0000 1 00 0 000 0 0000 = 0x0800 */
    __asm__ volatile(
        "move.l #3, %%d2\n\t"
        "move.l #5, %%d3\n\t"
        ".short 0xA403\n\t"         /* mac.l d2, d3, acc0 (opword) */
        ".short 0x0800\n\t"         /* extension: no scale, MAC, acc0 */
        : : : "d2", "d3");

    /* MOVE ACC0 → D0: read accumulator */
    /* Encoding: 1010 000 1 1 0 000 000 = 0xA180 */
    __asm__ volatile(
        ".short 0xA180\n\t"         /* move acc0, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 15);

    /* MAC.L again: accumulate 3*5 + 4*6 = 15 + 24 = 39 */
    __asm__ volatile(
        "move.l #4, %%d2\n\t"
        "move.l #6, %%d3\n\t"
        ".short 0xA403\n\t"         /* mac.l d2, d3, acc0 */
        ".short 0x0800\n\t"         /* extension */
        : : : "d2", "d3");

    __asm__ volatile(
        ".short 0xA180\n\t"         /* move acc0, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 39);

    /* MSAC.L D2, D3, ACC0: 39 - 2*10 = 19
     * Same opword as MAC but extension has sub=1
     * Extension: 0000 1 00 1 000 0 0000 = 0x0900 */
    __asm__ volatile(
        "move.l #2, %%d2\n\t"
        "move.l #10, %%d3\n\t"
        ".short 0xA403\n\t"         /* msac.l d2, d3, acc0 (opword) */
        ".short 0x0900\n\t"         /* extension: no scale, MSAC, acc0 */
        : : : "d2", "d3");

    __asm__ volatile(
        ".short 0xA180\n\t"         /* move acc0, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 19);

    /* MAC.L with scale <<1: ACC1 = 0 + (3*4)<<1 = 24
     * Use ACC1: ACC[0]=1 in opword (bit 7), ACC[1]=0 in ext
     * opword: 1010 010 0 1 0 000 011 = 0xA483
     * Extension: 0000 1 01 0 000 0 0000 = 0x0A00 (SF=01 = <<1) */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        ".short 0xA300\n\t"         /* move d0, acc1 (1010 001 1 0 0 000 000) */
        "move.l #3, %%d2\n\t"
        "move.l #4, %%d3\n\t"
        ".short 0xA483\n\t"         /* mac.l d2, d3, acc1 */
        ".short 0x0A00\n\t"         /* extension: scale <<1, MAC, acc1 */
        : : : "d0", "d2", "d3");

    /* MOVE ACC1 → D0 */
    /* Encoding: 1010 001 1 1 0 000 000 = 0xA380 */
    __asm__ volatile(
        ".short 0xA380\n\t"         /* move acc1, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 24);

    /* MAC.L with scale >>1: ACC2 = 0 + (100*100)>>1 = 5000
     * ACC2: ACC[0]=0 in opword, ACC[1]=1 in ext
     * opword: 1010 010 0 0 0 000 011 = 0xA403
     * Extension: 0000 1 11 0 000 1 0000 = 0x0E10 (SF=11 = >>1, ACC[1]=1) */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        ".short 0xA500\n\t"         /* move d0, acc2 (1010 010 1 0 0 000 000) */
        "move.l #100, %%d2\n\t"
        "move.l #100, %%d3\n\t"
        ".short 0xA403\n\t"         /* mac.l d2, d3, acc2 */
        ".short 0x0E10\n\t"         /* extension: scale >>1, MAC, acc2 */
        : : : "d0", "d2", "d3");

    /* MOVE ACC2 → D0 */
    /* Encoding: 1010 010 1 1 0 000 000 = 0xA580 */
    __asm__ volatile(
        ".short 0xA580\n\t"         /* move acc2, d0 */
        "move.l %%d0, %0"
        : "=m"(r) : : "d0");
    CHECK(r == 5000);

    /* MACSR N flag: accumulate a negative result */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        ".short 0xA100\n\t"         /* move d0, acc0 — clear acc0 */
        "move.l #0x40, %%d0\n\t"    /* SU=1 */
        ".short 0xA900\n\t"         /* move d0, macsr */
        "move.l #-5, %%d2\n\t"
        "move.l #3, %%d3\n\t"
        ".short 0xA403\n\t"         /* mac.l d2, d3, acc0 */
        ".short 0x0800\n\t"         /* extension */
        : : : "d0", "d2", "d3");

    __asm__ volatile(
        ".short 0xA981\n\t"         /* move macsr, d1 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d1");
    CHECK(r & 0x08);               /* N flag set */
    CHECK(!(r & 0x04));            /* Z flag clear */

    /* MACSR Z flag: accumulate to zero */
    __asm__ volatile(
        "move.l #0, %%d0\n\t"
        ".short 0xA100\n\t"         /* clear acc0 */
        "move.l #0x40, %%d0\n\t"
        ".short 0xA900\n\t"         /* macsr = SU */
        "move.l #0, %%d2\n\t"
        "move.l #99, %%d3\n\t"
        ".short 0xA403\n\t"         /* mac.l d2, d3, acc0 */
        ".short 0x0800\n\t"
        : : : "d0", "d2", "d3");

    __asm__ volatile(
        ".short 0xA981\n\t"         /* move macsr, d1 */
        "move.l %%d1, %0"
        : "=m"(r) : : "d1");
    CHECK(r & 0x04);               /* Z flag set */
    CHECK(!(r & 0x08));            /* N flag clear */

    GROUP_END();
}

/* ── Entry point ──────────────────────────────────────────── */

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    g_pass = 0;
    g_fail = 0;

#ifdef QEMU_USERMODE
    print_str("ColdFire V4e instruction tests\n");
    print_str("------------------------------\n");
#endif

    test_move();
    test_moveq_mvs_mvz();
    test_immediate();
    test_arithmetic();
    test_logic();
    test_compare();
    test_muldiv();
    test_shift();
    test_bit();
    test_branch();
    test_misc();
    test_fpu();
    test_ccr();
#ifndef QEMU_USERMODE
    /* These use hand-assembled opcodes that QEMU may not support */
    test_legacy();
    test_isac();
    test_emac();
#endif

    result_pass = g_pass;
    result_fail = g_fail;

#ifdef QEMU_USERMODE
    print_str("------------------------------\n");
    print_uint(g_pass);
    print_str("/");
    print_uint(g_pass + g_fail);
    print_str(" passed");
    if (g_fail) {
        print_str(", ");
        print_uint(g_fail);
        print_str(" FAILED");
    }
    print_str("\n");
    sys_exit(g_fail ? 1 : 0);
#else
    __asm__ volatile("trap #0");
    for (;;) ;
#endif
}
