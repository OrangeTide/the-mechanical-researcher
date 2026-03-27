/*
 * glide_raster.h -- Triton GPU: Glide 3.0 software rasterizer
 *
 * Host-side state, VRAM layout constants, and public API.
 * The rasterizer implements a subset of the Glide 3.0 API, exposed to
 * guest programs through LINE_A hypercalls.
 */

#ifndef GLIDE_RASTER_H
#define GLIDE_RASTER_H

#include <stdint.h>
#include "coldfire.h"

/* ---- VRAM layout ------------------------------------------------------- */

#define GR_SCREEN_W     640
#define GR_SCREEN_H     480
#define GR_FB_STRIDE    (GR_SCREEN_W * 2)       /* bytes per scanline */
#define GR_FB_SIZE      (GR_SCREEN_W * GR_SCREEN_H * 2)  /* 614400 */

#define GR_FRONT_OFFSET 0x000000
#define GR_BACK_OFFSET  0x096000                 /* 614400 */
#define GR_ZBUF_OFFSET  0x12C000                 /* 1228800 */
#define GR_TEX_START    0x1C2000                 /* 1843200 */
#define GR_TEX_END      0x800000                 /* 8 MB */

/* ---- Glide enums (must match guest-side glide3x.h) --------------------- */

/* Coordinate space */
#define GR_WINDOW_COORDS    0
#define GR_CLIP_COORDS      1

/* Cull modes */
#define GR_CULL_DISABLE     0
#define GR_CULL_NEGATIVE    1
#define GR_CULL_POSITIVE    2

/* Origin */
#define GR_ORIGIN_UPPER_LEFT    0
#define GR_ORIGIN_LOWER_LEFT    1

/* Depth buffer modes */
#define GR_DEPTHBUFFER_DISABLE              0
#define GR_DEPTHBUFFER_ZBUFFER              1
#define GR_DEPTHBUFFER_WBUFFER              2
#define GR_DEPTHBUFFER_ZBUFFER_COMPARE_BIAS 3
#define GR_DEPTHBUFFER_WBUFFER_COMPARE_BIAS 4

/* Comparison functions (depth, alpha test) */
#define GR_CMP_NEVER    0
#define GR_CMP_LESS     1
#define GR_CMP_EQUAL    2
#define GR_CMP_LEQUAL   3
#define GR_CMP_GREATER  4
#define GR_CMP_NOTEQUAL 5
#define GR_CMP_GEQUAL   6
#define GR_CMP_ALWAYS   7

/* Color/alpha combine functions */
#define GR_COMBINE_FUNCTION_ZERO                                    0
#define GR_COMBINE_FUNCTION_LOCAL                                   1
#define GR_COMBINE_FUNCTION_LOCAL_ALPHA                             2
#define GR_COMBINE_FUNCTION_SCALE_OTHER                             3
#define GR_COMBINE_FUNCTION_BLEND_OTHER                             4
#define GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL                   5
#define GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA             6
#define GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL                 7
#define GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL       8
#define GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA 9
#define GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL             10
#define GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA       11

/* Color/alpha combine factors */
#define GR_COMBINE_FACTOR_ZERO                  0
#define GR_COMBINE_FACTOR_LOCAL                  1
#define GR_COMBINE_FACTOR_OTHER_ALPHA            2
#define GR_COMBINE_FACTOR_LOCAL_ALPHA            3
#define GR_COMBINE_FACTOR_TEXTURE_ALPHA          4
#define GR_COMBINE_FACTOR_ONE                    8
#define GR_COMBINE_FACTOR_ONE_MINUS_LOCAL        9
#define GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA  10
#define GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA  11
#define GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA 12

/* Color/alpha combine local source */
#define GR_COMBINE_LOCAL_ITERATED   0
#define GR_COMBINE_LOCAL_CONSTANT   1
#define GR_COMBINE_LOCAL_DEPTH      2

/* Color/alpha combine other source */
#define GR_COMBINE_OTHER_ITERATED   0
#define GR_COMBINE_OTHER_TEXTURE    1
#define GR_COMBINE_OTHER_CONSTANT   2

/* Alpha blend functions */
#define GR_BLEND_ZERO                   0
#define GR_BLEND_SRC_ALPHA              1
#define GR_BLEND_SRC_COLOR              2
#define GR_BLEND_DST_ALPHA              3
#define GR_BLEND_DST_COLOR              4
#define GR_BLEND_ONE                    5
#define GR_BLEND_ONE_MINUS_SRC_ALPHA    6
#define GR_BLEND_ONE_MINUS_SRC_COLOR    7
#define GR_BLEND_ONE_MINUS_DST_ALPHA    8
#define GR_BLEND_ONE_MINUS_DST_COLOR    9

/* Texture formats */
#define GR_TEXFMT_RGB_332               0
#define GR_TEXFMT_ALPHA_8               1
#define GR_TEXFMT_INTENSITY_8           2
#define GR_TEXFMT_ALPHA_INTENSITY_44    3
#define GR_TEXFMT_P_8                   4
#define GR_TEXFMT_RGB_565               10
#define GR_TEXFMT_ARGB_1555             11
#define GR_TEXFMT_ARGB_4444             12
#define GR_TEXFMT_ALPHA_INTENSITY_88    13

/* Texture clamp modes */
#define GR_TEXCLAMP_WRAP    0
#define GR_TEXCLAMP_CLAMP   1

/* Texture filter modes */
#define GR_TEXTUREFILTER_POINT_SAMPLED  0
#define GR_TEXTUREFILTER_BILINEAR       1

/* Mipmap modes */
#define GR_MIPMAP_DISABLE               0
#define GR_MIPMAP_NEAREST               1
#define GR_MIPMAP_NEAREST_DITHERED      2

/* Fog modes */
#define GR_FOG_DISABLE              0
#define GR_FOG_WITH_TABLE_ON_Q      1

/* Render buffer */
#define GR_BUFFER_FRONTBUFFER   0
#define GR_BUFFER_BACKBUFFER    1

/* Dither modes */
#define GR_DITHER_DISABLE   0
#define GR_DITHER_2x2       1
#define GR_DITHER_4x4       2

/* LOD constants */
#define GR_LOD_LOG2_1       0
#define GR_LOD_LOG2_2       1
#define GR_LOD_LOG2_4       2
#define GR_LOD_LOG2_8       3
#define GR_LOD_LOG2_16      4
#define GR_LOD_LOG2_32      5
#define GR_LOD_LOG2_64      6
#define GR_LOD_LOG2_128     7
#define GR_LOD_LOG2_256     8

/* Aspect ratio constants */
#define GR_ASPECT_LOG2_8x1  3
#define GR_ASPECT_LOG2_4x1  2
#define GR_ASPECT_LOG2_2x1  1
#define GR_ASPECT_LOG2_1x1  0
#define GR_ASPECT_LOG2_1x2  (-1)
#define GR_ASPECT_LOG2_1x4  (-2)
#define GR_ASPECT_LOG2_1x8  (-3)

/* grGet query parameters */
#define GR_NUM_TMU          0x01
#define GR_MAX_TEXTURE_SIZE 0x02
#define GR_NUM_BOARDS       0x03
#define GR_MEMORY_FB        0x04
#define GR_MEMORY_TMU       0x05
#define GR_MEMORY_UMA       0x06

/* grGetString parameters */
#define GR_HARDWARE     1
#define GR_RENDERER     2
#define GR_VENDOR       3
#define GR_VERSION      4
#define GR_EXTENSION    5

/* Vertex layout parameters */
#define GR_PARAM_XY     0x01
#define GR_PARAM_Z      0x02
#define GR_PARAM_W      0x03
#define GR_PARAM_Q      0x04
#define GR_PARAM_A      0x10
#define GR_PARAM_RGB    0x11
#define GR_PARAM_PARGB  0x12
#define GR_PARAM_ST0    0x20
#define GR_PARAM_Q0     0x21
#define GR_PARAM_FOG_EXT 0x30

#define GR_PARAM_ENABLE  1
#define GR_PARAM_DISABLE 0

/* ---- Hypercall function IDs -------------------------------------------- */

/* Lifecycle */
#define GR_HC_GLIDE_INIT        0x001
#define GR_HC_GLIDE_SHUTDOWN    0x002
#define GR_HC_SST_SELECT        0x003
#define GR_HC_SST_WIN_OPEN      0x004
#define GR_HC_SST_WIN_CLOSE     0x005

/* Buffers */
#define GR_HC_BUFFER_CLEAR      0x010
#define GR_HC_BUFFER_SWAP       0x011
#define GR_HC_RENDER_BUFFER     0x012
#define GR_HC_CLIP_WINDOW       0x013
#define GR_HC_SST_ORIGIN        0x014

/* Drawing */
#define GR_HC_DRAW_TRIANGLE     0x020
#define GR_HC_DRAW_LINE         0x021
#define GR_HC_DRAW_POINT        0x022
#define GR_HC_DRAW_VERTEX_ARRAY 0x023
#define GR_HC_DRAW_VERTEX_ARRAY_CONTIGUOUS 0x024

/* Vertex format */
#define GR_HC_VERTEX_LAYOUT     0x030
#define GR_HC_COORDINATE_SPACE  0x031

/* Color combine */
#define GR_HC_COLOR_COMBINE         0x040
#define GR_HC_CONSTANT_COLOR_VALUE  0x041
#define GR_HC_COLOR_MASK            0x042
#define GR_HC_DITHER_MODE           0x043

/* Alpha */
#define GR_HC_ALPHA_COMBINE             0x050
#define GR_HC_ALPHA_BLEND_FUNCTION      0x051
#define GR_HC_ALPHA_TEST_FUNCTION       0x052
#define GR_HC_ALPHA_TEST_REFERENCE_VALUE 0x053

/* Depth */
#define GR_HC_DEPTH_BUFFER_MODE     0x060
#define GR_HC_DEPTH_BUFFER_FUNCTION 0x061
#define GR_HC_DEPTH_MASK            0x062
#define GR_HC_DEPTH_BIAS_LEVEL      0x063

/* Fog */
#define GR_HC_FOG_MODE          0x070
#define GR_HC_FOG_COLOR_VALUE   0x071
#define GR_HC_FOG_TABLE         0x072

/* Texture */
#define GR_HC_TEX_SOURCE                0x080
#define GR_HC_TEX_COMBINE               0x081
#define GR_HC_TEX_CLAMP_MODE            0x082
#define GR_HC_TEX_FILTER_MODE           0x083
#define GR_HC_TEX_MIPMAP_MODE           0x084
#define GR_HC_TEX_DOWNLOAD_MIPMAP       0x085
#define GR_HC_TEX_DOWNLOAD_MIPMAP_LEVEL 0x086
#define GR_HC_TEX_CALC_MEM_REQUIRED     0x087
#define GR_HC_TEX_TEXTURE_MEM_REQUIRED  0x088
#define GR_HC_TEX_MIN_ADDRESS           0x089
#define GR_HC_TEX_MAX_ADDRESS           0x08A

/* Culling */
#define GR_HC_CULL_MODE         0x090

/* Chroma-key */
#define GR_HC_CHROMAKEY_MODE    0x0A0
#define GR_HC_CHROMAKEY_VALUE   0x0A1

/* LFB */
#define GR_HC_LFB_LOCK         0x0B0
#define GR_HC_LFB_UNLOCK       0x0B1
#define GR_HC_LFB_WRITE_REGION 0x0B2

/* Query */
#define GR_HC_GET               0x0C0
#define GR_HC_GET_STRING        0x0C1

/* Enable/disable */
#define GR_HC_ENABLE            0x0D0
#define GR_HC_DISABLE           0x0D1

/* Sync */
#define GR_HC_FINISH            0x0E0
#define GR_HC_FLUSH             0x0E1

/* ---- Rasterizer state -------------------------------------------------- */

typedef struct glide_state {
    uint8_t *vram;              /* pointer to system VRAM */
    uint8_t *ram;               /* pointer to system RAM */
    int initialized;
    int win_open;

    /* Framebuffer */
    uint32_t front_offset;
    uint32_t back_offset;
    uint32_t zbuf_offset;
    uint32_t draw_offset;       /* current render target */

    /* Vertex layout (byte offsets, -1 = not present) */
    int vl_xy_offset;
    int vl_z_offset;
    int vl_w_offset;
    int vl_a_offset;
    int vl_rgb_offset;
    int vl_pargb_offset;
    int vl_st0_offset;
    int vl_q0_offset;
    int coord_space;

    /* Color combine */
    int cc_function, cc_factor, cc_local, cc_other;
    int cc_invert;
    uint32_t constant_color;    /* packed 0xAARRGGBB */

    /* Alpha combine */
    int ac_function, ac_factor, ac_local, ac_other;
    int ac_invert;

    /* Alpha blend */
    int ab_rgb_src, ab_rgb_dst;
    int ab_alpha_src, ab_alpha_dst;

    /* Alpha test */
    int alpha_test_func;
    uint8_t alpha_test_ref;

    /* Depth */
    int depth_mode;
    int depth_func;
    int depth_mask;
    int32_t depth_bias;

    /* Fog */
    int fog_mode;
    uint32_t fog_color;
    uint8_t fog_table[64];

    /* Texture (TMU 0) */
    uint32_t tex_base;          /* VRAM offset of current texture */
    int tex_width, tex_height;
    int tex_format;
    int tex_clamp_s, tex_clamp_t;
    int tex_filter_min, tex_filter_mag;
    int tex_mipmap_mode;
    /* Texture combine */
    int tc_rgb_func, tc_rgb_factor;
    int tc_alpha_func, tc_alpha_factor;
    int tc_rgb_invert, tc_alpha_invert;
    /* Texture LOD info (from grTexSource) */
    int tex_small_lod, tex_large_lod;
    int tex_aspect;

    /* Culling */
    int cull_mode;

    /* Scissor / clip window */
    int clip_x0, clip_y0, clip_x1, clip_y1;

    /* Color mask */
    int color_mask_rgb, color_mask_a;

    /* Dither */
    int dither_mode;

    /* Chroma-key */
    int chromakey_mode;
    uint16_t chromakey_value;

    /* Origin */
    int origin;

    /* Vsync: set by grBufferSwap when interval > 0 */
    int vblank_wait;
} glide_state;

/* ---- Public API -------------------------------------------------------- */

/* Initialize rasterizer state. Call before cf_set_hypercall. */
void glide_init(glide_state *gs, uint8_t *vram, uint8_t *ram);

/* Hypercall dispatch. Returns 0 if handled, nonzero to fall through. */
int glide_dispatch(glide_state *gs, cf_cpu *cpu, uint16_t opword);

#endif /* GLIDE_RASTER_H */
