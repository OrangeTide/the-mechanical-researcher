/* guest.c : RV32IMFC guest program for the browser demo */
/* Cross-compile: riscv64-linux-gnu-gcc -march=rv32imfc -mabi=ilp32f */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

/* A particle simulation in single-precision floating point, the sort of
 * thing a game would hand to a script. It runs entirely inside the
 * emulator: the only contact with the page is through ecall, once per
 * particle to draw it and once per frame to yield. */

typedef unsigned int uint32_t;
typedef int int32_t;

#define SYS_EXIT     93
#define SYS_DRAW     1024
#define SYS_PUTCHAR  1025
#define SYS_FRAME    1026

#define WIDTH   640.0f
#define HEIGHT  360.0f
#define COUNT   96

/****************************************************************
 * Host interface
 ****************************************************************/

static void
sys_draw(int32_t x, int32_t y, int32_t size, int32_t colour)
{
    register int32_t a0 __asm__("a0") = x;
    register int32_t a1 __asm__("a1") = y;
    register int32_t a2 __asm__("a2") = size;
    register int32_t a3 __asm__("a3") = colour;
    register uint32_t a7 __asm__("a7") = SYS_DRAW;

    __asm__ volatile("ecall"
                     : "+r"(a0)
                     : "r"(a1), "r"(a2), "r"(a3), "r"(a7)
                     : "memory");
}

static void
sys_frame(void)
{
    register uint32_t a7 __asm__("a7") = SYS_FRAME;

    __asm__ volatile("ecall" : : "r"(a7) : "memory");
}

static void
sys_putchar(int32_t c)
{
    register int32_t a0 __asm__("a0") = c;
    register uint32_t a7 __asm__("a7") = SYS_PUTCHAR;

    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

static void
put_str(const char *s)
{
    while (*s)
        sys_putchar(*s++);
}

/****************************************************************
 * Simulation
 ****************************************************************/

static float px[COUNT], py[COUNT], vx[COUNT], vy[COUNT];
static int32_t colour[COUNT];

/* A small deterministic generator so the demo looks the same every run */
static uint32_t seed = 0x2545f491;

static uint32_t
rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static float
rnd_unit(void)
{
    return (float)(rnd() & 0xffff) * (1.0f / 65536.0f);
}

/* Square root by Newton's method, which keeps the guest free of any
 * library and exercises fdiv.s and fadd.s on every iteration. */
static float
approx_sqrt(float x)
{
    float g;
    int i;

    if (x <= 0.0f)
        return 0.0f;
    g = x;
    for (i = 0; i < 8; i++)
        g = (g + x / g) * 0.5f;
    return g;
}

static void
init(void)
{
    int i;

    for (i = 0; i < COUNT; i++) {
        px[i] = rnd_unit() * WIDTH;
        py[i] = rnd_unit() * HEIGHT * 0.5f;
        vx[i] = (rnd_unit() - 0.5f) * 4.0f;
        vy[i] = (rnd_unit() - 0.5f) * 2.0f;
        colour[i] = (int32_t)(rnd() & 0x3f);
    }
}

static void
step(void)
{
    int i;

    for (i = 0; i < COUNT; i++) {
        vy[i] += 0.15f;                 /* gravity */
        px[i] += vx[i];
        py[i] += vy[i];

        if (px[i] < 0.0f) {
            px[i] = 0.0f;
            vx[i] = -vx[i] * 0.85f;
        }
        if (px[i] > WIDTH) {
            px[i] = WIDTH;
            vx[i] = -vx[i] * 0.85f;
        }
        if (py[i] > HEIGHT) {
            py[i] = HEIGHT;
            vy[i] = -vy[i] * 0.82f;
            vx[i] *= 0.98f;
        }
    }
}

/* A particle counts as at rest once it is sitting on the floor with
 * almost no speed left. Gravity keeps nudging it every frame, so the
 * bounce never converges to a true zero: it falls into a small limit
 * cycle around the floor instead. The thresholds allow for that rather
 * than looking for exact stillness, which would never arrive. */
static int
at_rest(int i)
{
    float ax = vx[i] < 0.0f ? -vx[i] : vx[i];
    float ay = vy[i] < 0.0f ? -vy[i] : vy[i];

    return py[i] >= HEIGHT - 1.0f && ax < 0.5f && ay < 0.5f;
}

static int
count_at_rest(void)
{
    int i, n = 0;

    for (i = 0; i < COUNT; i++)
        if (at_rest(i))
            n++;
    return n;
}

static void
draw(void)
{
    int i;

    for (i = 0; i < COUNT; i++) {
        /* Size follows speed, so the radius depends on a square root */
        float speed = approx_sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
        int32_t size = (int32_t)(2.0f + speed);

        if (size > 9)
            size = 9;
        sys_draw((int32_t)px[i], (int32_t)py[i], size, colour[i]);
    }
}

/* Once most of the particles have come to rest the picture stops moving,
 * and a demonstration that has gone still is not showing anything. So the
 * simulation scatters them again.
 *
 * The count has to hold for a while before that happens. Checking a
 * single frame would respawn the moment enough particles happened to be
 * slow together, cutting off the last few bounces that are the most
 * interesting part to watch. Waiting lets the pile actually settle first.
 *
 * The generator is deliberately not reseeded. It runs on from wherever it
 * had reached, so each scattering differs from the last while the whole
 * sequence stays reproducible from page load, which is what makes the
 * instruction counts comparable between runs. */
#define REST_FRACTION  (COUNT * 3 / 4)
#define REST_FRAMES    45

int
main(void)
{
    int calm = 0;

    put_str("rv32 guest running\n");
    init();

    for (;;) {
        step();
        draw();

        if (count_at_rest() >= REST_FRACTION) {
            if (++calm >= REST_FRAMES) {
                init();
                calm = 0;
            }
        } else {
            calm = 0;
        }

        sys_frame();
    }
    return 0;
}
