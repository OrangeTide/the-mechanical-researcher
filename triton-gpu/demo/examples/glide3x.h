/*
 * glide3x.h -- Glide 3.10 API for the Vertex Triton SDK
 *
 * Copyright (c) 2001 Vertex Technologies, Inc.
 * Portions Copyright (c) 1997-1999 3Dfx Interactive, Inc.
 * All Rights Reserved.
 *
 * Triton Glide 3.10 -- SST-2 compatible graphics API.
 * Types, enums, and inline LINE_A hypercall wrappers.
 *
 * Each Glide function emits a LINE_A opcode (0xA000 | function_id).
 * The host GPU intercepts the opcode and reads arguments from the
 * CPU registers: integer args in D0-D7, pointers in A0-A6,
 * floats in FP0-FP7.
 */

#ifndef GLIDE3X_H
#define GLIDE3X_H

/* ---- Types ------------------------------------------------------------- */

typedef unsigned int    FxU32;
typedef int             FxI32;
typedef unsigned short  FxU16;
typedef unsigned char   FxU8;
typedef int             FxBool;
typedef float           FxFloat;

typedef FxU32 GrColor_t;
typedef FxU8  GrAlpha_t;
typedef FxU8  GrFog_t;

/* ---- Vertex structure -------------------------------------------------- */

/*
 * Standard Triton vertex layout.  The offsets must be registered with
 * grVertexLayout() before drawing.
 */
typedef struct {
    float x, y;         /*  0: screen position */
    float ooz;          /*  8: 1/z (depth, 0-65535 range) */
    float oow;          /* 12: 1/w (perspective correction) */
    float r, g, b, a;   /* 16: vertex color (0.0-255.0) */
    float sow, tow;    /* 32: s/w, t/w texture coordinates */
} GrVertex;

/* Standard vertex field offsets (bytes) */
#define GR_VERTEX_X_OFFSET      0
#define GR_VERTEX_OOZ_OFFSET    8
#define GR_VERTEX_OOW_OFFSET    12
#define GR_VERTEX_R_OFFSET      16
#define GR_VERTEX_A_OFFSET      28
#define GR_VERTEX_SOW_OFFSET    32

/* ---- Coordinate spaces ------------------------------------------------- */

#define GR_WINDOW_COORDS    0
#define GR_CLIP_COORDS      1

/* ---- Cull modes -------------------------------------------------------- */

#define GR_CULL_DISABLE     0
#define GR_CULL_NEGATIVE    1
#define GR_CULL_POSITIVE    2

/* ---- Origin ------------------------------------------------------------ */

#define GR_ORIGIN_UPPER_LEFT    0
#define GR_ORIGIN_LOWER_LEFT    1

/* ---- Depth buffer ------------------------------------------------------ */

#define GR_DEPTHBUFFER_DISABLE              0
#define GR_DEPTHBUFFER_ZBUFFER              1
#define GR_DEPTHBUFFER_WBUFFER              2
#define GR_DEPTHBUFFER_ZBUFFER_COMPARE_BIAS 3
#define GR_DEPTHBUFFER_WBUFFER_COMPARE_BIAS 4

/* ---- Comparison functions ---------------------------------------------- */

#define GR_CMP_NEVER    0
#define GR_CMP_LESS     1
#define GR_CMP_EQUAL    2
#define GR_CMP_LEQUAL   3
#define GR_CMP_GREATER  4
#define GR_CMP_NOTEQUAL 5
#define GR_CMP_GEQUAL   6
#define GR_CMP_ALWAYS   7

/* ---- Color/alpha combine functions ------------------------------------- */

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

/* ---- Color/alpha combine factors --------------------------------------- */

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

/* ---- Color/alpha combine sources --------------------------------------- */

#define GR_COMBINE_LOCAL_ITERATED   0
#define GR_COMBINE_LOCAL_CONSTANT   1
#define GR_COMBINE_LOCAL_DEPTH      2

#define GR_COMBINE_OTHER_ITERATED   0
#define GR_COMBINE_OTHER_TEXTURE    1
#define GR_COMBINE_OTHER_CONSTANT   2

/* ---- Alpha blend functions --------------------------------------------- */

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

/* ---- Texture formats --------------------------------------------------- */

#define GR_TEXFMT_RGB_332               0
#define GR_TEXFMT_ALPHA_8               1
#define GR_TEXFMT_INTENSITY_8           2
#define GR_TEXFMT_ALPHA_INTENSITY_44    3
#define GR_TEXFMT_P_8                   4
#define GR_TEXFMT_RGB_565               10
#define GR_TEXFMT_ARGB_1555             11
#define GR_TEXFMT_ARGB_4444             12
#define GR_TEXFMT_ALPHA_INTENSITY_88    13

/* ---- Texture clamp modes ----------------------------------------------- */

#define GR_TEXCLAMP_WRAP    0
#define GR_TEXCLAMP_CLAMP   1

/* ---- Texture filter modes ---------------------------------------------- */

#define GR_TEXTUREFILTER_POINT_SAMPLED  0
#define GR_TEXTUREFILTER_BILINEAR       1

/* ---- Mipmap modes ------------------------------------------------------ */

#define GR_MIPMAP_DISABLE               0
#define GR_MIPMAP_NEAREST               1
#define GR_MIPMAP_NEAREST_DITHERED      2

/* ---- LOD constants ----------------------------------------------------- */

#define GR_LOD_LOG2_1       0
#define GR_LOD_LOG2_2       1
#define GR_LOD_LOG2_4       2
#define GR_LOD_LOG2_8       3
#define GR_LOD_LOG2_16      4
#define GR_LOD_LOG2_32      5
#define GR_LOD_LOG2_64      6
#define GR_LOD_LOG2_128     7
#define GR_LOD_LOG2_256     8

/* ---- Aspect ratio constants -------------------------------------------- */

#define GR_ASPECT_LOG2_8x1  3
#define GR_ASPECT_LOG2_4x1  2
#define GR_ASPECT_LOG2_2x1  1
#define GR_ASPECT_LOG2_1x1  0
#define GR_ASPECT_LOG2_1x2  (-1)
#define GR_ASPECT_LOG2_1x4  (-2)
#define GR_ASPECT_LOG2_1x8  (-3)

/* ---- Fog modes --------------------------------------------------------- */

#define GR_FOG_DISABLE              0
#define GR_FOG_WITH_TABLE_ON_Q      1

/* ---- Render buffer ----------------------------------------------------- */

#define GR_BUFFER_FRONTBUFFER   0
#define GR_BUFFER_BACKBUFFER    1

/* ---- Dither modes ------------------------------------------------------ */

#define GR_DITHER_DISABLE   0
#define GR_DITHER_2x2       1
#define GR_DITHER_4x4       2

/* ---- Vertex layout parameters ------------------------------------------ */

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

/* ---- grGet query parameters -------------------------------------------- */

#define GR_NUM_TMU          0x01
#define GR_MAX_TEXTURE_SIZE 0x02
#define GR_NUM_BOARDS       0x03
#define GR_MEMORY_FB        0x04
#define GR_MEMORY_TMU       0x05
#define GR_MEMORY_UMA       0x06

/* ---- LINE_A hypercall helper ------------------------------------------- */

/*
 * Emit a LINE_A opcode.  The 12-bit function ID is encoded in the
 * low bits of the opword: .short 0xA000 | func_id.
 *
 * Arguments are loaded into registers before the opword.  GCC's
 * register-variable extension ensures the right registers are used.
 */

#define _HC_LINE_A(id) __asm__ volatile(".short " #id : : : "memory")

/* ---- Lifecycle --------------------------------------------------------- */

static inline void grGlideInit(void)
{
    _HC_LINE_A(0xA001);
}

static inline void grGlideShutdown(void)
{
    _HC_LINE_A(0xA002);
}

static inline void grSstSelect(FxU32 which)
{
    register unsigned long _d0 __asm__("d0") = which;
    __asm__ volatile(".short 0xA003" : : "d"(_d0) : "memory");
}

static inline void grSstWinOpen(void)
{
    _HC_LINE_A(0xA004);
}

static inline void grSstWinClose(void)
{
    _HC_LINE_A(0xA005);
}

/* ---- Buffers ----------------------------------------------------------- */

static inline void grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU32 depth)
{
    register unsigned long _d0 __asm__("d0") = color;
    register unsigned long _d1 __asm__("d1") = alpha;
    register unsigned long _d2 __asm__("d2") = depth;
    __asm__ volatile(".short 0xA010"
        : : "d"(_d0), "d"(_d1), "d"(_d2) : "memory");
}

static inline void grBufferSwap(int interval)
{
    register unsigned long _d0 __asm__("d0") = (unsigned long)interval;
    __asm__ volatile(".short 0xA011" : : "d"(_d0) : "memory");
}

static inline void grRenderBuffer(FxU32 buffer)
{
    register unsigned long _d0 __asm__("d0") = buffer;
    __asm__ volatile(".short 0xA012" : : "d"(_d0) : "memory");
}

static inline void grClipWindow(FxU32 x0, FxU32 y0, FxU32 x1, FxU32 y1)
{
    register unsigned long _d0 __asm__("d0") = x0;
    register unsigned long _d1 __asm__("d1") = y0;
    register unsigned long _d2 __asm__("d2") = x1;
    register unsigned long _d3 __asm__("d3") = y1;
    __asm__ volatile(".short 0xA013"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3) : "memory");
}

static inline void grSstOrigin(FxU32 origin)
{
    register unsigned long _d0 __asm__("d0") = origin;
    __asm__ volatile(".short 0xA014" : : "d"(_d0) : "memory");
}

/* ---- Drawing ----------------------------------------------------------- */

static inline void grDrawTriangle(const GrVertex *a, const GrVertex *b,
                                  const GrVertex *c)
{
    register const GrVertex *_a0 __asm__("a0") = a;
    register const GrVertex *_a1 __asm__("a1") = b;
    register const GrVertex *_a2 __asm__("a2") = c;
    __asm__ volatile(".short 0xA020"
        : : "a"(_a0), "a"(_a1), "a"(_a2) : "memory");
}

static inline void grDrawLine(const GrVertex *a, const GrVertex *b)
{
    register const GrVertex *_a0 __asm__("a0") = a;
    register const GrVertex *_a1 __asm__("a1") = b;
    __asm__ volatile(".short 0xA021"
        : : "a"(_a0), "a"(_a1) : "memory");
}

static inline void grDrawPoint(const GrVertex *v)
{
    register const GrVertex *_a0 __asm__("a0") = v;
    __asm__ volatile(".short 0xA022" : : "a"(_a0) : "memory");
}

/* ---- Vertex layout ----------------------------------------------------- */

static inline void grVertexLayout(FxU32 param, FxI32 offset, FxU32 mode)
{
    register unsigned long _d0 __asm__("d0") = param;
    register unsigned long _d1 __asm__("d1") = (unsigned long)offset;
    register unsigned long _d2 __asm__("d2") = mode;
    __asm__ volatile(".short 0xA030"
        : : "d"(_d0), "d"(_d1), "d"(_d2) : "memory");
}

static inline void grCoordinateSpace(FxU32 mode)
{
    register unsigned long _d0 __asm__("d0") = mode;
    __asm__ volatile(".short 0xA031" : : "d"(_d0) : "memory");
}

/* ---- Color combine ----------------------------------------------------- */

static inline void grColorCombine(FxU32 func, FxU32 factor, FxU32 local,
                                  FxU32 other, FxBool invert)
{
    register unsigned long _d0 __asm__("d0") = func;
    register unsigned long _d1 __asm__("d1") = factor;
    register unsigned long _d2 __asm__("d2") = local;
    register unsigned long _d3 __asm__("d3") = other;
    register unsigned long _d4 __asm__("d4") = (unsigned long)invert;
    __asm__ volatile(".short 0xA040"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3), "d"(_d4) : "memory");
}

static inline void grConstantColorValue(GrColor_t color)
{
    register unsigned long _d0 __asm__("d0") = color;
    __asm__ volatile(".short 0xA041" : : "d"(_d0) : "memory");
}

static inline void grColorMask(FxBool rgb, FxBool alpha)
{
    register unsigned long _d0 __asm__("d0") = (unsigned long)rgb;
    register unsigned long _d1 __asm__("d1") = (unsigned long)alpha;
    __asm__ volatile(".short 0xA042" : : "d"(_d0), "d"(_d1) : "memory");
}

static inline void grDitherMode(FxU32 mode)
{
    register unsigned long _d0 __asm__("d0") = mode;
    __asm__ volatile(".short 0xA043" : : "d"(_d0) : "memory");
}

/* ---- Alpha ------------------------------------------------------------- */

static inline void grAlphaCombine(FxU32 func, FxU32 factor, FxU32 local,
                                  FxU32 other, FxBool invert)
{
    register unsigned long _d0 __asm__("d0") = func;
    register unsigned long _d1 __asm__("d1") = factor;
    register unsigned long _d2 __asm__("d2") = local;
    register unsigned long _d3 __asm__("d3") = other;
    register unsigned long _d4 __asm__("d4") = (unsigned long)invert;
    __asm__ volatile(".short 0xA050"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3), "d"(_d4) : "memory");
}

static inline void grAlphaBlendFunction(FxU32 rgb_src, FxU32 rgb_dst,
                                        FxU32 alpha_src, FxU32 alpha_dst)
{
    register unsigned long _d0 __asm__("d0") = rgb_src;
    register unsigned long _d1 __asm__("d1") = rgb_dst;
    register unsigned long _d2 __asm__("d2") = alpha_src;
    register unsigned long _d3 __asm__("d3") = alpha_dst;
    __asm__ volatile(".short 0xA051"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3) : "memory");
}

static inline void grAlphaTestFunction(FxU32 func)
{
    register unsigned long _d0 __asm__("d0") = func;
    __asm__ volatile(".short 0xA052" : : "d"(_d0) : "memory");
}

static inline void grAlphaTestReferenceValue(GrAlpha_t ref)
{
    register unsigned long _d0 __asm__("d0") = ref;
    __asm__ volatile(".short 0xA053" : : "d"(_d0) : "memory");
}

/* ---- Depth ------------------------------------------------------------- */

static inline void grDepthBufferMode(FxU32 mode)
{
    register unsigned long _d0 __asm__("d0") = mode;
    __asm__ volatile(".short 0xA060" : : "d"(_d0) : "memory");
}

static inline void grDepthBufferFunction(FxU32 func)
{
    register unsigned long _d0 __asm__("d0") = func;
    __asm__ volatile(".short 0xA061" : : "d"(_d0) : "memory");
}

static inline void grDepthMask(FxBool mask)
{
    register unsigned long _d0 __asm__("d0") = (unsigned long)mask;
    __asm__ volatile(".short 0xA062" : : "d"(_d0) : "memory");
}

static inline void grDepthBiasLevel(FxI32 level)
{
    register unsigned long _d0 __asm__("d0") = (unsigned long)level;
    __asm__ volatile(".short 0xA063" : : "d"(_d0) : "memory");
}

/* ---- Fog --------------------------------------------------------------- */

static inline void grFogMode(FxU32 mode)
{
    register unsigned long _d0 __asm__("d0") = mode;
    __asm__ volatile(".short 0xA070" : : "d"(_d0) : "memory");
}

static inline void grFogColorValue(GrColor_t color)
{
    register unsigned long _d0 __asm__("d0") = color;
    __asm__ volatile(".short 0xA071" : : "d"(_d0) : "memory");
}

static inline void grFogTable(const GrFog_t table[64])
{
    register const GrFog_t *_a0 __asm__("a0") = table;
    __asm__ volatile(".short 0xA072" : : "a"(_a0) : "memory");
}

/* ---- Texture ----------------------------------------------------------- */

/*
 * grTexSource: bind a texture in VRAM.
 * d[0] = tmu (0), d[1] = startAddress (VRAM-relative),
 * d[2] = largeLod, d[3] = smallLod, d[4] = aspect, d[5] = format
 */
static inline void grTexSource(FxU32 tmu, FxU32 startAddress,
                               FxU32 largeLod, FxU32 smallLod,
                               FxI32 aspect, FxU32 format)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    register unsigned long _d1 __asm__("d1") = startAddress;
    register unsigned long _d2 __asm__("d2") = largeLod;
    register unsigned long _d3 __asm__("d3") = smallLod;
    register unsigned long _d4 __asm__("d4") = (unsigned long)aspect;
    register unsigned long _d5 __asm__("d5") = format;
    __asm__ volatile(".short 0xA080"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3), "d"(_d4), "d"(_d5)
        : "memory");
}

/*
 * grTexDownloadMipMapLevel: upload one mipmap level from guest RAM to VRAM.
 * d[0] = tmu, d[1] = startAddress, d[2] = thisLod, d[3] = largeLod,
 * d[4] = aspect, d[5] = format, a[0] = data pointer
 */
static inline void grTexDownloadMipMapLevel(FxU32 tmu, FxU32 startAddress,
                                            FxU32 thisLod, FxU32 largeLod,
                                            FxI32 aspect, FxU32 format,
                                            const void *data)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    register unsigned long _d1 __asm__("d1") = startAddress;
    register unsigned long _d2 __asm__("d2") = thisLod;
    register unsigned long _d3 __asm__("d3") = largeLod;
    register unsigned long _d4 __asm__("d4") = (unsigned long)aspect;
    register unsigned long _d5 __asm__("d5") = format;
    register const void *_a0 __asm__("a0") = data;
    __asm__ volatile(".short 0xA086"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3), "d"(_d4), "d"(_d5),
            "a"(_a0)
        : "memory");
}

static inline void grTexCombine(FxU32 tmu, FxU32 rgbFunc, FxU32 rgbFactor,
                                FxU32 alphaFunc, FxU32 alphaFactor,
                                FxBool rgbInvert, FxBool alphaInvert)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    register unsigned long _d1 __asm__("d1") = rgbFunc;
    register unsigned long _d2 __asm__("d2") = rgbFactor;
    register unsigned long _d3 __asm__("d3") = alphaFunc;
    register unsigned long _d4 __asm__("d4") = alphaFactor;
    register unsigned long _d5 __asm__("d5") = (unsigned long)rgbInvert;
    register unsigned long _d6 __asm__("d6") = (unsigned long)alphaInvert;
    __asm__ volatile(".short 0xA081"
        : : "d"(_d0), "d"(_d1), "d"(_d2), "d"(_d3), "d"(_d4), "d"(_d5),
            "d"(_d6)
        : "memory");
}

static inline void grTexClampMode(FxU32 tmu, FxU32 sClamp, FxU32 tClamp)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    register unsigned long _d1 __asm__("d1") = sClamp;
    register unsigned long _d2 __asm__("d2") = tClamp;
    __asm__ volatile(".short 0xA082"
        : : "d"(_d0), "d"(_d1), "d"(_d2) : "memory");
}

static inline void grTexFilterMode(FxU32 tmu, FxU32 minFilter, FxU32 magFilter)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    register unsigned long _d1 __asm__("d1") = minFilter;
    register unsigned long _d2 __asm__("d2") = magFilter;
    __asm__ volatile(".short 0xA083"
        : : "d"(_d0), "d"(_d1), "d"(_d2) : "memory");
}

static inline void grTexMipMapMode(FxU32 tmu, FxU32 mode, FxBool lodBlend)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    register unsigned long _d1 __asm__("d1") = mode;
    register unsigned long _d2 __asm__("d2") = (unsigned long)lodBlend;
    __asm__ volatile(".short 0xA084"
        : : "d"(_d0), "d"(_d1), "d"(_d2) : "memory");
}

static inline FxU32 grTexMinAddress(FxU32 tmu)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    __asm__ volatile(".short 0xA089" : "+d"(_d0) : : "memory");
    return _d0;
}

static inline FxU32 grTexMaxAddress(FxU32 tmu)
{
    register unsigned long _d0 __asm__("d0") = tmu;
    __asm__ volatile(".short 0xA08A" : "+d"(_d0) : : "memory");
    return _d0;
}

/* ---- Culling ----------------------------------------------------------- */

static inline void grCullMode(FxU32 mode)
{
    register unsigned long _d0 __asm__("d0") = mode;
    __asm__ volatile(".short 0xA090" : : "d"(_d0) : "memory");
}

/* ---- Query ------------------------------------------------------------- */

static inline FxU32 grGet(FxU32 pname)
{
    register unsigned long _d0 __asm__("d0") = pname;
    __asm__ volatile(".short 0xA0C0" : "+d"(_d0) : : "memory");
    return _d0;
}

/* ---- Sync -------------------------------------------------------------- */

static inline void grFinish(void)
{
    _HC_LINE_A(0xA0E0);
}

static inline void grFlush(void)
{
    _HC_LINE_A(0xA0E1);
}

#endif /* GLIDE3X_H */
