/*
 * cube.c -- Textured spinning cube demo for the Triton console
 *
 * Uses the Glide 3.0 API to render a perspective-correct textured cube
 * with per-face vertex colors, depth buffering, and backface culling.
 * No libc — all math uses a 256-entry sine lookup table.
 *
 * Build: m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
 *        -T app_link.ld -o cube.elf cube.c common.o
 */

#include "common.h"
#include "glide3x.h"

/* ---- Fixed-point sine table (Q15) -------------------------------------- */

#define SIN_BITS    8
#define SIN_COUNT   (1 << SIN_BITS)         /* 256 entries */
#define SIN_MASK    (SIN_COUNT - 1)
#define FP_SHIFT    15
#define FP_ONE      (1 << FP_SHIFT)         /* 32768 */

/*
 * sin_table[i] = (int)(sin(2*pi*i/256) * 32768)
 * Generated offline; no floating-point needed at init time.
 */
static const short sin_table[SIN_COUNT] = {
         0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
      6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
     12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
     18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
     23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
     27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
     30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
     32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
     32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
     32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
     30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
     27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
     23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
     18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
     12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
      6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
         0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
     -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
    -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
     -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

static int
fp_sin(int angle)
{
    return sin_table[angle & SIN_MASK];
}

static int
fp_cos(int angle)
{
    return sin_table[(angle + 64) & SIN_MASK];
}

/*
 * Fixed-point multiply: (a * b) >> 15
 * Avoid 64-bit multiply (no __muldi3 in freestanding).
 * Split into high and low 16-bit halves.
 */
static int
fp_mul(int a, int b)
{
    int sign = 1;
    unsigned int ua, ub, ah, al, bh, bl;
    unsigned int result;

    if (a < 0) { sign = -sign; a = -a; }
    if (b < 0) { sign = -sign; b = -b; }

    ua = (unsigned int)a;
    ub = (unsigned int)b;
    ah = ua >> 16;
    al = ua & 0xFFFF;
    bh = ub >> 16;
    bl = ub & 0xFFFF;

    /* (ah*2^16 + al) * (bh*2^16 + bl) >> 15
     * = ah*bh*2^17 + (ah*bl + al*bh)*2^1 + (al*bl)>>15 */
    result = (ah * bh) << 17;
    result += (ah * bl + al * bh) << 1;
    result += (al * bl) >> 15;

    return sign < 0 ? -(int)result : (int)result;
}

/* ---- Integer-to-string for UART output --------------------------------- */

static void
put_int(int n)
{
    char buf[12];
    int i = 0;
    int neg = 0;

    if (n < 0) {
        neg = 1;
        n = -n;
    }
    do {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg)
        putc_uart('-');
    while (i > 0)
        putc_uart(buf[--i]);
}

/* ---- Cube geometry ----------------------------------------------------- */

/* Unit cube vertices in Q15 fixed-point (±16384 = ±0.5) */
#define H FP_ONE / 2   /* 16384 */

static const int cube_verts[8][3] = {
    { -H, -H, -H },    /* 0: left  bottom back  */
    {  H, -H, -H },    /* 1: right bottom back  */
    {  H,  H, -H },    /* 2: right top    back  */
    { -H,  H, -H },    /* 3: left  top    back  */
    { -H, -H,  H },    /* 4: left  bottom front */
    {  H, -H,  H },    /* 5: right bottom front */
    {  H,  H,  H },    /* 6: right top    front */
    { -H,  H,  H },    /* 7: left  top    front */
};

/*
 * 6 faces x 2 triangles = 12 triangles.
 * Winding order: counter-clockwise when viewed from outside.
 */
static const unsigned char cube_tris[12][3] = {
    /* Front face  (+Z) */  {4, 5, 6}, {4, 6, 7},
    /* Back face   (-Z) */  {1, 0, 3}, {1, 3, 2},
    /* Right face  (+X) */  {5, 1, 2}, {5, 2, 6},
    /* Left face   (-X) */  {0, 4, 7}, {0, 7, 3},
    /* Top face    (+Y) */  {7, 6, 2}, {7, 2, 3},
    /* Bottom face (-Y) */  {0, 1, 5}, {0, 5, 4},
};

/* Per-face vertex colors (R, G, B as 0-255 integers) */
static const unsigned char face_colors[6][3] = {
    { 255, 100, 100 },     /* front  — red */
    { 100, 255, 100 },     /* back   — green */
    { 100, 100, 255 },     /* right  — blue */
    { 255, 255, 100 },     /* left   — yellow */
    { 255, 100, 255 },     /* top    — magenta */
    { 100, 255, 255 },     /* bottom — cyan */
};

/* Per-triangle texture coordinates (texel space: 0..TEX_SIZE) */
#define TEX_SIZE    64
#define T TEX_SIZE
static const float tri_uv[12][3][2] = {
    /* Front 0 */  {{0,T}, {T,T}, {T,0}},
    /* Front 1 */  {{0,T}, {T,0}, {0,0}},
    /* Back 0  */  {{0,T}, {T,T}, {T,0}},
    /* Back 1  */  {{0,T}, {T,0}, {0,0}},
    /* Right 0 */  {{0,T}, {T,T}, {T,0}},
    /* Right 1 */  {{0,T}, {T,0}, {0,0}},
    /* Left 0  */  {{0,T}, {T,T}, {T,0}},
    /* Left 1  */  {{0,T}, {T,0}, {0,0}},
    /* Top 0   */  {{0,T}, {T,T}, {T,0}},
    /* Top 1   */  {{0,T}, {T,0}, {0,0}},
    /* Bot 0   */  {{0,T}, {T,T}, {T,0}},
    /* Bot 1   */  {{0,T}, {T,0}, {0,0}},
};
#undef T

/* ---- Checkerboard texture ---------------------------------------------- */

#define TEX_BYTES   (TEX_SIZE * TEX_SIZE * 2)   /* RGB565 */

static unsigned short tex_data[TEX_SIZE * TEX_SIZE];

static unsigned short
rgb565(int r, int g, int b)
{
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void
generate_checkerboard(void)
{
    int x, y;
    unsigned short c0 = rgb565(255, 255, 255);
    unsigned short c1 = rgb565(80, 80, 80);

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            int check = ((x >> 3) ^ (y >> 3)) & 1;
            tex_data[y * TEX_SIZE + x] = check ? c1 : c0;
        }
    }
}

/* ---- 3D projection ----------------------------------------------------- */

#define SCREEN_CX   320     /* screen center X */
#define SCREEN_CY   240     /* screen center Y */
#define PROJ_D      400     /* projection distance (pixels) */
#define CAM_Z       (3 * FP_ONE)  /* camera distance from origin */

/*
 * Transform a model-space vertex by rotation (angleX, angleY) and
 * project to screen space.  All rotation in fixed-point, projection
 * produces float GrVertex fields.
 */
static void
transform_vertex(const int v[3], int ax, int ay,
                 GrVertex *out, float r, float g, float b,
                 float s, float t)
{
    int x = v[0], y = v[1], z = v[2];

    /* Rotate around Y axis */
    int x1 = fp_mul(x, fp_cos(ay)) - fp_mul(z, fp_sin(ay));
    int z1 = fp_mul(x, fp_sin(ay)) + fp_mul(z, fp_cos(ay));
    int y1 = y;

    /* Rotate around X axis */
    int y2 = fp_mul(y1, fp_cos(ax)) - fp_mul(z1, fp_sin(ax));
    int z2 = fp_mul(y1, fp_sin(ax)) + fp_mul(z1, fp_cos(ax));
    int x2 = x1;

    /* Translate away from camera */
    int zt = z2 + CAM_Z;
    if (zt < 256)
        zt = 256;   /* clamp to avoid divide-by-zero */

    /* Perspective divide (integer) */
    int sx = SCREEN_CX + (x2 * PROJ_D / zt);
    int sy = SCREEN_CY - (y2 * PROJ_D / zt);

    /* 1/w for perspective-correct texturing */
    float w = (float)zt / (float)FP_ONE;
    float oow = 1.0f / w;

    /* Depth as 0..65535 range (closer = larger z in screen space) */
    float ooz = 65535.0f * (float)FP_ONE / (float)zt;

    out->x = (float)sx;
    out->y = (float)sy;
    out->ooz = ooz;
    out->oow = oow;
    out->r = r;
    out->g = g;
    out->b = b;
    out->a = 255.0f;
    out->sow = s * oow;
    out->tow = t * oow;
}

/* ---- Main -------------------------------------------------------------- */

#define NUM_FRAMES  300

void
_start(void)
{
    int frame, tri;
    int angle_x = 0, angle_y = 0;
    GrVertex va, vb, vc;

    puts_uart("cube: starting Glide demo\r\n");

    /* Initialize Glide */
    grGlideInit();
    grSstSelect(0);
    grSstWinOpen();

    /* Vertex layout — must match GrVertex struct */
    grVertexLayout(GR_PARAM_XY,  GR_VERTEX_X_OFFSET,   GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_Z,   GR_VERTEX_OOZ_OFFSET,  GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_W,   GR_VERTEX_OOW_OFFSET, GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_RGB, GR_VERTEX_R_OFFSET,   GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_A,   GR_VERTEX_A_OFFSET,   GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_ST0, GR_VERTEX_SOW_OFFSET, GR_PARAM_ENABLE);
    grCoordinateSpace(GR_WINDOW_COORDS);

    /* Color combine: texture modulated by iterated vertex color */
    grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER,
                   GR_COMBINE_FACTOR_LOCAL,
                   GR_COMBINE_LOCAL_ITERATED,
                   GR_COMBINE_OTHER_TEXTURE,
                   0);
    grTexCombine(0,
                 GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO,
                 GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO,
                 0, 0);
    grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL,
                   GR_COMBINE_FACTOR_ZERO,
                   GR_COMBINE_LOCAL_ITERATED,
                   GR_COMBINE_OTHER_ITERATED,
                   0);

    /* Depth buffer: enable, write, less-or-equal comparison */
    grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER);
    grDepthBufferFunction(GR_CMP_LEQUAL);
    grDepthMask(1);

    /* Backface culling: cull CW triangles (negative signed area) */
    grCullMode(GR_CULL_NEGATIVE);

    /* Generate and upload checkerboard texture */
    generate_checkerboard();
    grTexDownloadMipMapLevel(
        0,                      /* tmu */
        0,                      /* startAddress (VRAM-relative to tex area) */
        GR_LOD_LOG2_64,         /* thisLod */
        GR_LOD_LOG2_64,         /* largeLod */
        GR_ASPECT_LOG2_1x1,    /* aspect */
        GR_TEXFMT_RGB_565,     /* format */
        tex_data                /* data */
    );
    grTexSource(0, 0, GR_LOD_LOG2_64, GR_LOD_LOG2_64,
                GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565);
    grTexClampMode(0, GR_TEXCLAMP_WRAP, GR_TEXCLAMP_WRAP);
    grTexFilterMode(0, GR_TEXTUREFILTER_POINT_SAMPLED,
                    GR_TEXTUREFILTER_POINT_SAMPLED);

    /* Render loop */
    for (frame = 0; frame < NUM_FRAMES; frame++) {
        /* Clear color (dark blue) and depth buffer */
        grBufferClear(0xFF102040, 0, 0xFFFF);

        /* Draw 12 triangles (6 faces x 2 tris) */
        for (tri = 0; tri < 12; tri++) {
            int face = tri / 2;
            float cr = (float)face_colors[face][0];
            float cg = (float)face_colors[face][1];
            float cb = (float)face_colors[face][2];

            transform_vertex(cube_verts[cube_tris[tri][0]],
                             angle_x, angle_y, &va,
                             cr, cg, cb,
                             tri_uv[tri][0][0], tri_uv[tri][0][1]);
            transform_vertex(cube_verts[cube_tris[tri][1]],
                             angle_x, angle_y, &vb,
                             cr, cg, cb,
                             tri_uv[tri][1][0], tri_uv[tri][1][1]);
            transform_vertex(cube_verts[cube_tris[tri][2]],
                             angle_x, angle_y, &vc,
                             cr, cg, cb,
                             tri_uv[tri][2][0], tri_uv[tri][2][1]);

            grDrawTriangle(&va, &vb, &vc);
        }

        grBufferSwap(1);

        angle_x += 1;
        angle_y += 2;
    }

    puts_uart("cube: ");
    put_int(NUM_FRAMES);
    puts_uart(" frames rendered\r\n");

    grSstWinClose();
    grGlideShutdown();

    /* Halt */
    __asm__ volatile("halt");
    for (;;) ;
}
