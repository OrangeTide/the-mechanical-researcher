/* test_glide.c — Host-side test suite for the Glide software rasterizer.
 *
 * Tests the rasterizer in isolation: no ColdFire CPU needed.
 * Sets up glide_state directly and calls draw_triangle / helpers,
 * then verifies output pixels in VRAM.
 *
 * Build:  gcc -Wall -Wextra -O2 -o test_glide test_glide.c glide_raster.c -lm
 * Run:    ./test_glide
 */

#include "glide_raster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VRAM_SIZE   (8 * 1024 * 1024)
#define RAM_SIZE    (8 * 1024 * 1024)

static uint8_t *vram;
static uint8_t *ram;
static glide_state gs;
static cf_cpu cpu;

static int tests_run;
static int tests_passed;
static int tests_failed;

/* ---- Helpers ----------------------------------------------------------- */

static void
reset(void)
{
    memset(vram, 0, VRAM_SIZE);
    memset(ram, 0, RAM_SIZE);
    memset(&cpu, 0, sizeof(cpu));
    glide_init(&gs, vram, ram);

    /* Open a window (sets defaults) */
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_SST_WIN_OPEN);
}

/** Read a 16-bit big-endian pixel from VRAM. */
static uint16_t
read_pixel(uint32_t fb_offset, int x, int y)
{
    uint32_t off = fb_offset + ((uint32_t)y * GR_SCREEN_W + (uint32_t)x) * 2;
    return (uint16_t)((vram[off] << 8) | vram[off + 1]);
}

/** Pack RGB565. */
static uint16_t
pack565(int r, int g, int b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/** Unpack RGB565 to 0-255. */
static void
unpack565(uint16_t c, int *r, int *g, int *b)
{
    int r5 = (c >> 11) & 0x1F;
    int g6 = (c >> 5) & 0x3F;
    int b5 = c & 0x1F;
    *r = (r5 << 3) | (r5 >> 2);
    *g = (g6 << 2) | (g6 >> 4);
    *b = (b5 << 3) | (b5 >> 2);
}

/** Write a big-endian float to guest RAM at the given address. */
static void
write_float(uint32_t addr, float f)
{
    uint32_t bits;
    memcpy(&bits, &f, 4);
    ram[addr]     = (uint8_t)(bits >> 24);
    ram[addr + 1] = (uint8_t)(bits >> 16);
    ram[addr + 2] = (uint8_t)(bits >> 8);
    ram[addr + 3] = (uint8_t)(bits);
}

/** Write a big-endian 32-bit int to guest RAM. */
static void
write_u32(uint32_t addr, uint32_t val)
{
    ram[addr]     = (uint8_t)(val >> 24);
    ram[addr + 1] = (uint8_t)(val >> 16);
    ram[addr + 2] = (uint8_t)(val >> 8);
    ram[addr + 3] = (uint8_t)(val);
}

/** Write a big-endian 16-bit int to guest RAM or VRAM. */
static void
write_u16_to(uint8_t *mem, uint32_t addr, uint16_t val)
{
    mem[addr]     = (uint8_t)(val >> 8);
    mem[addr + 1] = (uint8_t)(val);
}

/**
 * Write a vertex to guest RAM.
 * Layout: x(4) y(4) z(4) w(4) r(4) g(4) b(4) a(4) s(4) t(4) = 40 bytes
 */
static void
write_vertex(uint32_t addr, float x, float y, float z, float w,
             float r, float g, float b, float a,
             float s, float t)
{
    write_float(addr +  0, x);
    write_float(addr +  4, y);
    write_float(addr +  8, z);
    write_float(addr + 12, w);
    write_float(addr + 16, r);
    write_float(addr + 20, g);
    write_float(addr + 24, b);
    write_float(addr + 28, a);
    write_float(addr + 32, s);
    write_float(addr + 36, t);
}

/** Set up vertex layout for the 40-byte format above. */
static void
setup_vertex_layout(void)
{
    /* GR_PARAM_XY at offset 0 */
    cpu.d[0] = GR_PARAM_XY; cpu.d[1] = 0; cpu.d[2] = GR_PARAM_ENABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_VERTEX_LAYOUT);
    /* GR_PARAM_Z at offset 8 */
    cpu.d[0] = GR_PARAM_Z; cpu.d[1] = 8; cpu.d[2] = GR_PARAM_ENABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_VERTEX_LAYOUT);
    /* GR_PARAM_W at offset 12 */
    cpu.d[0] = GR_PARAM_W; cpu.d[1] = 12; cpu.d[2] = GR_PARAM_ENABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_VERTEX_LAYOUT);
    /* GR_PARAM_RGB at offset 16 */
    cpu.d[0] = GR_PARAM_RGB; cpu.d[1] = 16; cpu.d[2] = GR_PARAM_ENABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_VERTEX_LAYOUT);
    /* GR_PARAM_A at offset 28 */
    cpu.d[0] = GR_PARAM_A; cpu.d[1] = 28; cpu.d[2] = GR_PARAM_ENABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_VERTEX_LAYOUT);
    /* GR_PARAM_ST0 at offset 32 */
    cpu.d[0] = GR_PARAM_ST0; cpu.d[1] = 32; cpu.d[2] = GR_PARAM_ENABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_VERTEX_LAYOUT);
}

/** Draw a triangle from three guest-memory vertices. */
static void
draw_tri(uint32_t v0_addr, uint32_t v1_addr, uint32_t v2_addr)
{
    cpu.a[0] = v0_addr;
    cpu.a[1] = v1_addr;
    cpu.a[2] = v2_addr;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DRAW_TRIANGLE);
}

static void
check(const char *name, int condition)
{
    tests_run++;
    if (condition) {
        tests_passed++;
    } else {
        tests_failed++;
        printf("  FAIL: %s\n", name);
    }
}

/** Check pixel is approximately a given color (within tolerance). */
static int
pixel_approx(uint32_t fb_offset, int x, int y, int er, int eg, int eb, int tol)
{
    uint16_t px = read_pixel(fb_offset, x, y);
    int r, g, b;
    unpack565(px, &r, &g, &b);
    return abs(r - er) <= tol && abs(g - eg) <= tol && abs(b - eb) <= tol;
}

/* ---- Tests ------------------------------------------------------------- */

static void
test_buffer_clear(void)
{
    printf("test_buffer_clear\n");
    reset();

    /* Clear back buffer to red */
    cpu.d[0] = 0x00FF0000; /* AARRGGBB */
    cpu.d[1] = 0;
    cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    /* Back buffer should be red */
    uint16_t px = read_pixel(gs.draw_offset, 320, 240);
    check("center pixel is red", px == pack565(255, 0, 0));

    /* Front buffer should still be black */
    px = read_pixel(gs.front_offset, 320, 240);
    check("front buffer untouched", px == 0);
}

static void
test_buffer_clear_front(void)
{
    printf("test_buffer_clear_front\n");
    reset();

    /* Switch render target to front buffer */
    cpu.d[0] = GR_BUFFER_FRONTBUFFER;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_RENDER_BUFFER);

    /* Clear to green */
    cpu.d[0] = 0x0000FF00;
    cpu.d[1] = 0;
    cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    /* Front buffer should be green (this was Bug #2) */
    uint16_t px = read_pixel(gs.front_offset, 320, 240);
    check("front buffer cleared to green", px == pack565(0, 255, 0));

    /* Back buffer should still be black */
    px = read_pixel(gs.back_offset, 320, 240);
    check("back buffer untouched", px == 0);
}

static void
test_flat_triangle(void)
{
    printf("test_flat_triangle\n");
    reset();
    setup_vertex_layout();

    /* Color combine: local iterated */
    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    /* Disable depth test */
    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Large white triangle covering center of screen */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Center should be white */
    check("center is white", pixel_approx(gs.draw_offset, 320, 200, 255, 255, 255, 8));

    /* Outside triangle should be black */
    check("outside is black", pixel_approx(gs.draw_offset, 10, 10, 0, 0, 0, 0));
}

static void
test_gouraud_triangle(void)
{
    printf("test_gouraud_triangle\n");
    reset();
    setup_vertex_layout();

    /* Color combine: local iterated (Gouraud) */
    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Red, green, blue vertices */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f,   0.0f,   0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f,   0.0f, 255.0f,   0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f,   0.0f,   0.0f, 255.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Near v0 (top-left area, ~220,130) should be mostly red */
    int r, g, b;
    unpack565(read_pixel(gs.draw_offset, 220, 130), &r, &g, &b);
    check("near v0 red > 150", r > 150);
    check("near v0 green < 100", g < 100);

    /* Near v1 (top-right area, ~420,130) should be mostly green */
    unpack565(read_pixel(gs.draw_offset, 420, 130), &r, &g, &b);
    check("near v1 green > 150", g > 150);
    check("near v1 red < 100", r < 100);

    /* Near v2 (bottom center, ~320,360) should be mostly blue */
    unpack565(read_pixel(gs.draw_offset, 320, 360), &r, &g, &b);
    check("near v2 blue > 150", b > 150);
    check("near v2 red < 100", r < 100);

    /* Center (~320,200) should be a mix — all channels present */
    unpack565(read_pixel(gs.draw_offset, 320, 200), &r, &g, &b);
    check("center has red", r > 30);
    check("center has green", g > 30);
    check("center has blue", b > 30);
}

static void
test_depth_buffer(void)
{
    printf("test_depth_buffer\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    /* Enable Z-buffer, LESS compare, clear to far */
    cpu.d[0] = GR_DEPTHBUFFER_ZBUFFER;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);
    cpu.d[0] = GR_CMP_LESS;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_FUNCTION);
    cpu.d[0] = 1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_MASK);

    /* Clear buffers */
    cpu.d[0] = 0; cpu.d[1] = 0; cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    /* Draw a red triangle at z=0.7 (far) */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.7f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.7f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.7f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Draw a green triangle at z=0.3 (near), overlapping */
    write_vertex(v0, 250.0f, 150.0f, 0.3f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 390.0f, 150.0f, 0.3f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 330.0f, 0.3f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Overlap region should be green (nearer) */
    check("overlap is green", pixel_approx(gs.draw_offset, 320, 220, 0, 255, 0, 8));

    /* Red-only region (outside green tri) should still be red */
    check("red region intact", pixel_approx(gs.draw_offset, 220, 130, 255, 0, 0, 8));
}

static void
test_depth_gequal(void)
{
    printf("test_depth_gequal (reversed Z)\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    /* Reversed Z: GEQUAL, clear to 0 (near in reversed Z) */
    cpu.d[0] = GR_DEPTHBUFFER_ZBUFFER;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);
    cpu.d[0] = GR_CMP_GEQUAL;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_FUNCTION);
    cpu.d[0] = 1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_MASK);

    /* Clear to z=0 (near plane in reversed-Z) */
    cpu.d[0] = 0; cpu.d[1] = 0; cpu.d[2] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    /* Red triangle at z=0.7 — in reversed-Z, larger z = farther from viewer
     * but with GEQUAL, z=0.7 >= z_stored(0), so it passes */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.7f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.7f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.7f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    check("red tri drawn with GEQUAL", pixel_approx(gs.draw_offset, 320, 200, 255, 0, 0, 8));

    /* Green triangle at z=0.3 — z=0.3 < z_stored(0.7), fails GEQUAL */
    write_vertex(v0, 250.0f, 150.0f, 0.3f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 390.0f, 150.0f, 0.3f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 330.0f, 0.3f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Should still be red — green was rejected by GEQUAL */
    check("green rejected by GEQUAL", pixel_approx(gs.draw_offset, 320, 220, 255, 0, 0, 8));
}

static void
test_alpha_blend(void)
{
    printf("test_alpha_blend\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    /* Alpha combine: pass through iterated alpha */
    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_ALPHA_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Set alpha blend: SRC_ALPHA, ONE_MINUS_SRC_ALPHA */
    cpu.d[0] = GR_BLEND_SRC_ALPHA;
    cpu.d[1] = GR_BLEND_ONE_MINUS_SRC_ALPHA;
    cpu.d[2] = GR_BLEND_ONE;
    cpu.d[3] = GR_BLEND_ZERO;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_ALPHA_BLEND_FUNCTION);

    /* Draw opaque red background triangle */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 100.0f,  50.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 540.0f,  50.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 430.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Draw 50% alpha green triangle on top */
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 128.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 128.0f, 0, 0);
    write_vertex(v2, 320.0f, 350.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 128.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Overlap region should be ~50% red + ~50% green = yellow-ish */
    int r, g, b;
    unpack565(read_pixel(gs.draw_offset, 320, 200), &r, &g, &b);
    check("blend: red channel ~128", abs(r - 128) < 30);
    check("blend: green channel ~128", abs(g - 128) < 30);
    check("blend: blue channel ~0", b < 20);
}

static void
test_alpha_test(void)
{
    printf("test_alpha_test\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_ALPHA_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Alpha test: GREATER, ref=128 — only pixels with alpha > 128 pass */
    cpu.d[0] = GR_CMP_GREATER;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_ALPHA_TEST_FUNCTION);
    cpu.d[0] = 128;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_ALPHA_TEST_REFERENCE_VALUE);

    /* Draw triangle with alpha=64 — should be rejected */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 64.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 64.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 64.0f, 0, 0);
    draw_tri(v0, v1, v2);

    check("alpha<ref rejected", pixel_approx(gs.draw_offset, 320, 200, 0, 0, 0, 0));

    /* Draw triangle with alpha=255 — should pass */
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    check("alpha>ref passes", pixel_approx(gs.draw_offset, 320, 200, 0, 255, 0, 8));
}

static void
test_color_mask(void)
{
    printf("test_color_mask (Z-only pass)\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_ZBUFFER;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);
    cpu.d[0] = GR_CMP_LESS;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_FUNCTION);
    cpu.d[0] = 1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_MASK);

    cpu.d[0] = 0; cpu.d[1] = 0; cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    /* Disable color writes (Z-only pass) */
    cpu.d[0] = 0; /* rgb = false */
    cpu.d[1] = 0; /* alpha = false */
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_MASK);

    /* Draw triangle — Z should be written but color should not */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.3f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.3f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.3f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Color buffer should still be black */
    check("color not written", pixel_approx(gs.draw_offset, 320, 200, 0, 0, 0, 0));

    /* Re-enable color, try drawing a green triangle at z=0.5 (farther) */
    cpu.d[0] = 1; cpu.d[1] = 1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_MASK);

    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Green should be rejected — Z-only pass wrote z=0.3, green is at z=0.5 */
    check("green rejected by Z-only pass", pixel_approx(gs.draw_offset, 320, 200, 0, 0, 0, 0));
}

static void
test_scissor(void)
{
    printf("test_scissor\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Set scissor to a small rectangle */
    cpu.d[0] = 300; cpu.d[1] = 200; cpu.d[2] = 340; cpu.d[3] = 240;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_CLIP_WINDOW);

    /* Draw a full-screen white triangle */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0,   0.0f,   0.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    write_vertex(v1, 639.0f,   0.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 479.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Inside scissor should be white */
    check("inside scissor is white", pixel_approx(gs.draw_offset, 320, 220, 255, 255, 255, 8));

    /* Outside scissor should be black */
    check("left of scissor is black", pixel_approx(gs.draw_offset, 100, 220, 0, 0, 0, 0));
    check("below scissor is black", pixel_approx(gs.draw_offset, 320, 300, 0, 0, 0, 0));
}

static void
test_texture_rgb565(void)
{
    printf("test_texture_rgb565\n");
    reset();
    setup_vertex_layout();

    /* Create a 4x4 RGB565 texture: top-left red, top-right green,
     * bottom-left blue, bottom-right white */
    uint32_t tex_offset = 0;
    uint32_t tex_vram = GR_TEX_START + tex_offset;
    for (int ty = 0; ty < 4; ty++) {
        for (int tx = 0; tx < 4; tx++) {
            uint16_t c;
            if (tx < 2 && ty < 2)       c = pack565(255, 0, 0);    /* red */
            else if (tx >= 2 && ty < 2)  c = pack565(0, 255, 0);    /* green */
            else if (tx < 2 && ty >= 2)  c = pack565(0, 0, 255);    /* blue */
            else                         c = pack565(255, 255, 255); /* white */
            write_u16_to(vram, tex_vram + (ty * 4 + tx) * 2, c);
        }
    }

    /* Set texture source */
    cpu.d[0] = 0; /* tmu */
    cpu.d[1] = tex_offset;
    cpu.d[2] = GR_LOD_LOG2_4;  /* large_lod */
    cpu.d[3] = GR_LOD_LOG2_4;  /* small_lod */
    cpu.d[4] = GR_ASPECT_LOG2_1x1;
    cpu.d[5] = GR_TEXFMT_RGB_565;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_SOURCE);

    /* Clamp mode */
    cpu.d[0] = 0; cpu.d[1] = GR_TEXCLAMP_CLAMP; cpu.d[2] = GR_TEXCLAMP_CLAMP;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_CLAMP_MODE);

    /* Color combine: texture only (SCALE_OTHER with factor=ONE, other=texture) */
    cpu.d[0] = GR_COMBINE_FUNCTION_SCALE_OTHER;
    cpu.d[1] = GR_COMBINE_FACTOR_ONE;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_TEXTURE;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Draw a textured quad as two triangles.
     * UVs map to texel space: 0..4 for a 4x4 texture.
     * Quad covers 200..440 x 100..340 */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40, v3 = v2 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0.0f, 0.0f);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 4.0f, 0.0f);
    write_vertex(v2, 440.0f, 340.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 4.0f, 4.0f);
    write_vertex(v3, 200.0f, 340.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0.0f, 4.0f);
    draw_tri(v0, v1, v2);
    draw_tri(v0, v2, v3);

    /* Top-left quadrant of the quad should be red (texel 0,0 or 1,1) */
    check("tex top-left red", pixel_approx(gs.draw_offset, 260, 160, 255, 0, 0, 16));

    /* Top-right quadrant should be green */
    check("tex top-right green", pixel_approx(gs.draw_offset, 380, 160, 0, 255, 0, 16));

    /* Bottom-left quadrant should be blue */
    check("tex bottom-left blue", pixel_approx(gs.draw_offset, 260, 280, 0, 0, 255, 16));

    /* Bottom-right quadrant should be white */
    check("tex bottom-right white", pixel_approx(gs.draw_offset, 380, 280, 255, 255, 255, 16));
}

static void
test_texture_wrap(void)
{
    printf("test_texture_wrap\n");
    reset();
    setup_vertex_layout();

    /* Create a 2x2 texture: red, green, blue, white */
    uint32_t tex_offset = 0;
    uint32_t tex_vram = GR_TEX_START + tex_offset;
    write_u16_to(vram, tex_vram + 0, pack565(255, 0, 0));
    write_u16_to(vram, tex_vram + 2, pack565(0, 255, 0));
    write_u16_to(vram, tex_vram + 4, pack565(0, 0, 255));
    write_u16_to(vram, tex_vram + 6, pack565(255, 255, 255));

    cpu.d[0] = 0; cpu.d[1] = tex_offset;
    cpu.d[2] = GR_LOD_LOG2_2; cpu.d[3] = GR_LOD_LOG2_2;
    cpu.d[4] = GR_ASPECT_LOG2_1x1; cpu.d[5] = GR_TEXFMT_RGB_565;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_SOURCE);

    /* WRAP mode */
    cpu.d[0] = 0; cpu.d[1] = GR_TEXCLAMP_WRAP; cpu.d[2] = GR_TEXCLAMP_WRAP;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_CLAMP_MODE);

    cpu.d[0] = GR_COMBINE_FUNCTION_SCALE_OTHER;
    cpu.d[1] = GR_COMBINE_FACTOR_ONE;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_TEXTURE;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Draw quad with UVs going 0..4 on a 2x2 texture — should wrap twice */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40, v3 = v2 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0.0f, 0.0f);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 4.0f, 0.0f);
    write_vertex(v2, 440.0f, 340.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 4.0f, 4.0f);
    write_vertex(v3, 200.0f, 340.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0.0f, 4.0f);
    draw_tri(v0, v1, v2);
    draw_tri(v0, v2, v3);

    /* At s=0,t=0 and s=2,t=0 should both be red (texel 0,0 wraps) */
    check("wrap: (0,0) is red", pixel_approx(gs.draw_offset, 205, 105, 255, 0, 0, 16));

    /* At s=3,t=0 should be green (texel 1,0) — that's 3/4 of the way across */
    int quarter_w = (440 - 200) / 4;
    int x_s3 = 200 + 3 * quarter_w + quarter_w / 2;
    check("wrap: s=3 is green", pixel_approx(gs.draw_offset, x_s3, 105, 0, 255, 0, 16));
}

static void
test_texture_nonsquare(void)
{
    printf("test_texture_nonsquare (aspect ratio)\n");
    reset();
    setup_vertex_layout();

    /* Create a 4x2 texture (aspect 2:1).
     * Left half red, right half blue. */
    uint32_t tex_offset = 0;
    uint32_t tex_vram = GR_TEX_START + tex_offset;
    for (int ty = 0; ty < 2; ty++) {
        for (int tx = 0; tx < 4; tx++) {
            uint16_t c = (tx < 2) ? pack565(255, 0, 0) : pack565(0, 0, 255);
            write_u16_to(vram, tex_vram + (ty * 4 + tx) * 2, c);
        }
    }

    /* aspect = GR_ASPECT_LOG2_2x1 = 1, meaning width = 2 * height */
    cpu.d[0] = 0; cpu.d[1] = tex_offset;
    cpu.d[2] = GR_LOD_LOG2_4; /* large_lod = 4x4 base, but with aspect 2:1 → 4x2 */
    cpu.d[3] = GR_LOD_LOG2_4;
    cpu.d[4] = (uint32_t)GR_ASPECT_LOG2_2x1;
    cpu.d[5] = GR_TEXFMT_RGB_565;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_SOURCE);

    cpu.d[0] = 0; cpu.d[1] = GR_TEXCLAMP_CLAMP; cpu.d[2] = GR_TEXCLAMP_CLAMP;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_CLAMP_MODE);

    cpu.d[0] = GR_COMBINE_FUNCTION_SCALE_OTHER;
    cpu.d[1] = GR_COMBINE_FACTOR_ONE;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_TEXTURE;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Draw quad with UVs 0..4 x 0..2 */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40, v3 = v2 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0.0f, 0.0f);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 4.0f, 0.0f);
    write_vertex(v2, 440.0f, 340.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 4.0f, 2.0f);
    write_vertex(v3, 200.0f, 340.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0.0f, 2.0f);
    draw_tri(v0, v1, v2);
    draw_tri(v0, v2, v3);

    /* Left half should be red, right half blue */
    check("nonsquare left is red", pixel_approx(gs.draw_offset, 260, 220, 255, 0, 0, 16));
    check("nonsquare right is blue", pixel_approx(gs.draw_offset, 380, 220, 0, 0, 255, 16));
}

static void
test_cull_mode(void)
{
    printf("test_cull_mode\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Set cull mode to NEGATIVE (cull CW winding) */
    cpu.d[0] = GR_CULL_NEGATIVE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_CULL_MODE);

    /* Draw CCW triangle (should survive culling) */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    check("CCW survives CULL_NEGATIVE", pixel_approx(gs.draw_offset, 320, 200, 255, 0, 0, 8));

    /* Clear and draw CW triangle (reversed winding) — should be culled */
    cpu.d[0] = 0; cpu.d[1] = 0; cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    draw_tri(v0, v2, v1); /* reversed order = CW */

    check("CW culled by CULL_NEGATIVE", pixel_approx(gs.draw_offset, 320, 200, 0, 0, 0, 0));
}

static void
test_constant_color(void)
{
    printf("test_constant_color\n");
    reset();
    setup_vertex_layout();

    /* Color combine: local=constant, function=LOCAL */
    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_CONSTANT;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    /* Set constant color to cyan */
    cpu.d[0] = 0xFF00FFFF; /* AARRGGBB: alpha=FF, R=00, G=FF, B=FF */
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_CONSTANT_COLOR_VALUE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Draw triangle — vertex colors should be ignored, constant cyan used */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    check("constant cyan: r=0", pixel_approx(gs.draw_offset, 320, 200, 0, 255, 255, 8));
}

static void
test_texture_modulate(void)
{
    printf("test_texture_modulate (texture * vertex color)\n");
    reset();
    setup_vertex_layout();

    /* Create a 2x2 white texture */
    uint32_t tex_offset = 0;
    uint32_t tex_vram = GR_TEX_START + tex_offset;
    for (int i = 0; i < 4; i++)
        write_u16_to(vram, tex_vram + i * 2, pack565(255, 255, 255));

    cpu.d[0] = 0; cpu.d[1] = tex_offset;
    cpu.d[2] = GR_LOD_LOG2_2; cpu.d[3] = GR_LOD_LOG2_2;
    cpu.d[4] = GR_ASPECT_LOG2_1x1; cpu.d[5] = GR_TEXFMT_RGB_565;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_SOURCE);

    cpu.d[0] = 0; cpu.d[1] = GR_TEXCLAMP_CLAMP; cpu.d[2] = GR_TEXCLAMP_CLAMP;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_TEX_CLAMP_MODE);

    /* SCALE_OTHER with factor=LOCAL (iterated), other=TEXTURE
     * This is the classic: output = texture * vertex_color */
    cpu.d[0] = GR_COMBINE_FUNCTION_SCALE_OTHER;
    cpu.d[1] = GR_COMBINE_FACTOR_LOCAL;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_TEXTURE;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Draw triangle with 50% red vertex color.
     * white texture * (128,0,0) should = (128,0,0) */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 128.0f, 0.0f, 0.0f, 255.0f, 0.5f, 0.5f);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 128.0f, 0.0f, 0.0f, 255.0f, 0.5f, 0.5f);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 128.0f, 0.0f, 0.0f, 255.0f, 0.5f, 0.5f);
    draw_tri(v0, v1, v2);

    int r, g, b;
    unpack565(read_pixel(gs.draw_offset, 320, 200), &r, &g, &b);
    check("modulate: r ~128", abs(r - 128) < 20);
    check("modulate: g ~0", g < 10);
    check("modulate: b ~0", b < 10);
}

static void
test_line_drawing(void)
{
    printf("test_line_drawing\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    /* Draw a horizontal white line from (100,240) to (539,240) */
    uint32_t v0 = 0x100000, v1 = v0 + 40;
    write_vertex(v0, 100.0f, 240.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);
    write_vertex(v1, 539.0f, 240.0f, 0.5f, 1.0f, 255.0f, 255.0f, 255.0f, 255.0f, 0, 0);

    cpu.a[0] = v0; cpu.a[1] = v1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DRAW_LINE);

    /* Pixels on the line should be white */
    check("line midpoint white", pixel_approx(gs.draw_offset, 320, 240, 255, 255, 255, 8));
    check("line start white", pixel_approx(gs.draw_offset, 100, 240, 255, 255, 255, 8));

    /* Pixel above the line should be black */
    check("above line black", pixel_approx(gs.draw_offset, 320, 239, 0, 0, 0, 0));
}

static void
test_lfb_write(void)
{
    printf("test_lfb_write_region\n");
    reset();

    /* Write a 2x2 patch of red pixels to guest RAM */
    uint32_t src = 0x200000;
    write_u16_to(ram, src + 0, pack565(255, 0, 0));
    write_u16_to(ram, src + 2, pack565(255, 0, 0));
    write_u16_to(ram, src + 4, pack565(255, 0, 0));
    write_u16_to(ram, src + 6, pack565(255, 0, 0));

    /* grLfbWriteRegion(backbuffer, 100, 100, 2, 2, stride=4, src) */
    cpu.d[0] = GR_BUFFER_BACKBUFFER;
    cpu.d[1] = 100; /* x */
    cpu.d[2] = 100; /* y */
    cpu.d[3] = 2;   /* w */
    cpu.d[4] = 2;   /* h */
    cpu.d[5] = 4;   /* stride in bytes */
    cpu.a[0] = src;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_LFB_WRITE_REGION);

    check("LFB (100,100) red", pixel_approx(gs.back_offset, 100, 100, 255, 0, 0, 8));
    check("LFB (101,101) red", pixel_approx(gs.back_offset, 101, 101, 255, 0, 0, 8));
    check("LFB (102,100) black", pixel_approx(gs.back_offset, 102, 100, 0, 0, 0, 0));
}

static void
test_fog_table_indexing(void)
{
    printf("test_fog_table_indexing\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_ALPHA_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_DISABLE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);

    /* Set fog table: entry 0 = no fog, entry 63 = full fog.
     * Linear ramp for simplicity. */
    uint32_t fog_ptr = 0x300000;
    for (int i = 0; i < 64; i++)
        ram[fog_ptr + i] = (uint8_t)(i * 255 / 63);

    cpu.a[0] = fog_ptr;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_FOG_TABLE);

    /* Set fog color to white */
    cpu.d[0] = 0x00FFFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_FOG_COLOR_VALUE);

    /* Enable fog */
    cpu.d[0] = GR_FOG_WITH_TABLE_ON_Q;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_FOG_MODE);

    /* Draw a red triangle with oow (q) = 1.0.
     * q=1.0 → table index = 4*log2(1) = 0 → fog factor = 0 → no fog.
     * Color should be pure red. */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    int r, g, b;
    unpack565(read_pixel(gs.draw_offset, 320, 200), &r, &g, &b);
    check("fog q=1 (near): red still bright", r > 200);
    check("fog q=1 (near): no white bleed", g < 30);

    /* Clear and draw with large q (far away) — should be heavily fogged */
    cpu.d[0] = 0; cpu.d[1] = 0; cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    float far_q = 10000.0f; /* q = 10000 → idx = 4*log2(10000) ≈ 53 → heavy fog */
    write_vertex(v0, 200.0f, 100.0f, 0.5f, far_q, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, far_q, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, far_q, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    unpack565(read_pixel(gs.draw_offset, 320, 200), &r, &g, &b);
    check("fog q=10000 (far): red faded", r > 200); /* fog blends toward white */
    check("fog q=10000 (far): fogged white", g > 150); /* significant white blend */
}

static void
test_depth_bias(void)
{
    printf("test_depth_bias\n");
    reset();
    setup_vertex_layout();

    cpu.d[0] = GR_COMBINE_FUNCTION_LOCAL;
    cpu.d[1] = GR_COMBINE_FACTOR_ZERO;
    cpu.d[2] = GR_COMBINE_LOCAL_ITERATED;
    cpu.d[3] = GR_COMBINE_OTHER_ITERATED;
    cpu.d[4] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_COLOR_COMBINE);

    cpu.d[0] = GR_DEPTHBUFFER_ZBUFFER;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_MODE);
    cpu.d[0] = GR_CMP_LESS;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BUFFER_FUNCTION);
    cpu.d[0] = 1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_MASK);

    cpu.d[0] = 0; cpu.d[1] = 0; cpu.d[2] = 0xFFFF;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_BUFFER_CLEAR);

    /* Draw red triangle at z=0.5 */
    uint32_t v0 = 0x100000, v1 = v0 + 40, v2 = v1 + 40;
    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 255.0f, 0.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    /* Draw coplanar green triangle at z=0.5 with depth bias -1.
     * Bias makes incoming z slightly less, so it should pass LESS test. */
    cpu.d[0] = (uint32_t)(int32_t)-1;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BIAS_LEVEL);

    write_vertex(v0, 200.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v1, 440.0f, 100.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    write_vertex(v2, 320.0f, 380.0f, 0.5f, 1.0f, 0.0f, 255.0f, 0.0f, 255.0f, 0, 0);
    draw_tri(v0, v1, v2);

    check("depth bias: green replaces red", pixel_approx(gs.draw_offset, 320, 200, 0, 255, 0, 8));

    /* Reset bias */
    cpu.d[0] = 0;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_DEPTH_BIAS_LEVEL);
}

static void
test_get_string(void)
{
    printf("test_get_string (Vertex Technologies branding)\n");
    reset();

    /* GR_HARDWARE */
    cpu.d[0] = GR_HARDWARE;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_GET_STRING);
    uint32_t addr = cpu.d[0];
    check("grGetString(HARDWARE) returns address", addr != 0);
    check("grGetString(HARDWARE) contains SST-2",
          strstr((char *)(ram + addr), "SST-2") != NULL);

    /* GR_VENDOR */
    cpu.d[0] = GR_VENDOR;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_GET_STRING);
    addr = cpu.d[0];
    check("grGetString(VENDOR) returns address", addr != 0);
    check("grGetString(VENDOR) is Vertex Technologies",
          strstr((char *)(ram + addr), "Vertex Technologies") != NULL);

    /* GR_VERSION */
    cpu.d[0] = GR_VERSION;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_GET_STRING);
    addr = cpu.d[0];
    check("grGetString(VERSION) is 3.10",
          strcmp((char *)(ram + addr), "3.10") == 0);

    /* Invalid ID returns 0 */
    cpu.d[0] = 99;
    glide_dispatch(&gs, &cpu, 0xA000 | GR_HC_GET_STRING);
    check("grGetString(invalid) returns 0", cpu.d[0] == 0);
}

/* ---- Main -------------------------------------------------------------- */

int
main(void)
{
    vram = calloc(1, VRAM_SIZE);
    ram = calloc(1, RAM_SIZE);
    if (!vram || !ram) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    printf("=== Glide Rasterizer Test Suite ===\n\n");

    test_buffer_clear();
    test_buffer_clear_front();
    test_flat_triangle();
    test_gouraud_triangle();
    test_depth_buffer();
    test_depth_gequal();
    test_alpha_blend();
    test_alpha_test();
    test_color_mask();
    test_scissor();
    test_texture_rgb565();
    test_texture_wrap();
    test_texture_nonsquare();
    test_cull_mode();
    test_constant_color();
    test_texture_modulate();
    test_line_drawing();
    test_lfb_write();
    test_fog_table_indexing();
    test_depth_bias();
    test_get_string();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    free(vram);
    free(ram);
    return tests_failed > 0 ? 1 : 0;
}
