/* glide_raster.c : Triton GPU — Glide 3.0 software rasterizer */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#include "glide_raster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/************************************************************
 * Big-endian memory helpers
 ************************************************************/

/** Read a 32-bit big-endian float from guest memory. */
static float
read_guest_float(const uint8_t *mem, uint32_t addr)
{
    uint32_t raw = ((uint32_t)mem[addr] << 24) |
                   ((uint32_t)mem[addr + 1] << 16) |
                   ((uint32_t)mem[addr + 2] << 8) |
                   ((uint32_t)mem[addr + 3]);
    float f;
    memcpy(&f, &raw, sizeof(f));
    return f;
}

/** Read a 32-bit big-endian unsigned integer from guest memory. */
static uint32_t
read_guest_u32(const uint8_t *mem, uint32_t addr)
{
    return ((uint32_t)mem[addr] << 24) |
           ((uint32_t)mem[addr + 1] << 16) |
           ((uint32_t)mem[addr + 2] << 8) |
           ((uint32_t)mem[addr + 3]);
}

/** Write a 16-bit big-endian value to VRAM. */
static void
write_vram_u16(uint8_t *vram, uint32_t off, uint16_t val)
{
    vram[off]     = (uint8_t)(val >> 8);
    vram[off + 1] = (uint8_t)(val);
}

/** Read a 16-bit big-endian value from VRAM. */
static uint16_t
read_vram_u16(const uint8_t *vram, uint32_t off)
{
    return (uint16_t)((vram[off] << 8) | vram[off + 1]);
}

/************************************************************
 * Guest pointer resolution
 ************************************************************/

/** Resolve a guest address to a host pointer.
 *  RAM: 0x00000000 .. 0x007FFFFF
 *  VRAM: 0x00800000 .. 0x00FFFFFF */
static uint8_t *
guest_ptr(glide_state *gs, uint32_t addr)
{
    if (addr < 0x00800000)
        return gs->ram + addr;
    if (addr >= 0x00800000 && addr < 0x01000000)
        return gs->vram + (addr - 0x00800000);
    return NULL;
}

/************************************************************
 * RGB565 helpers
 ************************************************************/

/** Pack 0-255 RGB components into RGB565. */
static uint16_t
pack_rgb565(int r, int g, int b)
{
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/** Unpack RGB565 to 0-255 components. */
static void
unpack_rgb565(uint16_t c, int *r, int *g, int *b)
{
    int r5 = (c >> 11) & 0x1F;
    int g6 = (c >> 5) & 0x3F;
    int b5 = c & 0x1F;
    *r = (r5 << 3) | (r5 >> 2);
    *g = (g6 << 2) | (g6 >> 4);
    *b = (b5 << 3) | (b5 >> 2);
}

/************************************************************
 * Raster vertex
 ************************************************************/

typedef struct raster_vertex {
    float x, y, z, w;
    float r, g, b, a;
    float s, t;
} raster_vertex;

/************************************************************
 * Vertex reading
 ************************************************************/

/** Read a vertex from guest RAM using configured layout offsets. */
static void
read_vertex(glide_state *gs, uint32_t addr, raster_vertex *v)
{
    memset(v, 0, sizeof(*v));
    v->w = 1.0f;
    v->a = 255.0f;

    if (gs->vl_xy_offset >= 0) {
        v->x = read_guest_float(gs->ram, addr + gs->vl_xy_offset);
        v->y = read_guest_float(gs->ram, addr + gs->vl_xy_offset + 4);
    }
    if (gs->vl_z_offset >= 0)
        v->z = read_guest_float(gs->ram, addr + gs->vl_z_offset);
    if (gs->vl_w_offset >= 0)
        v->w = read_guest_float(gs->ram, addr + gs->vl_w_offset);
    if (gs->vl_rgb_offset >= 0) {
        v->r = read_guest_float(gs->ram, addr + gs->vl_rgb_offset);
        v->g = read_guest_float(gs->ram, addr + gs->vl_rgb_offset + 4);
        v->b = read_guest_float(gs->ram, addr + gs->vl_rgb_offset + 8);
    }
    if (gs->vl_a_offset >= 0)
        v->a = read_guest_float(gs->ram, addr + gs->vl_a_offset);
    if (gs->vl_pargb_offset >= 0) {
        uint32_t packed = read_guest_u32(gs->ram, addr + gs->vl_pargb_offset);
        v->a = (float)((packed >> 24) & 0xFF);
        v->r = (float)((packed >> 16) & 0xFF);
        v->g = (float)((packed >> 8) & 0xFF);
        v->b = (float)(packed & 0xFF);
    }
    if (gs->vl_st0_offset >= 0) {
        v->s = read_guest_float(gs->ram, addr + gs->vl_st0_offset);
        v->t = read_guest_float(gs->ram, addr + gs->vl_st0_offset + 4);
    }
}

/************************************************************
 * Texture sampling
 ************************************************************/

/** Sample a texel from VRAM at the given (s,t) integer coordinates. */
static void
tex_sample(glide_state *gs, int s, int t,
           int *tr, int *tg, int *tb, int *ta)
{
    int tw = gs->tex_width;
    int th = gs->tex_height;

    /* Clamp or wrap */
    if (gs->tex_clamp_s == GR_TEXCLAMP_CLAMP) {
        if (s < 0) s = 0;
        if (s >= tw) s = tw - 1;
    } else {
        s = s % tw;
        if (s < 0) s += tw;
    }
    if (gs->tex_clamp_t == GR_TEXCLAMP_CLAMP) {
        if (t < 0) t = 0;
        if (t >= th) t = th - 1;
    } else {
        t = t % th;
        if (t < 0) t += th;
    }

    uint32_t base = gs->tex_base;

    switch (gs->tex_format) {
    case GR_TEXFMT_RGB_565: {
        uint32_t off = base + (t * tw + s) * 2;
        uint16_t px = read_vram_u16(gs->vram, off);
        unpack_rgb565(px, tr, tg, tb);
        *ta = 255;
        break;
    }
    case GR_TEXFMT_ARGB_1555: {
        uint32_t off = base + (t * tw + s) * 2;
        uint16_t px = read_vram_u16(gs->vram, off);
        *ta = (px & 0x8000) ? 255 : 0;
        int r5 = (px >> 10) & 0x1F;
        int g5 = (px >> 5) & 0x1F;
        int b5 = px & 0x1F;
        *tr = (r5 << 3) | (r5 >> 2);
        *tg = (g5 << 3) | (g5 >> 2);
        *tb = (b5 << 3) | (b5 >> 2);
        break;
    }
    case GR_TEXFMT_ARGB_4444: {
        uint32_t off = base + (t * tw + s) * 2;
        uint16_t px = read_vram_u16(gs->vram, off);
        int a4 = (px >> 12) & 0xF;
        int r4 = (px >> 8) & 0xF;
        int g4 = (px >> 4) & 0xF;
        int b4 = px & 0xF;
        *ta = (a4 << 4) | a4;
        *tr = (r4 << 4) | r4;
        *tg = (g4 << 4) | g4;
        *tb = (b4 << 4) | b4;
        break;
    }
    case GR_TEXFMT_INTENSITY_8: {
        uint32_t off = base + t * tw + s;
        uint8_t px = gs->vram[off];
        *tr = *tg = *tb = px;
        *ta = 255;
        break;
    }
    case GR_TEXFMT_ALPHA_8: {
        uint32_t off = base + t * tw + s;
        *tr = *tg = *tb = 255;
        *ta = gs->vram[off];
        break;
    }
    default:
        /* Magenta for unsupported formats */
        *tr = 255;
        *tg = 0;
        *tb = 255;
        *ta = 255;
        break;
    }
}

/************************************************************
 * Color combine system
 ************************************************************/

/** Evaluate one channel of the Glide combine equation. */
static float
combine_channel(float local, float other, float factor,
                int function, float local_alpha)
{
    switch (function) {
    case GR_COMBINE_FUNCTION_ZERO:
        return 0.0f;
    case GR_COMBINE_FUNCTION_LOCAL:
        return local;
    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        return local_alpha;
    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        return other * factor;
    case GR_COMBINE_FUNCTION_BLEND_OTHER:
        return other * factor + local * (1.0f - factor);
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
        return other * factor + local;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        return other * factor + local_alpha;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        return (other - local) * factor;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
        return (other - local) * factor + local;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        return (other - local) * factor + local_alpha;
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
        return -local * factor + local;
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        return -local * factor + local_alpha;
    default:
        return local;
    }
}

/** Select the combine factor value (0.0-1.0) based on Glide enum. */
static float
select_factor(int factor_enum, float local_val, float local_alpha,
              float other_alpha, float tex_alpha)
{
    switch (factor_enum) {
    case GR_COMBINE_FACTOR_ZERO:
        return 0.0f;
    case GR_COMBINE_FACTOR_LOCAL:
        return local_val / 255.0f;
    case GR_COMBINE_FACTOR_OTHER_ALPHA:
        return other_alpha / 255.0f;
    case GR_COMBINE_FACTOR_LOCAL_ALPHA:
        return local_alpha / 255.0f;
    case GR_COMBINE_FACTOR_TEXTURE_ALPHA:
        return tex_alpha / 255.0f;
    case GR_COMBINE_FACTOR_ONE:
        return 1.0f;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL:
        return 1.0f - local_val / 255.0f;
    case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA:
        return 1.0f - other_alpha / 255.0f;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA:
        return 1.0f - local_alpha / 255.0f;
    case GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA:
        return 1.0f - tex_alpha / 255.0f;
    default:
        return 0.0f;
    }
}

/** Full RGB color combine using the gs->cc_* state. */
static void
color_combine(glide_state *gs,
              float iter_r, float iter_g, float iter_b, float iter_a,
              float tex_r, float tex_g, float tex_b, float tex_a,
              float *out_r, float *out_g, float *out_b)
{
    /* Select local source */
    float local_r, local_g, local_b, local_a;
    switch (gs->cc_local) {
    case GR_COMBINE_LOCAL_ITERATED:
        local_r = iter_r;
        local_g = iter_g;
        local_b = iter_b;
        local_a = iter_a;
        break;
    case GR_COMBINE_LOCAL_CONSTANT:
        local_r = (float)((gs->constant_color >> 16) & 0xFF);
        local_g = (float)((gs->constant_color >> 8) & 0xFF);
        local_b = (float)(gs->constant_color & 0xFF);
        local_a = (float)((gs->constant_color >> 24) & 0xFF);
        break;
    default:
        local_r = local_g = local_b = local_a = 0.0f;
        break;
    }

    /* Select other source */
    float other_r, other_g, other_b, other_a;
    switch (gs->cc_other) {
    case GR_COMBINE_OTHER_ITERATED:
        other_r = iter_r;
        other_g = iter_g;
        other_b = iter_b;
        other_a = iter_a;
        break;
    case GR_COMBINE_OTHER_TEXTURE:
        other_r = tex_r;
        other_g = tex_g;
        other_b = tex_b;
        other_a = tex_a;
        break;
    case GR_COMBINE_OTHER_CONSTANT:
        other_r = (float)((gs->constant_color >> 16) & 0xFF);
        other_g = (float)((gs->constant_color >> 8) & 0xFF);
        other_b = (float)(gs->constant_color & 0xFF);
        other_a = (float)((gs->constant_color >> 24) & 0xFF);
        break;
    default:
        other_r = other_g = other_b = other_a = 0.0f;
        break;
    }

    /* Compute per-channel factors */
    float factor_r = select_factor(gs->cc_factor, local_r, local_a,
                                   other_a, tex_a);
    float factor_g = select_factor(gs->cc_factor, local_g, local_a,
                                   other_a, tex_a);
    float factor_b = select_factor(gs->cc_factor, local_b, local_a,
                                   other_a, tex_a);

    /* Evaluate per-channel */
    *out_r = combine_channel(local_r, other_r, factor_r,
                             gs->cc_function, local_a);
    *out_g = combine_channel(local_g, other_g, factor_g,
                             gs->cc_function, local_a);
    *out_b = combine_channel(local_b, other_b, factor_b,
                             gs->cc_function, local_a);

    /* Invert */
    if (gs->cc_invert) {
        *out_r = 255.0f - *out_r;
        *out_g = 255.0f - *out_g;
        *out_b = 255.0f - *out_b;
    }
}

/** Alpha channel combine using the gs->ac_* state. */
static float
alpha_combine(glide_state *gs, float iter_a, float tex_a)
{
    /* Select local */
    float local_a;
    switch (gs->ac_local) {
    case GR_COMBINE_LOCAL_ITERATED:
        local_a = iter_a;
        break;
    case GR_COMBINE_LOCAL_CONSTANT:
        local_a = (float)((gs->constant_color >> 24) & 0xFF);
        break;
    default:
        local_a = 0.0f;
        break;
    }

    /* Select other */
    float other_a;
    switch (gs->ac_other) {
    case GR_COMBINE_OTHER_ITERATED:
        other_a = iter_a;
        break;
    case GR_COMBINE_OTHER_TEXTURE:
        other_a = tex_a;
        break;
    case GR_COMBINE_OTHER_CONSTANT:
        other_a = (float)((gs->constant_color >> 24) & 0xFF);
        break;
    default:
        other_a = 0.0f;
        break;
    }

    float factor = select_factor(gs->ac_factor, local_a, local_a,
                                 other_a, tex_a);
    float result = combine_channel(local_a, other_a, factor,
                                   gs->ac_function, local_a);

    if (gs->ac_invert)
        result = 255.0f - result;
    return result;
}

/************************************************************
 * Depth compare
 ************************************************************/

/** Compare two 16-bit depth values per GR_CMP_* constants. */
static int
depth_compare(int func, uint16_t incoming, uint16_t stored)
{
    switch (func) {
    case GR_CMP_NEVER:    return 0;
    case GR_CMP_LESS:     return incoming < stored;
    case GR_CMP_EQUAL:    return incoming == stored;
    case GR_CMP_LEQUAL:   return incoming <= stored;
    case GR_CMP_GREATER:  return incoming > stored;
    case GR_CMP_NOTEQUAL: return incoming != stored;
    case GR_CMP_GEQUAL:   return incoming >= stored;
    case GR_CMP_ALWAYS:   return 1;
    default:              return 1;
    }
}

/************************************************************
 * Alpha blend factor
 ************************************************************/

/** Return a 0.0-1.0 blend factor for the given GR_BLEND_* enum. */
static float
blend_factor(int func, float src_a, float dst_a)
{
    switch (func) {
    case GR_BLEND_ZERO:                  return 0.0f;
    case GR_BLEND_SRC_ALPHA:             return src_a / 255.0f;
    case GR_BLEND_DST_ALPHA:             return dst_a / 255.0f;
    case GR_BLEND_ONE:                   return 1.0f;
    case GR_BLEND_ONE_MINUS_SRC_ALPHA:   return 1.0f - src_a / 255.0f;
    case GR_BLEND_ONE_MINUS_DST_ALPHA:   return 1.0f - dst_a / 255.0f;
    default:                             return 1.0f;
    }
}

/************************************************************
 * Fog
 ************************************************************/

/** Apply q-based fog to an RGB color using the fog table.
 *  The fog table is indexed by q (= 1/w = oow), with logarithmic spacing:
 *  table entry i corresponds to q ≈ 2^(i/4).  See Glide Programming Guide
 *  §8, guFogTableIndexToW().  */
static void
apply_fog(glide_state *gs, float q, float *r, float *g, float *b)
{
    if (gs->fog_mode == GR_FOG_DISABLE)
        return;

    /* Map q (oow) to fog table index via i = 4 * log2(q), clamped 0..63.
     * q <= 0 means behind the camera — full fog. */
    int idx;
    if (q <= 0.0f) {
        idx = 63;
    } else {
        float fi = 4.0f * log2f(q);
        idx = (int)(fi + 0.5f);
        if (idx < 0) idx = 0;
        if (idx > 63) idx = 63;
    }

    float fog = (float)gs->fog_table[idx] / 255.0f;
    float fog_r = (float)((gs->fog_color >> 16) & 0xFF);
    float fog_g = (float)((gs->fog_color >> 8) & 0xFF);
    float fog_b = (float)(gs->fog_color & 0xFF);

    *r = *r * (1.0f - fog) + fog_r * fog;
    *g = *g * (1.0f - fog) + fog_g * fog;
    *b = *b * (1.0f - fog) + fog_b * fog;
}

/************************************************************
 * Triangle rasterizer
 ************************************************************/

/** Rasterize a triangle using half-space (edge function) method. */
static void
draw_triangle(glide_state *gs,
              const raster_vertex *v0,
              const raster_vertex *v1,
              const raster_vertex *v2)
{
    /* Fixed-point subpixel precision: 4 fractional bits */
    int x0 = (int)(v0->x * 16.0f + 0.5f);
    int y0 = (int)(v0->y * 16.0f + 0.5f);
    int x1 = (int)(v1->x * 16.0f + 0.5f);
    int y1 = (int)(v1->y * 16.0f + 0.5f);
    int x2 = (int)(v2->x * 16.0f + 0.5f);
    int y2 = (int)(v2->y * 16.0f + 0.5f);

    /* Signed area (2x, in fixed-point) */
    int area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);

    /* Degenerate triangle — skip */
    if (area == 0)
        return;

    /* Backface culling */
    if (gs->cull_mode == GR_CULL_POSITIVE && area > 0)
        return;
    if (gs->cull_mode == GR_CULL_NEGATIVE && area < 0)
        return;

    /* Ensure CCW winding (positive area) — swap v1 and v2 if needed */
    if (area < 0) {
        const raster_vertex *tmp = v1;
        v1 = v2;
        v2 = tmp;
        int tx = x1; x1 = x2; x2 = tx;
        int ty = y1; y1 = y2; y2 = ty;
        area = -area;
    }

    float inv_area = 1.0f / (float)area;

    /* Bounding box in pixels, clipped to scissor */
    int minx = (x0 < x1 ? x0 : x1);
    if (x2 < minx) minx = x2;
    int miny = (y0 < y1 ? y0 : y1);
    if (y2 < miny) miny = y2;
    int maxx = (x0 > x1 ? x0 : x1);
    if (x2 > maxx) maxx = x2;
    int maxy = (y0 > y1 ? y0 : y1);
    if (y2 > maxy) maxy = y2;

    /* Convert to pixels (round outward) */
    minx = (minx + 15) >> 4;
    miny = (miny + 15) >> 4;
    maxx = maxx >> 4;
    maxy = maxy >> 4;

    /* Clip to scissor rect */
    if (minx < gs->clip_x0) minx = gs->clip_x0;
    if (miny < gs->clip_y0) miny = gs->clip_y0;
    if (maxx >= gs->clip_x1) maxx = gs->clip_x1 - 1;
    if (maxy >= gs->clip_y1) maxy = gs->clip_y1 - 1;

    /* Edge function coefficients:
     *   E01(x,y) = (y0 - y1)*x + (x1 - x0)*y + (x0*y1 - x1*y0)
     *   E12(x,y) = (y1 - y2)*x + (x2 - x1)*y + (x1*y2 - x2*y1)
     *   E20(x,y) = (y2 - y0)*x + (x0 - x2)*y + (x2*y0 - x0*y2)
     */
    int a01 = y0 - y1, b01 = x1 - x0;
    int a12 = y1 - y2, b12 = x2 - x1;
    int a20 = y2 - y0, b20 = x0 - x2;

    /* Starting pixel center in fixed-point */
    int px0 = (minx << 4) + 8;
    int py0 = (miny << 4) + 8;

    int w0_row = a12 * (px0 - x1) + b12 * (py0 - y1);
    int w1_row = a20 * (px0 - x2) + b20 * (py0 - y2);
    int w2_row = a01 * (px0 - x0) + b01 * (py0 - y0);

    /* Top-left fill rule bias */
    int bias0 = (a12 > 0 || (a12 == 0 && b12 > 0)) ? 0 : -1;
    int bias1 = (a20 > 0 || (a20 == 0 && b20 > 0)) ? 0 : -1;
    int bias2 = (a01 > 0 || (a01 == 0 && b01 > 0)) ? 0 : -1;

    /* Step amounts for moving one pixel (16 subpixel units) */
    int a12_step = a12 * 16;
    int b12_step = b12 * 16;
    int a20_step = a20 * 16;
    int b20_step = b20 * 16;
    int a01_step = a01 * 16;
    int b01_step = b01 * 16;

    /* Is texturing active? */
    int texturing = (gs->tex_base != 0 && gs->tex_width > 0 &&
                     gs->tex_height > 0);

    for (int py = miny; py <= maxy; py++) {
        int w0 = w0_row;
        int w1 = w1_row;
        int w2 = w2_row;

        for (int px = minx; px <= maxx; px++) {
            /* Inside test with top-left fill rule */
            if ((w0 + bias0) < 0 || (w1 + bias1) < 0 || (w2 + bias2) < 0)
                goto next_pixel;

            {
                /* Barycentric coordinates */
                float bary0 = (float)w0 * inv_area;
                float bary1 = (float)w1 * inv_area;
                float bary2 = 1.0f - bary0 - bary1;

                /* Interpolate vertex attributes */
                float frag_r = v0->r * bary0 + v1->r * bary1 + v2->r * bary2;
                float frag_g = v0->g * bary0 + v1->g * bary1 + v2->g * bary2;
                float frag_b = v0->b * bary0 + v1->b * bary1 + v2->b * bary2;
                float frag_a = v0->a * bary0 + v1->a * bary1 + v2->a * bary2;
                float frag_z = v0->z * bary0 + v1->z * bary1 + v2->z * bary2;
                float frag_w = v0->w * bary0 + v1->w * bary1 + v2->w * bary2;

                /* Texture sampling with perspective correction.
                 * Vertex s,t are already s/w and t/w; vertex w is 1/w.
                 * Interpolate all three linearly, then divide to recover
                 * texel coordinates: s = (s/w) / (1/w). */
                float tex_r = 0, tex_g = 0, tex_b = 0, tex_a = 255;
                if (texturing) {
                    float sow = v0->s * bary0 + v1->s * bary1 +
                                v2->s * bary2;
                    float tow = v0->t * bary0 + v1->t * bary1 +
                                v2->t * bary2;
                    float oow = v0->w * bary0 + v1->w * bary1 +
                                v2->w * bary2;

                    if (oow > 0.0f) {
                        sow /= oow;
                        tow /= oow;
                    }

                    int ti_s = (int)sow;
                    int ti_t = (int)tow;
                    int sample_r, sample_g, sample_b, sample_a;
                    tex_sample(gs, ti_s, ti_t,
                               &sample_r, &sample_g, &sample_b, &sample_a);
                    tex_r = (float)sample_r;
                    tex_g = (float)sample_g;
                    tex_b = (float)sample_b;
                    tex_a = (float)sample_a;
                }

                /* Color combine */
                float cc_r, cc_g, cc_b;
                color_combine(gs, frag_r, frag_g, frag_b, frag_a,
                              tex_r, tex_g, tex_b, tex_a,
                              &cc_r, &cc_g, &cc_b);
                float cc_a = alpha_combine(gs, frag_a, tex_a);

                /* Fog */
                apply_fog(gs, frag_w, &cc_r, &cc_g, &cc_b);

                /* Alpha test */
                if (gs->alpha_test_func != GR_CMP_ALWAYS) {
                    int ai = (int)(cc_a + 0.5f);
                    if (ai < 0) ai = 0;
                    if (ai > 255) ai = 255;
                    if (!depth_compare(gs->alpha_test_func,
                                       (uint16_t)ai,
                                       (uint16_t)gs->alpha_test_ref))
                        goto next_pixel;
                }

                /* Depth test */
                uint32_t fb_off = gs->draw_offset +
                                  ((uint32_t)py * GR_SCREEN_W + (uint32_t)px) * 2;
                uint32_t zb_off = gs->zbuf_offset +
                                  ((uint32_t)py * GR_SCREEN_W + (uint32_t)px) * 2;

                if (gs->depth_mode != GR_DEPTHBUFFER_DISABLE) {
                    int zi = (int)(frag_z * 65535.0f + 0.5f);
                    if (zi < 0) zi = 0;
                    if (zi > 65535) zi = 65535;
                    int z_biased = zi + gs->depth_bias;
                    if (z_biased < 0) z_biased = 0;
                    if (z_biased > 65535) z_biased = 65535;
                    uint16_t z16 = (uint16_t)z_biased;
                    uint16_t stored_z = read_vram_u16(gs->vram, zb_off);
                    if (!depth_compare(gs->depth_func, z16, stored_z))
                        goto next_pixel;
                    if (gs->depth_mask)
                        write_vram_u16(gs->vram, zb_off, z16);
                }

                /* Alpha blend */
                int final_r = (int)(cc_r + 0.5f);
                int final_g = (int)(cc_g + 0.5f);
                int final_b = (int)(cc_b + 0.5f);

                if (gs->ab_rgb_src != GR_BLEND_ONE ||
                    gs->ab_rgb_dst != GR_BLEND_ZERO) {
                    uint16_t dst_px = read_vram_u16(gs->vram, fb_off);
                    int dst_r, dst_g, dst_b;
                    unpack_rgb565(dst_px, &dst_r, &dst_g, &dst_b);
                    float dst_a = 255.0f; // framebuffer has no alpha

                    float sf = blend_factor(gs->ab_rgb_src, cc_a, dst_a);
                    float df = blend_factor(gs->ab_rgb_dst, cc_a, dst_a);

                    final_r = (int)(cc_r * sf + (float)dst_r * df + 0.5f);
                    final_g = (int)(cc_g * sf + (float)dst_g * df + 0.5f);
                    final_b = (int)(cc_b * sf + (float)dst_b * df + 0.5f);
                }

                /* Write pixel */
                if (gs->color_mask_rgb)
                    write_vram_u16(gs->vram, fb_off,
                                   pack_rgb565(final_r, final_g, final_b));
            }

        next_pixel:
            w0 += a12_step;
            w1 += a20_step;
            w2 += a01_step;
        }

        w0_row += b12_step;
        w1_row += b20_step;
        w2_row += b01_step;
    }
}

/************************************************************
 * Line drawing
 ************************************************************/

/** Draw a line using Bresenham's algorithm with color interpolation. */
static void
draw_line(glide_state *gs,
          const raster_vertex *v0, const raster_vertex *v1)
{
    int ix0 = (int)(v0->x + 0.5f);
    int iy0 = (int)(v0->y + 0.5f);
    int ix1 = (int)(v1->x + 0.5f);
    int iy1 = (int)(v1->y + 0.5f);

    int dx = abs(ix1 - ix0);
    int dy = abs(iy1 - iy0);
    int sx = (ix0 < ix1) ? 1 : -1;
    int sy = (iy0 < iy1) ? 1 : -1;
    int err = dx - dy;

    int steps = (dx > dy) ? dx : dy;
    if (steps == 0)
        steps = 1;
    float inv_steps = 1.0f / (float)steps;

    int step = 0;
    for (;;) {
        float t = (float)step * inv_steps;
        float r = v0->r + (v1->r - v0->r) * t;
        float g = v0->g + (v1->g - v0->g) * t;
        float b = v0->b + (v1->b - v0->b) * t;

        if (ix0 >= gs->clip_x0 && ix0 < gs->clip_x1 &&
            iy0 >= gs->clip_y0 && iy0 < gs->clip_y1) {
            uint32_t off = gs->draw_offset +
                           ((uint32_t)iy0 * GR_SCREEN_W + (uint32_t)ix0) * 2;
            write_vram_u16(gs->vram, off,
                           pack_rgb565((int)(r + 0.5f),
                                       (int)(g + 0.5f),
                                       (int)(b + 0.5f)));
        }

        if (ix0 == ix1 && iy0 == iy1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            ix0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            iy0 += sy;
        }
        step++;
    }
}

/************************************************************
 * Point drawing
 ************************************************************/

/** Draw a single point, clipped to the scissor rect. */
static void
draw_point(glide_state *gs, const raster_vertex *v)
{
    int px = (int)(v->x + 0.5f);
    int py = (int)(v->y + 0.5f);

    if (px < gs->clip_x0 || px >= gs->clip_x1 ||
        py < gs->clip_y0 || py >= gs->clip_y1)
        return;

    uint32_t off = gs->draw_offset +
                   ((uint32_t)py * GR_SCREEN_W + (uint32_t)px) * 2;
    write_vram_u16(gs->vram, off,
                   pack_rgb565((int)(v->r + 0.5f),
                               (int)(v->g + 0.5f),
                               (int)(v->b + 0.5f)));
}

/************************************************************
 * Texture memory management
 ************************************************************/

/** Compute the byte size of one mip level, accounting for aspect ratio. */
static uint32_t
tex_level_size(int format, int lod, int aspect)
{
    int base_dim = 1 << lod;
    int w, h;
    if (aspect >= 0) {
        w = base_dim;
        h = base_dim >> aspect;
        if (h < 1) h = 1;
    } else {
        h = base_dim;
        w = base_dim >> (-aspect);
        if (w < 1) w = 1;
    }
    int pixels = w * h;

    switch (format) {
    case GR_TEXFMT_RGB_565:
    case GR_TEXFMT_ARGB_1555:
    case GR_TEXFMT_ARGB_4444:
    case GR_TEXFMT_ALPHA_INTENSITY_88:
        return (uint32_t)(pixels * 2);
    case GR_TEXFMT_RGB_332:
    case GR_TEXFMT_ALPHA_8:
    case GR_TEXFMT_INTENSITY_8:
    case GR_TEXFMT_ALPHA_INTENSITY_44:
    case GR_TEXFMT_P_8:
        return (uint32_t)pixels;
    default:
        return (uint32_t)(pixels * 2);
    }
}

/** Compute total byte size for a mip chain from large_lod down to small_lod. */
static uint32_t
tex_mipmap_size(int format, int large_lod, int small_lod, int aspect)
{
    uint32_t total = 0;
    for (int lod = large_lod; lod >= small_lod; lod--)
        total += tex_level_size(format, lod, aspect);
    return total;
}

/************************************************************
 * Glide API implementation (hypercall handlers)
 ************************************************************/

static int
hc_glide_init(glide_state *gs, cf_cpu *cpu)
{
    (void)cpu;
    gs->initialized = 1;
    printf("[glide] Glide 3.10 -- Vertex Technologies, Inc.\n");
    printf("[glide] Triton GPU (Banshee SST-2 derivative)\n");
    return 0;
}

static int
hc_glide_shutdown(glide_state *gs, cf_cpu *cpu)
{
    (void)cpu;
    gs->initialized = 0;
    gs->win_open = 0;
    printf("[glide] grGlideShutdown\n");
    return 0;
}

static int
hc_sst_win_open(glide_state *gs, cf_cpu *cpu)
{
    (void)cpu;
    gs->win_open = 1;
    gs->front_offset = GR_FRONT_OFFSET;
    gs->back_offset = GR_BACK_OFFSET;
    gs->zbuf_offset = GR_ZBUF_OFFSET;
    gs->draw_offset = gs->back_offset;
    gs->origin = GR_ORIGIN_UPPER_LEFT;
    gs->clip_x0 = 0;
    gs->clip_y0 = 0;
    gs->clip_x1 = GR_SCREEN_W;
    gs->clip_y1 = GR_SCREEN_H;
    gs->depth_func = GR_CMP_LESS;
    gs->depth_mask = 1;
    gs->alpha_test_func = GR_CMP_ALWAYS;
    gs->color_mask_rgb = 1;
    gs->color_mask_a = 1;
    gs->ab_rgb_src = GR_BLEND_ONE;
    gs->ab_rgb_dst = GR_BLEND_ZERO;
    gs->ab_alpha_src = GR_BLEND_ONE;
    gs->ab_alpha_dst = GR_BLEND_ZERO;
    gs->cc_function = GR_COMBINE_FUNCTION_LOCAL;
    gs->cc_factor = GR_COMBINE_FACTOR_ZERO;
    gs->cc_local = GR_COMBINE_LOCAL_ITERATED;
    gs->cc_other = GR_COMBINE_OTHER_ITERATED;
    gs->ac_function = GR_COMBINE_FUNCTION_LOCAL;
    gs->ac_factor = GR_COMBINE_FACTOR_ZERO;
    gs->ac_local = GR_COMBINE_LOCAL_ITERATED;
    gs->ac_other = GR_COMBINE_OTHER_ITERATED;
    printf("[glide] grSstWinOpen: %dx%d RGB565, 8 MB VRAM\n",
           GR_SCREEN_W, GR_SCREEN_H);
    return 0;
}

static int
hc_sst_win_close(glide_state *gs, cf_cpu *cpu)
{
    (void)cpu;
    gs->win_open = 0;
    printf("[glide] grSstWinClose\n");
    return 0;
}

static int
hc_buffer_clear(glide_state *gs, cf_cpu *cpu)
{
    uint32_t color = cpu->d[0];
    uint32_t depth = cpu->d[2];

    int cr = (color >> 16) & 0xFF;
    int cg = (color >> 8) & 0xFF;
    int cb = color & 0xFF;
    uint16_t c565 = pack_rgb565(cr, cg, cb);
    uint16_t z16 = (uint16_t)(depth & 0xFFFF);

    /* Clear current render target */
    for (uint32_t off = 0; off < GR_FB_SIZE; off += 2)
        write_vram_u16(gs->vram, gs->draw_offset + off, c565);

    /* Clear depth buffer if enabled */
    if (gs->depth_mode != GR_DEPTHBUFFER_DISABLE) {
        for (uint32_t off = 0; off < GR_FB_SIZE; off += 2)
            write_vram_u16(gs->vram, gs->zbuf_offset + off, z16);
    }

    return 0;
}

static int
hc_buffer_swap(glide_state *gs, cf_cpu *cpu)
{
    int interval = (int)cpu->d[0];
    uint32_t tmp = gs->front_offset;
    gs->front_offset = gs->back_offset;
    gs->back_offset = tmp;
    gs->draw_offset = gs->back_offset;

    /* Signal the main loop to wait for vblank before resuming */
    if (interval > 0) {
        gs->vblank_wait = interval;
        cpu->halted = 1;
    }
    return 0;
}

static int
hc_render_buffer(glide_state *gs, cf_cpu *cpu)
{
    int buf = (int)cpu->d[0];
    if (buf == GR_BUFFER_FRONTBUFFER)
        gs->draw_offset = gs->front_offset;
    else
        gs->draw_offset = gs->back_offset;
    return 0;
}

static int
hc_clip_window(glide_state *gs, cf_cpu *cpu)
{
    gs->clip_x0 = (int)cpu->d[0];
    gs->clip_y0 = (int)cpu->d[1];
    gs->clip_x1 = (int)cpu->d[2];
    gs->clip_y1 = (int)cpu->d[3];

    /* Clamp to screen */
    if (gs->clip_x0 < 0) gs->clip_x0 = 0;
    if (gs->clip_y0 < 0) gs->clip_y0 = 0;
    if (gs->clip_x1 > GR_SCREEN_W) gs->clip_x1 = GR_SCREEN_W;
    if (gs->clip_y1 > GR_SCREEN_H) gs->clip_y1 = GR_SCREEN_H;
    return 0;
}

static int
hc_sst_origin(glide_state *gs, cf_cpu *cpu)
{
    gs->origin = (int)cpu->d[0];
    return 0;
}

static int
hc_draw_triangle(glide_state *gs, cf_cpu *cpu)
{
    raster_vertex v0, v1, v2;
    read_vertex(gs, cpu->a[0], &v0);
    read_vertex(gs, cpu->a[1], &v1);
    read_vertex(gs, cpu->a[2], &v2);

    /* Flip Y for lower-left origin */
    if (gs->origin == GR_ORIGIN_LOWER_LEFT) {
        v0.y = (float)(GR_SCREEN_H - 1) - v0.y;
        v1.y = (float)(GR_SCREEN_H - 1) - v1.y;
        v2.y = (float)(GR_SCREEN_H - 1) - v2.y;
    }

    draw_triangle(gs, &v0, &v1, &v2);
    return 0;
}

static int
hc_draw_line(glide_state *gs, cf_cpu *cpu)
{
    raster_vertex v0, v1;
    read_vertex(gs, cpu->a[0], &v0);
    read_vertex(gs, cpu->a[1], &v1);

    if (gs->origin == GR_ORIGIN_LOWER_LEFT) {
        v0.y = (float)(GR_SCREEN_H - 1) - v0.y;
        v1.y = (float)(GR_SCREEN_H - 1) - v1.y;
    }

    draw_line(gs, &v0, &v1);
    return 0;
}

static int
hc_draw_point(glide_state *gs, cf_cpu *cpu)
{
    raster_vertex v;
    read_vertex(gs, cpu->a[0], &v);

    if (gs->origin == GR_ORIGIN_LOWER_LEFT)
        v.y = (float)(GR_SCREEN_H - 1) - v.y;

    draw_point(gs, &v);
    return 0;
}

static int
hc_draw_vertex_array(glide_state *gs, cf_cpu *cpu)
{
    int mode = (int)cpu->d[0];
    int count = (int)cpu->d[1];
    uint32_t ptr_array = cpu->a[0];

    /* Only GR_TRIANGLES mode supported */
    if (mode != 0) // GR_TRIANGLES = 0
        return 0;

    for (int i = 0; i + 2 < count; i += 3) {
        uint32_t a0 = read_guest_u32(gs->ram, ptr_array + (uint32_t)(i * 4));
        uint32_t a1 = read_guest_u32(gs->ram, ptr_array + (uint32_t)((i + 1) * 4));
        uint32_t a2 = read_guest_u32(gs->ram, ptr_array + (uint32_t)((i + 2) * 4));

        raster_vertex v0, v1, v2;
        read_vertex(gs, a0, &v0);
        read_vertex(gs, a1, &v1);
        read_vertex(gs, a2, &v2);

        if (gs->origin == GR_ORIGIN_LOWER_LEFT) {
            v0.y = (float)(GR_SCREEN_H - 1) - v0.y;
            v1.y = (float)(GR_SCREEN_H - 1) - v1.y;
            v2.y = (float)(GR_SCREEN_H - 1) - v2.y;
        }

        draw_triangle(gs, &v0, &v1, &v2);
    }
    return 0;
}

static int
hc_draw_vertex_array_contiguous(glide_state *gs, cf_cpu *cpu)
{
    int mode = (int)cpu->d[0];
    int count = (int)cpu->d[1];
    uint32_t base = cpu->a[0];
    uint32_t stride = cpu->d[2];

    if (mode != 0) // GR_TRIANGLES = 0
        return 0;

    for (int i = 0; i + 2 < count; i += 3) {
        raster_vertex v0, v1, v2;
        read_vertex(gs, base + (uint32_t)(i) * stride, &v0);
        read_vertex(gs, base + (uint32_t)(i + 1) * stride, &v1);
        read_vertex(gs, base + (uint32_t)(i + 2) * stride, &v2);

        if (gs->origin == GR_ORIGIN_LOWER_LEFT) {
            v0.y = (float)(GR_SCREEN_H - 1) - v0.y;
            v1.y = (float)(GR_SCREEN_H - 1) - v1.y;
            v2.y = (float)(GR_SCREEN_H - 1) - v2.y;
        }

        draw_triangle(gs, &v0, &v1, &v2);
    }
    return 0;
}

static int
hc_vertex_layout(glide_state *gs, cf_cpu *cpu)
{
    int param = (int)cpu->d[0];
    int offset = (int)cpu->d[1];
    int mode = (int)cpu->d[2];

    if (mode == GR_PARAM_DISABLE)
        offset = -1;

    switch (param) {
    case GR_PARAM_XY:      gs->vl_xy_offset = offset; break;
    case GR_PARAM_Z:       gs->vl_z_offset = offset; break;
    case GR_PARAM_W:       gs->vl_w_offset = offset; break;
    case GR_PARAM_Q:       gs->vl_q0_offset = offset; break;
    case GR_PARAM_A:       gs->vl_a_offset = offset; break;
    case GR_PARAM_RGB:     gs->vl_rgb_offset = offset; break;
    case GR_PARAM_PARGB:   gs->vl_pargb_offset = offset; break;
    case GR_PARAM_ST0:     gs->vl_st0_offset = offset; break;
    case GR_PARAM_Q0:      gs->vl_q0_offset = offset; break;
    default:
        break;
    }
    return 0;
}

static int
hc_coordinate_space(glide_state *gs, cf_cpu *cpu)
{
    gs->coord_space = (int)cpu->d[0];
    return 0;
}

static int
hc_color_combine(glide_state *gs, cf_cpu *cpu)
{
    gs->cc_function = (int)cpu->d[0];
    gs->cc_factor   = (int)cpu->d[1];
    gs->cc_local    = (int)cpu->d[2];
    gs->cc_other    = (int)cpu->d[3];
    gs->cc_invert   = (int)cpu->d[4];
    return 0;
}

static int
hc_alpha_combine(glide_state *gs, cf_cpu *cpu)
{
    gs->ac_function = (int)cpu->d[0];
    gs->ac_factor   = (int)cpu->d[1];
    gs->ac_local    = (int)cpu->d[2];
    gs->ac_other    = (int)cpu->d[3];
    gs->ac_invert   = (int)cpu->d[4];
    return 0;
}

static int
hc_alpha_blend_function(glide_state *gs, cf_cpu *cpu)
{
    gs->ab_rgb_src   = (int)cpu->d[0];
    gs->ab_rgb_dst   = (int)cpu->d[1];
    gs->ab_alpha_src = (int)cpu->d[2];
    gs->ab_alpha_dst = (int)cpu->d[3];
    return 0;
}

static int
hc_tex_source(glide_state *gs, cf_cpu *cpu)
{
    /* tmu = cpu->d[0] — ignored, single TMU */
    uint32_t start_addr = cpu->d[1];
    int large_lod = (int)cpu->d[2];
    int small_lod = (int)cpu->d[3];
    int aspect = (int32_t)cpu->d[4]; // signed
    int format = (int)cpu->d[5];

    gs->tex_base = GR_TEX_START + start_addr;
    gs->tex_large_lod = large_lod;
    gs->tex_small_lod = small_lod;
    gs->tex_aspect = aspect;
    gs->tex_format = format;

    /* Compute texture dimensions from LOD and aspect */
    int base_dim = 1 << large_lod;
    if (aspect >= 0) {
        gs->tex_width = base_dim;
        gs->tex_height = base_dim >> aspect;
        if (gs->tex_height < 1)
            gs->tex_height = 1;
    } else {
        gs->tex_height = base_dim;
        gs->tex_width = base_dim >> (-aspect);
        if (gs->tex_width < 1)
            gs->tex_width = 1;
    }

    return 0;
}

static int
hc_tex_download_mipmap_level(glide_state *gs, cf_cpu *cpu)
{
    uint32_t dest_offset = cpu->d[1];
    int lod = (int)cpu->d[2];
    int aspect = (int32_t)cpu->d[4];
    int format = (int)cpu->d[5];
    uint32_t src_ptr = cpu->a[0];

    uint32_t size = tex_level_size(format, lod, aspect);
    uint32_t dst = GR_TEX_START + dest_offset;

    /* Bounds check */
    if (dst + size > GR_TEX_END)
        return 0;

    uint8_t *src = guest_ptr(gs, src_ptr);
    if (src)
        memcpy(gs->vram + dst, src, size);

    return 0;
}

static int
hc_tex_download_mipmap(glide_state *gs, cf_cpu *cpu)
{
    /* Convenience — just calls through to the level download */
    return hc_tex_download_mipmap_level(gs, cpu);
}

static int
hc_tex_combine(glide_state *gs, cf_cpu *cpu)
{
    /* tmu = cpu->d[0] — ignored */
    gs->tc_rgb_func      = (int)cpu->d[1];
    gs->tc_rgb_factor     = (int)cpu->d[2];
    gs->tc_alpha_func     = (int)cpu->d[3];
    gs->tc_alpha_factor   = (int)cpu->d[4];
    gs->tc_rgb_invert     = (int)cpu->d[5];
    gs->tc_alpha_invert   = (int)cpu->d[6];
    return 0;
}

static int
hc_tex_clamp_mode(glide_state *gs, cf_cpu *cpu)
{
    /* tmu = cpu->d[0] — ignored */
    gs->tex_clamp_s = (int)cpu->d[1];
    gs->tex_clamp_t = (int)cpu->d[2];
    return 0;
}

static int
hc_tex_filter_mode(glide_state *gs, cf_cpu *cpu)
{
    /* tmu = cpu->d[0] — ignored */
    gs->tex_filter_min = (int)cpu->d[1];
    gs->tex_filter_mag = (int)cpu->d[2];
    return 0;
}

static int
hc_tex_mipmap_mode(glide_state *gs, cf_cpu *cpu)
{
    /* tmu = cpu->d[0] — ignored */
    gs->tex_mipmap_mode = (int)cpu->d[1];
    return 0;
}

static int
hc_get(glide_state *gs, cf_cpu *cpu)
{
    (void)gs;
    int param = (int)cpu->d[0];

    switch (param) {
    case GR_NUM_TMU:
        cpu->d[0] = 1;
        break;
    case GR_MAX_TEXTURE_SIZE:
        cpu->d[0] = 256;
        break;
    case GR_NUM_BOARDS:
        cpu->d[0] = 1;
        break;
    case GR_MEMORY_FB:
        cpu->d[0] = GR_FRONT_OFFSET + GR_FB_SIZE * 2; // front + back
        break;
    case GR_MEMORY_TMU:
        cpu->d[0] = GR_TEX_END - GR_TEX_START;
        break;
    case GR_MEMORY_UMA:
        cpu->d[0] = 0x800000;  /* 8 MB VRAM */
        break;
    default:
        cpu->d[0] = 0;
        break;
    }
    return 0;
}

static int
hc_lfb_write_region(glide_state *gs, cf_cpu *cpu)
{
    int dst_buf = (int)cpu->d[0];
    int dst_x = (int)cpu->d[1];
    int dst_y = (int)cpu->d[2];
    int w = (int)cpu->d[3];
    int h = (int)cpu->d[4];
    int src_stride = (int)cpu->d[5];
    uint32_t src_ptr = cpu->a[0];

    uint32_t buf_offset;
    if (dst_buf == GR_BUFFER_FRONTBUFFER)
        buf_offset = gs->front_offset;
    else
        buf_offset = gs->back_offset;

    for (int row = 0; row < h; row++) {
        int sy = dst_y + row;
        if (sy < 0 || sy >= GR_SCREEN_H)
            continue;
        for (int col = 0; col < w; col++) {
            int sx = dst_x + col;
            if (sx < 0 || sx >= GR_SCREEN_W)
                continue;

            uint32_t src_off = src_ptr + (uint32_t)(row * src_stride + col * 2);
            uint8_t *src = guest_ptr(gs, src_off);
            if (!src)
                continue;

            uint16_t px = (uint16_t)((src[0] << 8) | src[1]);
            uint32_t dst_off = buf_offset +
                               ((uint32_t)sy * GR_SCREEN_W + (uint32_t)sx) * 2;
            write_vram_u16(gs->vram, dst_off, px);
        }
    }
    return 0;
}

/************************************************************
 * Initialization
 ************************************************************/

/** Initialize the Glide rasterizer state to defaults. */
void
glide_init(glide_state *gs, uint8_t *vram, uint8_t *ram)
{
    memset(gs, 0, sizeof(*gs));
    gs->vram = vram;
    gs->ram = ram;

    /* Vertex layout: all offsets disabled */
    gs->vl_xy_offset = -1;
    gs->vl_z_offset = -1;
    gs->vl_w_offset = -1;
    gs->vl_a_offset = -1;
    gs->vl_rgb_offset = -1;
    gs->vl_pargb_offset = -1;
    gs->vl_st0_offset = -1;
    gs->vl_q0_offset = -1;

    /* Clip to full screen */
    gs->clip_x0 = 0;
    gs->clip_y0 = 0;
    gs->clip_x1 = GR_SCREEN_W;
    gs->clip_y1 = GR_SCREEN_H;

    /* Color/depth masks on */
    gs->color_mask_rgb = 1;
    gs->color_mask_a = 1;
    gs->depth_mask = 1;

    /* Default blend: replace */
    gs->ab_rgb_src = GR_BLEND_ONE;
    gs->ab_rgb_dst = GR_BLEND_ZERO;
    gs->ab_alpha_src = GR_BLEND_ONE;
    gs->ab_alpha_dst = GR_BLEND_ZERO;

    /* Default combine: pass through iterated color */
    gs->cc_function = GR_COMBINE_FUNCTION_LOCAL;
    gs->cc_local = GR_COMBINE_LOCAL_ITERATED;
    gs->ac_function = GR_COMBINE_FUNCTION_LOCAL;
    gs->ac_local = GR_COMBINE_LOCAL_ITERATED;

    /* Default depth/alpha test */
    gs->depth_func = GR_CMP_LESS;
    gs->alpha_test_func = GR_CMP_ALWAYS;
}

/************************************************************
 * Hypercall dispatch
 ************************************************************/

/** Dispatch a Glide hypercall. Returns 0 if handled, 1 if unknown. */
int
glide_dispatch(glide_state *gs, cf_cpu *cpu, uint16_t opword)
{
    int func = opword & 0xFFF;

    switch (func) {

    /* Lifecycle */
    case GR_HC_GLIDE_INIT:      return hc_glide_init(gs, cpu);
    case GR_HC_GLIDE_SHUTDOWN:  return hc_glide_shutdown(gs, cpu);
    case GR_HC_SST_SELECT:      return 0; // no-op, single board
    case GR_HC_SST_WIN_OPEN:    return hc_sst_win_open(gs, cpu);
    case GR_HC_SST_WIN_CLOSE:   return hc_sst_win_close(gs, cpu);

    /* Buffers */
    case GR_HC_BUFFER_CLEAR:    return hc_buffer_clear(gs, cpu);
    case GR_HC_BUFFER_SWAP:     return hc_buffer_swap(gs, cpu);
    case GR_HC_RENDER_BUFFER:   return hc_render_buffer(gs, cpu);
    case GR_HC_CLIP_WINDOW:     return hc_clip_window(gs, cpu);
    case GR_HC_SST_ORIGIN:      return hc_sst_origin(gs, cpu);

    /* Drawing */
    case GR_HC_DRAW_TRIANGLE:   return hc_draw_triangle(gs, cpu);
    case GR_HC_DRAW_LINE:       return hc_draw_line(gs, cpu);
    case GR_HC_DRAW_POINT:      return hc_draw_point(gs, cpu);
    case GR_HC_DRAW_VERTEX_ARRAY:
        return hc_draw_vertex_array(gs, cpu);
    case GR_HC_DRAW_VERTEX_ARRAY_CONTIGUOUS:
        return hc_draw_vertex_array_contiguous(gs, cpu);

    /* Vertex format */
    case GR_HC_VERTEX_LAYOUT:       return hc_vertex_layout(gs, cpu);
    case GR_HC_COORDINATE_SPACE:    return hc_coordinate_space(gs, cpu);

    /* Color combine */
    case GR_HC_COLOR_COMBINE:   return hc_color_combine(gs, cpu);
    case GR_HC_CONSTANT_COLOR_VALUE:
        gs->constant_color = cpu->d[0];
        return 0;
    case GR_HC_COLOR_MASK:
        gs->color_mask_rgb = (int)cpu->d[0];
        gs->color_mask_a = (int)cpu->d[1];
        return 0;
    case GR_HC_DITHER_MODE:
        gs->dither_mode = (int)cpu->d[0];
        return 0;

    /* Alpha */
    case GR_HC_ALPHA_COMBINE:       return hc_alpha_combine(gs, cpu);
    case GR_HC_ALPHA_BLEND_FUNCTION: return hc_alpha_blend_function(gs, cpu);
    case GR_HC_ALPHA_TEST_FUNCTION:
        gs->alpha_test_func = (int)cpu->d[0];
        return 0;
    case GR_HC_ALPHA_TEST_REFERENCE_VALUE:
        gs->alpha_test_ref = (uint8_t)cpu->d[0];
        return 0;

    /* Depth */
    case GR_HC_DEPTH_BUFFER_MODE:
        gs->depth_mode = (int)cpu->d[0];
        return 0;
    case GR_HC_DEPTH_BUFFER_FUNCTION:
        gs->depth_func = (int)cpu->d[0];
        return 0;
    case GR_HC_DEPTH_MASK:
        gs->depth_mask = (int)cpu->d[0];
        return 0;
    case GR_HC_DEPTH_BIAS_LEVEL:
        gs->depth_bias = (int32_t)cpu->d[0];
        return 0;

    /* Fog */
    case GR_HC_FOG_MODE:
        gs->fog_mode = (int)cpu->d[0];
        return 0;
    case GR_HC_FOG_COLOR_VALUE:
        gs->fog_color = cpu->d[0];
        return 0;
    case GR_HC_FOG_TABLE: {
        uint32_t tbl_ptr = cpu->a[0];
        uint8_t *src = guest_ptr(gs, tbl_ptr);
        if (src)
            memcpy(gs->fog_table, src, 64);
        return 0;
    }

    /* Texture */
    case GR_HC_TEX_SOURCE:      return hc_tex_source(gs, cpu);
    case GR_HC_TEX_COMBINE:     return hc_tex_combine(gs, cpu);
    case GR_HC_TEX_CLAMP_MODE:  return hc_tex_clamp_mode(gs, cpu);
    case GR_HC_TEX_FILTER_MODE: return hc_tex_filter_mode(gs, cpu);
    case GR_HC_TEX_MIPMAP_MODE: return hc_tex_mipmap_mode(gs, cpu);
    case GR_HC_TEX_DOWNLOAD_MIPMAP:
        return hc_tex_download_mipmap(gs, cpu);
    case GR_HC_TEX_DOWNLOAD_MIPMAP_LEVEL:
        return hc_tex_download_mipmap_level(gs, cpu);
    case GR_HC_TEX_CALC_MEM_REQUIRED:
    case GR_HC_TEX_TEXTURE_MEM_REQUIRED: {
        int fmt = (int)cpu->d[5];
        int large = (int)cpu->d[2];
        int small = (int)cpu->d[3];
        int asp = (int32_t)cpu->d[4];
        cpu->d[0] = tex_mipmap_size(fmt, large, small, asp);
        return 0;
    }
    case GR_HC_TEX_MIN_ADDRESS:
        cpu->d[0] = 0;
        return 0;
    case GR_HC_TEX_MAX_ADDRESS:
        cpu->d[0] = GR_TEX_END - GR_TEX_START;
        return 0;

    /* Culling */
    case GR_HC_CULL_MODE:
        gs->cull_mode = (int)cpu->d[0];
        return 0;

    /* Chroma-key */
    case GR_HC_CHROMAKEY_MODE:
        gs->chromakey_mode = (int)cpu->d[0];
        return 0;
    case GR_HC_CHROMAKEY_VALUE:
        gs->chromakey_value = (uint16_t)cpu->d[0];
        return 0;

    /* LFB */
    case GR_HC_LFB_LOCK:   return 0; // no-op, always accessible
    case GR_HC_LFB_UNLOCK: return 0;
    case GR_HC_LFB_WRITE_REGION: return hc_lfb_write_region(gs, cpu);

    /* Query */
    case GR_HC_GET:         return hc_get(gs, cpu);
    case GR_HC_GET_STRING: {
        /* Write string to a scratch area at top of RAM and return address.
         * Real Glide returns a pointer to a static buffer. */
        static const char *str_hardware  = "Vertex Triton GPU (SST-2)";
        static const char *str_renderer  = "Glide 3.10 for Triton";
        static const char *str_vendor    = "Vertex Technologies, Inc.";
        static const char *str_version   = "3.10";
        static const char *str_extension = "";
        const char *s = NULL;
        switch ((int)cpu->d[0]) {
        case GR_HARDWARE:  s = str_hardware;  break;
        case GR_RENDERER:  s = str_renderer;  break;
        case GR_VENDOR:    s = str_vendor;    break;
        case GR_VERSION:   s = str_version;   break;
        case GR_EXTENSION: s = str_extension; break;
        }
        if (s) {
            /* Write to scratch area at 0x007FF000 */
            uint32_t addr = 0x007FF000;
            size_t len = strlen(s);
            if (len > 255) len = 255;
            memcpy(gs->ram + addr, s, len);
            gs->ram[addr + len] = '\0';
            cpu->d[0] = addr;
        } else {
            cpu->d[0] = 0;
        }
        return 0;
    }

    /* Enable/disable — no-ops for now */
    case GR_HC_ENABLE:      return 0;
    case GR_HC_DISABLE:     return 0;

    /* Sync */
    case GR_HC_FINISH:      return 0;
    case GR_HC_FLUSH:       return 0;

    default:
        break;
    }

    /* Tier 2 stubs: 0x100-0x1FF are silent no-ops */
    if (func >= 0x100 && func <= 0x1FF)
        return 0;

    /* Unknown function */
    return 1;
}
