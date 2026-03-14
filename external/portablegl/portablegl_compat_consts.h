/*
 * portablegl_compat_consts.h  –  OpenGL 1.x/2.x compatibility constants
 *
 * PortableGL implements OpenGL 3.x core profile and deliberately omits
 * the deprecated fixed-function constants from GL 1.x/2.x.  Obol's source
 * references many of these constants as arguments to the GL dispatch layer
 * (SoGLContext_glMatrixMode, SoGLContext_glLightfv, etc.).  Since those
 * calls are routed through the ObolPGLCompatState interceptors at run time,
 * the values only need to be known at compile time.
 *
 * This header must be included AFTER portablegl.h.
 *
 * PROPOSED UPSTREAM CONTRIBUTION TO PORTABLEGL
 * ─────────────────────────────────────────────
 * Once the fixed-function state additions described in portablegl_compat_state.h
 * are merged upstream, these constants should move into portablegl.h itself
 * (perhaps guarded by a PGL_COMPAT_PROFILE macro).  This header would then
 * become obsolete and be removed from the Obol build.
 */

#ifndef PORTABLEGL_COMPAT_CONSTS_H
#define PORTABLEGL_COMPAT_CONSTS_H

/* ─── Matrix mode ─────────────────────────────────────────────────────────── */
#ifndef GL_MODELVIEW
#  define GL_MODELVIEW           0x1700
#endif
#ifndef GL_PROJECTION
#  define GL_PROJECTION          0x1701
#endif
#ifndef GL_TEXTURE
#  define GL_TEXTURE             0x1702
#endif
#ifndef GL_COLOR
#  define GL_COLOR               0x1800
#endif
#ifndef GL_MODELVIEW_MATRIX
#  define GL_MODELVIEW_MATRIX    0x0BA6
#endif
#ifndef GL_PROJECTION_MATRIX
#  define GL_PROJECTION_MATRIX   0x0BA7
#endif
#ifndef GL_TEXTURE_MATRIX
#  define GL_TEXTURE_MATRIX      0x0BA8
#endif

/* ─── Light sources ───────────────────────────────────────────────────────── */
#ifndef GL_LIGHTING
#  define GL_LIGHTING            0x0B50
#endif
#ifndef GL_LIGHT0
#  define GL_LIGHT0              0x4000
#endif
#ifndef GL_LIGHT1
#  define GL_LIGHT1              0x4001
#endif
#ifndef GL_LIGHT2
#  define GL_LIGHT2              0x4002
#endif
#ifndef GL_LIGHT3
#  define GL_LIGHT3              0x4003
#endif
#ifndef GL_LIGHT4
#  define GL_LIGHT4              0x4004
#endif
#ifndef GL_LIGHT5
#  define GL_LIGHT5              0x4005
#endif
#ifndef GL_LIGHT6
#  define GL_LIGHT6              0x4006
#endif
#ifndef GL_LIGHT7
#  define GL_LIGHT7              0x4007
#endif
#ifndef GL_MAX_LIGHTS
#  define GL_MAX_LIGHTS          0x0D31
#endif
#ifndef GL_POSITION
#  define GL_POSITION            0x1203
#endif
#ifndef GL_SPOT_DIRECTION
#  define GL_SPOT_DIRECTION      0x1204
#endif
#ifndef GL_SPOT_EXPONENT
#  define GL_SPOT_EXPONENT       0x1205
#endif
#ifndef GL_SPOT_CUTOFF
#  define GL_SPOT_CUTOFF         0x1206
#endif
#ifndef GL_CONSTANT_ATTENUATION
#  define GL_CONSTANT_ATTENUATION  0x1207
#endif
#ifndef GL_LINEAR_ATTENUATION
#  define GL_LINEAR_ATTENUATION    0x1208
#endif
#ifndef GL_QUADRATIC_ATTENUATION
#  define GL_QUADRATIC_ATTENUATION 0x1209
#endif
#ifndef GL_AMBIENT
#  define GL_AMBIENT             0x1200
#endif
#ifndef GL_DIFFUSE
#  define GL_DIFFUSE             0x1201
#endif
#ifndef GL_SPECULAR
#  define GL_SPECULAR            0x1202
#endif
#ifndef GL_LIGHT_MODEL_AMBIENT
#  define GL_LIGHT_MODEL_AMBIENT     0x0B53
#endif
#ifndef GL_LIGHT_MODEL_TWO_SIDE
#  define GL_LIGHT_MODEL_TWO_SIDE    0x0B52
#endif
#ifndef GL_LIGHT_MODEL_LOCAL_VIEWER
#  define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#endif

/* ─── Material ────────────────────────────────────────────────────────────── */
#ifndef GL_EMISSION
#  define GL_EMISSION            0x1600
#endif
#ifndef GL_SHININESS
#  define GL_SHININESS           0x1601
#endif
#ifndef GL_AMBIENT_AND_DIFFUSE
#  define GL_AMBIENT_AND_DIFFUSE 0x1602
#endif
#ifndef GL_COLOR_MATERIAL
#  define GL_COLOR_MATERIAL      0x0B57
#endif
#ifndef GL_COLOR_MATERIAL_FACE
#  define GL_COLOR_MATERIAL_FACE 0x0B55
#endif
#ifndef GL_COLOR_MATERIAL_PARAMETER
#  define GL_COLOR_MATERIAL_PARAMETER 0x0B56
#endif
#ifndef GL_FRONT
#  define GL_FRONT               0x0404
#endif
#ifndef GL_BACK
#  define GL_BACK                0x0405
#endif
#ifndef GL_FRONT_AND_BACK
#  define GL_FRONT_AND_BACK      0x0408
#endif

/* ─── Texture environment ─────────────────────────────────────────────────── */
#ifndef GL_TEXTURE_ENV
#  define GL_TEXTURE_ENV         0x2300
#endif
#ifndef GL_TEXTURE_ENV_MODE
#  define GL_TEXTURE_ENV_MODE    0x2200
#endif
#ifndef GL_TEXTURE_ENV_COLOR
#  define GL_TEXTURE_ENV_COLOR   0x2201
#endif
#ifndef GL_MODULATE
#  define GL_MODULATE            0x2100
#endif
#ifndef GL_DECAL
#  define GL_DECAL               0x2101
#endif
#ifndef GL_REPLACE
#  define GL_REPLACE             0x1E01
#endif
#ifndef GL_BLEND
#  define GL_BLEND               0x0BE2
#endif

/* ─── Texture coordinate generation ─────────────────────────────────────── */
#ifndef GL_TEXTURE_GEN_S
#  define GL_TEXTURE_GEN_S       0x0C60
#endif
#ifndef GL_TEXTURE_GEN_T
#  define GL_TEXTURE_GEN_T       0x0C61
#endif
#ifndef GL_TEXTURE_GEN_R
#  define GL_TEXTURE_GEN_R       0x0C62
#endif
#ifndef GL_TEXTURE_GEN_Q
#  define GL_TEXTURE_GEN_Q       0x0C63
#endif
#ifndef GL_TEXTURE_GEN_MODE
#  define GL_TEXTURE_GEN_MODE    0x2500
#endif
#ifndef GL_OBJECT_LINEAR
#  define GL_OBJECT_LINEAR       0x2401
#endif
#ifndef GL_OBJECT_PLANE
#  define GL_OBJECT_PLANE        0x2501
#endif
#ifndef GL_EYE_LINEAR
#  define GL_EYE_LINEAR          0x2400
#endif
#ifndef GL_SPHERE_MAP
#  define GL_SPHERE_MAP          0x2402
#endif
#ifndef GL_NORMAL_MAP
#  define GL_NORMAL_MAP          0x8511
#endif
#ifndef GL_REFLECTION_MAP
#  define GL_REFLECTION_MAP      0x8512
#endif

/* ─── Fog ─────────────────────────────────────────────────────────────────── */
#ifndef GL_FOG
#  define GL_FOG                 0x0B60
#endif
#ifndef GL_FOG_MODE
#  define GL_FOG_MODE            0x0B65
#endif
#ifndef GL_FOG_DENSITY
#  define GL_FOG_DENSITY         0x0B62
#endif
#ifndef GL_FOG_START
#  define GL_FOG_START           0x0B63
#endif
#ifndef GL_FOG_END
#  define GL_FOG_END             0x0B64
#endif
#ifndef GL_FOG_COLOR
#  define GL_FOG_COLOR           0x0B66
#endif
#ifndef GL_FOG_INDEX
#  define GL_FOG_INDEX           0x0B61
#endif
#ifndef GL_EXP
#  define GL_EXP                 0x0800
#endif
#ifndef GL_EXP2
#  define GL_EXP2                0x0801
#endif
#ifndef GL_LINEAR
#  define GL_LINEAR              0x2601
#endif

/* ─── Polygon/Line/Point modes ───────────────────────────────────────────── */
#ifndef GL_POINT
#  define GL_POINT               0x1B00
#endif
#ifndef GL_LINE
#  define GL_LINE                0x1B01
#endif
#ifndef GL_FILL
#  define GL_FILL                0x1B02
#endif
#ifndef GL_LINE_SMOOTH
#  define GL_LINE_SMOOTH         0x0B20
#endif
#ifndef GL_POINT_SMOOTH
#  define GL_POINT_SMOOTH        0x0B10
#endif
#ifndef GL_POLYGON_SMOOTH
#  define GL_POLYGON_SMOOTH      0x0B41
#endif
#ifndef GL_MULTISAMPLE
#  define GL_MULTISAMPLE         0x809D
#endif

/* ─── Vertex arrays (legacy) ─────────────────────────────────────────────── */
#ifndef GL_VERTEX_ARRAY
#  define GL_VERTEX_ARRAY        0x8074
#endif
#ifndef GL_NORMAL_ARRAY
#  define GL_NORMAL_ARRAY        0x8075
#endif
#ifndef GL_COLOR_ARRAY
#  define GL_COLOR_ARRAY         0x8076
#endif
#ifndef GL_INDEX_ARRAY
#  define GL_INDEX_ARRAY         0x8077
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#  define GL_TEXTURE_COORD_ARRAY 0x8078
#endif
#ifndef GL_EDGE_FLAG_ARRAY
#  define GL_EDGE_FLAG_ARRAY     0x8079
#endif

/* ─── glPushAttrib / glPopAttrib bits ────────────────────────────────────── */
#ifndef GL_CURRENT_BIT
#  define GL_CURRENT_BIT         0x00000001
#endif
#ifndef GL_POINT_BIT
#  define GL_POINT_BIT           0x00000002
#endif
#ifndef GL_LINE_BIT
#  define GL_LINE_BIT            0x00000004
#endif
#ifndef GL_POLYGON_BIT
#  define GL_POLYGON_BIT         0x00000008
#endif
#ifndef GL_POLYGON_STIPPLE_BIT
#  define GL_POLYGON_STIPPLE_BIT 0x00000010
#endif
#ifndef GL_PIXEL_MODE_BIT
#  define GL_PIXEL_MODE_BIT      0x00000020
#endif
#ifndef GL_LIGHTING_BIT
#  define GL_LIGHTING_BIT        0x00000040
#endif
#ifndef GL_FOG_BIT
#  define GL_FOG_BIT             0x00000080
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#  define GL_DEPTH_BUFFER_BIT    0x00000100
#endif
#ifndef GL_ACCUM_BUFFER_BIT
#  define GL_ACCUM_BUFFER_BIT    0x00000200
#endif
#ifndef GL_STENCIL_BUFFER_BIT
#  define GL_STENCIL_BUFFER_BIT  0x00000400
#endif
#ifndef GL_VIEWPORT_BIT
#  define GL_VIEWPORT_BIT        0x00000800
#endif
#ifndef GL_TRANSFORM_BIT
#  define GL_TRANSFORM_BIT       0x00001000
#endif
#ifndef GL_ENABLE_BIT
#  define GL_ENABLE_BIT          0x00002000
#endif
#ifndef GL_COLOR_BUFFER_BIT
#  define GL_COLOR_BUFFER_BIT    0x00004000
#endif
#ifndef GL_HINT_BIT
#  define GL_HINT_BIT            0x00008000
#endif
#ifndef GL_EVAL_BIT
#  define GL_EVAL_BIT            0x00010000
#endif
#ifndef GL_LIST_BIT
#  define GL_LIST_BIT            0x00020000
#endif
#ifndef GL_TEXTURE_BIT
#  define GL_TEXTURE_BIT         0x00040000
#endif
#ifndef GL_SCISSOR_BIT
#  define GL_SCISSOR_BIT         0x00080000
#endif
#ifndef GL_ALL_ATTRIB_BITS
#  define GL_ALL_ATTRIB_BITS     0x000FFFFF
#endif

/* ─── Clip planes ─────────────────────────────────────────────────────────── */
#ifndef GL_CLIP_PLANE0
#  define GL_CLIP_PLANE0         0x3000
#endif
#ifndef GL_CLIP_PLANE1
#  define GL_CLIP_PLANE1         0x3001
#endif
#ifndef GL_CLIP_PLANE2
#  define GL_CLIP_PLANE2         0x3002
#endif
#ifndef GL_CLIP_PLANE3
#  define GL_CLIP_PLANE3         0x3003
#endif
#ifndef GL_CLIP_PLANE4
#  define GL_CLIP_PLANE4         0x3004
#endif
#ifndef GL_CLIP_PLANE5
#  define GL_CLIP_PLANE5         0x3005
#endif
#ifndef GL_MAX_CLIP_PLANES
#  define GL_MAX_CLIP_PLANES     0x0D32
#endif

/* ─── Alpha test ─────────────────────────────────────────────────────────── */
#ifndef GL_ALPHA_TEST
#  define GL_ALPHA_TEST          0x0BC0
#endif
#ifndef GL_ALPHA_TEST_FUNC
#  define GL_ALPHA_TEST_FUNC     0x0BC1
#endif
#ifndef GL_ALPHA_TEST_REF
#  define GL_ALPHA_TEST_REF      0x0BC2
#endif

/* ─── Normalize, rescale ─────────────────────────────────────────────────── */
#ifndef GL_NORMALIZE
#  define GL_NORMALIZE           0x0BA1
#endif
#ifndef GL_RESCALE_NORMAL
#  define GL_RESCALE_NORMAL      0x803A
#endif

/* ─── Primitive types not in core profile ────────────────────────────────── */
#ifndef GL_QUADS
#  define GL_QUADS               0x0007
#endif
#ifndef GL_QUAD_STRIP
#  define GL_QUAD_STRIP          0x0008
#endif
#ifndef GL_POLYGON
#  define GL_POLYGON             0x0009
#endif

/* ─── Combine (GL_ARB_texture_env_combine) ───────────────────────────────── */
#ifndef GL_COMBINE
#  define GL_COMBINE             0x8570
#endif
#ifndef GL_COMBINE_RGB
#  define GL_COMBINE_RGB         0x8571
#endif
#ifndef GL_COMBINE_ALPHA
#  define GL_COMBINE_ALPHA       0x8572
#endif
#ifndef GL_SOURCE0_RGB
#  define GL_SOURCE0_RGB         0x8580
#endif
#ifndef GL_SOURCE1_RGB
#  define GL_SOURCE1_RGB         0x8581
#endif
#ifndef GL_SOURCE2_RGB
#  define GL_SOURCE2_RGB         0x8582
#endif
#ifndef GL_SOURCE0_ALPHA
#  define GL_SOURCE0_ALPHA       0x8588
#endif
#ifndef GL_SOURCE1_ALPHA
#  define GL_SOURCE1_ALPHA       0x8589
#endif
#ifndef GL_SOURCE2_ALPHA
#  define GL_SOURCE2_ALPHA       0x858A
#endif
#ifndef GL_OPERAND0_RGB
#  define GL_OPERAND0_RGB        0x8590
#endif
#ifndef GL_OPERAND1_RGB
#  define GL_OPERAND1_RGB        0x8591
#endif
#ifndef GL_OPERAND2_RGB
#  define GL_OPERAND2_RGB        0x8592
#endif
#ifndef GL_OPERAND0_ALPHA
#  define GL_OPERAND0_ALPHA      0x8598
#endif
#ifndef GL_OPERAND1_ALPHA
#  define GL_OPERAND1_ALPHA      0x8599
#endif
#ifndef GL_OPERAND2_ALPHA
#  define GL_OPERAND2_ALPHA      0x859A
#endif
#ifndef GL_RGB_SCALE
#  define GL_RGB_SCALE           0x8573
#endif
#ifndef GL_ALPHA_SCALE
#  define GL_ALPHA_SCALE         0x0D1C
#endif
#ifndef GL_PREVIOUS
#  define GL_PREVIOUS            0x8578
#endif
#ifndef GL_DOT3_RGB
#  define GL_DOT3_RGB            0x86AE
#endif
#ifndef GL_DOT3_RGBA
#  define GL_DOT3_RGBA           0x86AF
#endif

/* ─── Texture coord names ─────────────────────────────────────────────────── */
#ifndef GL_S
#  define GL_S                   0x2000
#endif
#ifndef GL_T
#  define GL_T                   0x2001
#endif
#ifndef GL_R
#  define GL_R                   0x2002
#endif
#ifndef GL_Q
#  define GL_Q                   0x2003
#endif

/* ─── Legacy vertex attribute names ─────────────────────────────────────── */
#ifndef GL_VERTEX_ATTRIB_ARRAY_POINTER
#  define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#endif

/* ─── glGetIntegerv tokens not in core ───────────────────────────────────── */
#ifndef GL_ACCUM_RED_BITS
#  define GL_ACCUM_RED_BITS      0x0D58
#endif
#ifndef GL_ACCUM_GREEN_BITS
#  define GL_ACCUM_GREEN_BITS    0x0D59
#endif
#ifndef GL_ACCUM_BLUE_BITS
#  define GL_ACCUM_BLUE_BITS     0x0D5A
#endif
#ifndef GL_ACCUM_ALPHA_BITS
#  define GL_ACCUM_ALPHA_BITS    0x0D5B
#endif
#ifndef GL_ACCUM
#  define GL_ACCUM               0x0100
#endif

/* ─── Raster / pixel ─────────────────────────────────────────────────────── */
#ifndef GL_AUTO_NORMAL
#  define GL_AUTO_NORMAL         0x0D80
#endif
#ifndef GL_ZOOM_X
#  define GL_ZOOM_X              0x0D16
#endif
#ifndef GL_ZOOM_Y
#  define GL_ZOOM_Y              0x0D17
#endif
#ifndef GL_PACK_ROW_LENGTH
#  define GL_PACK_ROW_LENGTH     0x0D02
#endif
#ifndef GL_PACK_SKIP_ROWS
#  define GL_PACK_SKIP_ROWS      0x0D03
#endif
#ifndef GL_PACK_SKIP_PIXELS
#  define GL_PACK_SKIP_PIXELS    0x0D04
#endif
#ifndef GL_PACK_SWAP_BYTES
#  define GL_PACK_SWAP_BYTES     0x0D00
#endif
#ifndef GL_PACK_LSB_FIRST
#  define GL_PACK_LSB_FIRST      0x0D01
#endif
#ifndef GL_PACK_SKIP_IMAGES
#  define GL_PACK_SKIP_IMAGES    0x806B
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#  define GL_UNPACK_ROW_LENGTH   0x0CF2
#endif
#ifndef GL_UNPACK_SKIP_ROWS
#  define GL_UNPACK_SKIP_ROWS    0x0CF3
#endif
#ifndef GL_UNPACK_SKIP_PIXELS
#  define GL_UNPACK_SKIP_PIXELS  0x0CF4
#endif
#ifndef GL_DEPTH_COMPONENT
#  define GL_DEPTH_COMPONENT     0x1902
#endif
#ifndef GL_DEPTH_COMPONENT32
#  define GL_DEPTH_COMPONENT32   0x81A7
#endif
#ifndef GL_DEPTH_CLEAR_VALUE
#  define GL_DEPTH_CLEAR_VALUE   0x0B73
#endif
#ifndef GL_COLOR_CLEAR_VALUE
#  define GL_COLOR_CLEAR_VALUE   0x0C22
#endif
#ifndef GL_CLAMP
#  define GL_CLAMP               0x2900
#endif

/* ─── Stack error codes ───────────────────────────────────────────────────── */
#ifndef GL_STACK_OVERFLOW
#  define GL_STACK_OVERFLOW      0x0503
#endif
#ifndef GL_STACK_UNDERFLOW
#  define GL_STACK_UNDERFLOW     0x0504
#endif

/* ─── ARB program extensions (symbol values only; functions are stubs) ──── */
#ifndef GL_VERTEX_PROGRAM_ARB
#  define GL_VERTEX_PROGRAM_ARB  0x8620
#endif
#ifndef GL_FRAGMENT_PROGRAM_ARB
#  define GL_FRAGMENT_PROGRAM_ARB 0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII_ARB
#  define GL_PROGRAM_FORMAT_ASCII_ARB 0x8875
#endif

/* ─── Compressed textures (ARB) ──────────────────────────────────────────── */
#ifndef GL_COMPRESSED_RGBA_ARB
#  define GL_COMPRESSED_RGBA_ARB 0x84EE
#endif

/* ─── Framebuffer-object (FBO) status values ─────────────────────────────── */
/* GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER
 * are defined as enum values in portablegl.h.  The completeness status codes
 * and legacy EXT aliases used by Obol's FBO compat layer are NOT in
 * portablegl.h and are defined here.                                          */
#ifndef GL_FRAMEBUFFER_COMPLETE
#  define GL_FRAMEBUFFER_COMPLETE                      0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
#  define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT         0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#  define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
#  define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER        0x8CDB
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
#  define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER        0x8CDC
#endif
#ifndef GL_FRAMEBUFFER_UNDEFINED
#  define GL_FRAMEBUFFER_UNDEFINED                     0x8219
#endif
/* EXT aliases (used by Obol's SoSceneTexture2 and shadow map code) */
#ifndef GL_FRAMEBUFFER_EXT
#  define GL_FRAMEBUFFER_EXT          GL_FRAMEBUFFER
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE_EXT
#  define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
#endif
#ifndef GL_COLOR_ATTACHMENT0_EXT
#  define GL_COLOR_ATTACHMENT0_EXT    GL_COLOR_ATTACHMENT0
#endif
#ifndef GL_DEPTH_ATTACHMENT_EXT
#  define GL_DEPTH_ATTACHMENT_EXT     GL_DEPTH_ATTACHMENT
#endif
#ifndef GL_STENCIL_ATTACHMENT_EXT
#  define GL_STENCIL_ATTACHMENT_EXT   GL_STENCIL_ATTACHMENT
#endif
#ifndef GL_RENDERBUFFER_EXT
#  define GL_RENDERBUFFER_EXT         GL_RENDERBUFFER
#endif
/* GL_PACK_ROW_LENGTH may not be defined in portablegl.h (core 3.x only) */
#ifndef GL_PACK_ROW_LENGTH
#  define GL_PACK_ROW_LENGTH          0x0D02
#endif

/* ─── glFinish / glFlush ─────────────────────────────────────────────────── */
/* PortableGL is synchronous (CPU-based); Finish/Flush are no-ops.
 * Defined here so Obol source files that call them compile without changes.  */
#ifndef GL_FINISH_DEFINED
#  define GL_FINISH_DEFINED
static inline void glFinish(void) {}
static inline void glFlush(void)  {}
#endif

/* ─── Current state queries ──────────────────────────────────────────────── */
#ifndef GL_CURRENT_COLOR
#  define GL_CURRENT_COLOR       0x0B00
#endif
#ifndef GL_RED_BITS
#  define GL_RED_BITS            0x0D52
#endif
#ifndef GL_GREEN_BITS
#  define GL_GREEN_BITS          0x0D53
#endif
#ifndef GL_BLUE_BITS
#  define GL_BLUE_BITS           0x0D54
#endif
#ifndef GL_RGBA_MODE
#  define GL_RGBA_MODE           0x0C31
#endif
#ifndef GL_LINE_STIPPLE
#  define GL_LINE_STIPPLE        0x0B24
#endif

/* ─── ARB shader object tokens (used by Obol's GLSL-over-PGL path) ──────── */
#ifndef GL_FRAGMENT_SHADER_ARB
#  define GL_FRAGMENT_SHADER_ARB            0x8B30
#endif
#ifndef GL_OBJECT_COMPILE_STATUS_ARB
#  define GL_OBJECT_COMPILE_STATUS_ARB      0x8B81
#endif
#ifndef GL_OBJECT_LINK_STATUS_ARB
#  define GL_OBJECT_LINK_STATUS_ARB         0x8B82
#endif
#ifndef GL_OBJECT_INFO_LOG_LENGTH_ARB
#  define GL_OBJECT_INFO_LOG_LENGTH_ARB     0x8B84
#endif

/* ─── GL 1.x legacy pixel-pipeline / framebuffer-query constants ─────────── */
/* These are used by Obol's CoinOffscreenGLCanvas and related paths for pixel
 * transfer state save/restore. PortableGL ignores pixel transfer state.       */#ifndef GL_MAP_COLOR
#  define GL_MAP_COLOR                      0x0D10
#endif
#ifndef GL_MAP_STENCIL
#  define GL_MAP_STENCIL                    0x0D11
#endif
#ifndef GL_INDEX_SHIFT
#  define GL_INDEX_SHIFT                    0x0D12
#endif
#ifndef GL_INDEX_OFFSET
#  define GL_INDEX_OFFSET                   0x0D13
#endif
#ifndef GL_RED_SCALE
#  define GL_RED_SCALE                      0x0D14
#endif
#ifndef GL_RED_BIAS
#  define GL_RED_BIAS                       0x0D15
#endif
#ifndef GL_GREEN_SCALE
#  define GL_GREEN_SCALE                    0x0D18
#endif
#ifndef GL_GREEN_BIAS
#  define GL_GREEN_BIAS                     0x0D19
#endif
#ifndef GL_BLUE_SCALE
#  define GL_BLUE_SCALE                     0x0D1A
#endif
#ifndef GL_BLUE_BIAS
#  define GL_BLUE_BIAS                      0x0D1B
#endif
#ifndef GL_ALPHA_BIAS
#  define GL_ALPHA_BIAS                     0x0D1D
#endif
#ifndef GL_DEPTH_SCALE
#  define GL_DEPTH_SCALE                    0x0D1E
#endif
#ifndef GL_DEPTH_BIAS
#  define GL_DEPTH_BIAS                     0x0D1F
#endif
#ifndef GL_PIXEL_MAP_I_TO_I
#  define GL_PIXEL_MAP_I_TO_I               0x0C70
#endif
#ifndef GL_PIXEL_MAP_S_TO_S
#  define GL_PIXEL_MAP_S_TO_S               0x0C71
#endif
#ifndef GL_PIXEL_MAP_I_TO_R
#  define GL_PIXEL_MAP_I_TO_R               0x0C72
#endif
#ifndef GL_PIXEL_MAP_I_TO_G
#  define GL_PIXEL_MAP_I_TO_G               0x0C73
#endif
#ifndef GL_PIXEL_MAP_I_TO_B
#  define GL_PIXEL_MAP_I_TO_B               0x0C74
#endif
#ifndef GL_PIXEL_MAP_I_TO_A
#  define GL_PIXEL_MAP_I_TO_A               0x0C75
#endif
#ifndef GL_PIXEL_MAP_R_TO_R
#  define GL_PIXEL_MAP_R_TO_R               0x0C76
#endif
#ifndef GL_PIXEL_MAP_G_TO_G
#  define GL_PIXEL_MAP_G_TO_G               0x0C77
#endif
#ifndef GL_PIXEL_MAP_B_TO_B
#  define GL_PIXEL_MAP_B_TO_B               0x0C78
#endif
#ifndef GL_PIXEL_MAP_A_TO_A
#  define GL_PIXEL_MAP_A_TO_A               0x0C79
#endif
#ifndef GL_ALPHA_BITS
#  define GL_ALPHA_BITS                     0x0D55
#endif
#ifndef GL_STENCIL_INDEX
#  define GL_STENCIL_INDEX                  0x1901
#endif
/* GL_ACCUM buffer operations */
#ifndef GL_LOAD
#  define GL_LOAD                           0x0101
#endif
#ifndef GL_RETURN
#  define GL_RETURN                         0x0102
#endif
/* Draw-buffer identifiers for stereo/mono rendering */
#ifndef GL_FRONT_LEFT
#  define GL_FRONT_LEFT                     0x0400
#endif
#ifndef GL_FRONT_RIGHT
#  define GL_FRONT_RIGHT                    0x0401
#endif
#ifndef GL_BACK_LEFT
#  define GL_BACK_LEFT                      0x0402
#endif
#ifndef GL_BACK_RIGHT
#  define GL_BACK_RIGHT                     0x0403
#endif

/* ─── String / capability query tokens ──────────────────────────────────── */
#ifndef GL_EXTENSIONS
#  define GL_EXTENSIONS                     0x1F03
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#  define GL_MAX_TEXTURE_UNITS              0x84E2
#endif
#ifndef GL_MAX_TEXTURE_UNITS_ARB
#  define GL_MAX_TEXTURE_UNITS_ARB          0x84E2
#endif
#ifndef GL_MAX_VIEWPORT_DIMS
#  define GL_MAX_VIEWPORT_DIMS              0x0D3A
#endif
#ifndef GL_POINT_SIZE
#  define GL_POINT_SIZE                     0x0B11
#endif
#ifndef GL_POINT_SIZE_RANGE
#  define GL_POINT_SIZE_RANGE               0x0B12
#endif
#ifndef GL_LINE_WIDTH
#  define GL_LINE_WIDTH                     0x0B21
#endif
#ifndef GL_LINE_WIDTH_RANGE
#  define GL_LINE_WIDTH_RANGE               0x0B22
#endif
#ifndef GL_VIEWPORT_BIT
#  define GL_VIEWPORT_BIT                   0x00000800
#endif

/* ─── Luminance / luminance-alpha texture formats ────────────────────────── */
#ifndef GL_LUMINANCE
#  define GL_LUMINANCE                      0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#  define GL_LUMINANCE_ALPHA                0x190A
#endif
#ifndef GL_LUMINANCE8
#  define GL_LUMINANCE8                     0x8040
#endif
#ifndef GL_LUMINANCE8_ALPHA8
#  define GL_LUMINANCE8_ALPHA8              0x8045
#endif

/* ─── Texture dimension queries ──────────────────────────────────────────── */
#ifndef GL_TEXTURE_HEIGHT
#  define GL_TEXTURE_HEIGHT                 0x1001
#endif
#ifndef GL_TEXTURE_WIDTH
#  define GL_TEXTURE_WIDTH                  0x1000
#endif
#ifndef GL_TEXTURE_DEPTH
#  define GL_TEXTURE_DEPTH                  0x8071
#endif
#ifndef GL_PROXY_TEXTURE_2D
#  define GL_PROXY_TEXTURE_2D               0x8064
#endif
#ifndef GL_PROXY_TEXTURE_3D
#  define GL_PROXY_TEXTURE_3D               0x8070
#endif
/* glGetTexLevelParameteriv: PortableGL has no proxy texture mechanism.
 * Always indicate that the texture can be created (non-zero width).           */
#ifndef GL_GETTEXLEVELPARAMETERIV_STUB
#  define GL_GETTEXLEVELPARAMETERIV_STUB
static inline void glGetTexLevelParameteriv(GLenum /*target*/, GLint /*level*/,
                                            GLenum pname, GLint* params) {
    if (params)
        *params = (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT ||
                   pname == GL_TEXTURE_DEPTH) ? 1 : 0;
}
#endif

/* ─── Shade model ────────────────────────────────────────────────────────── */
/* PortableGL uses PGL_FLAT / PGL_SMOOTH for per-shader interpolation;
 * glShadeModel is provided here as a no-op so Obol source files compile
 * unchanged.  Gouraud/Phong selection is driven by Obol's own shader
 * selector, not by the GL shade-model state.                                  */
#ifndef GL_FLAT
#  define GL_FLAT                           0x1D00
#endif
#ifndef GL_SMOOTH
#  define GL_SMOOTH                         0x1D01
#endif
#ifndef GL_SHADEMODEL_STUB
#  define GL_SHADEMODEL_STUB
static inline void glShadeModel(GLenum /*mode*/) {}
#endif

/* ─── Display lists (not supported in PortableGL) ───────────────────────── */
#ifndef GL_COMPILE
#  define GL_COMPILE                        0x1300
#endif
#ifndef GL_COMPILE_AND_EXECUTE
#  define GL_COMPILE_AND_EXECUTE            0x1301
#endif

/* ─── Polygon stipple ────────────────────────────────────────────────────── */
#ifndef GL_POLYGON_STIPPLE
#  define GL_POLYGON_STIPPLE                0x0B42
#endif

/* ─── Miscellaneous constants missing from PortableGL ───────────────────── */
#ifndef GL_NONE
#  define GL_NONE                           0
#endif
#ifndef GL_BLUE
#  define GL_BLUE                           0x1905
#endif
#ifndef GL_DEPTH_BITS
#  define GL_DEPTH_BITS                     0x0D56
#endif
#ifndef GL_EYE_PLANE
#  define GL_EYE_PLANE                      0x2502
#endif

#endif /* PORTABLEGL_COMPAT_CONSTS_H */
