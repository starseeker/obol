/*
 * portablegl_compat_state.h  –  OpenGL compatibility-profile state for Obol/PortableGL
 *
 * Obol's internally-generated shaders are written in GLSL 1.10 compatibility
 * profile and rely on OpenGL's fixed-function state as GLSL built-ins:
 *
 *   gl_ModelViewMatrix, gl_ProjectionMatrix, gl_ModelViewProjectionMatrix,
 *   gl_NormalMatrix, gl_Vertex, gl_Normal, gl_Color, gl_MultiTexCoord0/1,
 *   gl_LightSource[i], gl_FrontMaterial, gl_FrontLightModelProduct
 *
 * PortableGL is a core-profile-style renderer with no built-in fixed-function
 * state.  This header defines the ObolPGLCompatState struct that mirrors all
 * the GLSL compat-profile state that Obol needs, plus matrix-stack helpers.
 *
 * ObolPGLCompatState is:
 *  • stored per-PortableGL-context (inside PGLContextData in SoDBPortableGL.cpp)
 *  • passed as the 'void* uniforms' pointer to every PortableGL C shader
 *  • updated by our interceptor implementations of glMatrixMode, glLoadMatrixf,
 *    glLightfv, glMaterialfv, etc. (see portablegl_compat_funcs.h)
 *
 * Proposed upstream contribution to PortableGL
 * ─────────────────────────────────────────────
 * The additions described here (matrix stack, light/material state) are
 * proposed as a "GL 1.x compatibility emulation layer" addition to PortableGL.
 * PortableGL's own source comment says "Meant to ease the transition from old
 * fixed function"; this struct formalises that intention.
 */

#ifndef OBOL_PORTABLEGL_COMPAT_STATE_H
#define OBOL_PORTABLEGL_COMPAT_STATE_H

#include <cstring>
#include <cmath>

/* Include PortableGL declarations only (no implementation). */
#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#include <portablegl/portablegl.h>
#include <portablegl/portablegl_compat_consts.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OBOL_PGL_MAX_LIGHTS       8
#define OBOL_PGL_MATRIX_STACK     32
#define OBOL_PGL_MAX_TEX_UNITS    4

/* Magic uniform locations returned by our glGetUniformLocationARB interceptor.
 * These are non-overlapping ranges so the glUniform* dispatcher knows what to
 * update in ObolPGLCompatState. */
enum ObolPGLUniformLocation : GLint {
    UPGL_INVALID             = -1,
    /* coin_ prefix uniforms */
    UPGL_LIGHT_MODEL         = 0x1000,
    UPGL_TWO_SIDED           = 0x1001,
    UPGL_TEXUNIT0_MODEL      = 0x1010,
    UPGL_TEXUNIT1_MODEL      = 0x1011,
    UPGL_TEXUNIT2_MODEL      = 0x1012,
    UPGL_TEXUNIT3_MODEL      = 0x1013,
    /* texture sampler uniforms */
    UPGL_TEXMAP0             = 0x2000,
    UPGL_TEXMAP1             = 0x2001,
    UPGL_TEXMAP2             = 0x2002,
    UPGL_TEXMAP3             = 0x2003,
    /* shadow uniforms (range 0x3000–0x30FF) */
    UPGL_SHADOW_MATRIX_BASE  = 0x3000,   /* +light_idx */
    UPGL_SHADOW_MAP_BASE     = 0x3008,
    UPGL_SHADOW_FARVAL_BASE  = 0x3010,
    UPGL_SHADOW_NEARVAL_BASE = 0x3018,
    UPGL_SHADOW_LIGHTPLANE_BASE = 0x3020,
    UPGL_CAMERA_TRANSFORM    = 0x3100,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Sub-structs mirroring GLSL compatibility built-in state
 * ═══════════════════════════════════════════════════════════════════════════ */

struct ObolPGLLightState {
    float ambient[4]       = {0,0,0,1};
    float diffuse[4]       = {1,1,1,1};
    float specular[4]      = {1,1,1,1};
    float position[4]      = {0,0,1,0};   /* w=0 → directional */
    float spot_direction[3]= {0,0,-1};
    float spot_exponent    = 0.f;
    float spot_cutoff      = 180.f;       /* disabled */
    float spot_cos_cutoff  = -1.f;
    float attenuation[3]   = {1,0,0};    /* const, linear, quad */
    float half_vector[4]   = {0,0,1,0};  /* in eye space */
    bool  enabled          = false;
};

struct ObolPGLMaterialState {
    float ambient[4]  = {0.2f,0.2f,0.2f,1.f};
    float diffuse[4]  = {0.8f,0.8f,0.8f,1.f};
    float specular[4] = {0.f,0.f,0.f,1.f};
    float emission[4] = {0.f,0.f,0.f,1.f};
    float shininess   = 0.f;
};

/* Shadow-map state (per shadow light, up to OBOL_PGL_MAX_LIGHTS) */
struct ObolPGLShadowState {
    float shadow_matrix[16] = {};   /* gl_ModelViewProjectionMatrix * bias */
    GLuint shadow_map       = 0;    /* depth texture handle */
    float  farval           = 1.f;
    float  nearval          = 0.f;
    float  lightplane[4]    = {};   /* optional light plane */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * 4×4 matrix helpers (column-major, matching OpenGL convention)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Identity matrix */
inline void obol_pgl_identity(float m[16]) {
    memset(m, 0, 16*sizeof(float));
    m[0]=m[5]=m[10]=m[15]=1.f;
}

/* C = A × B  (all column-major) */
inline void obol_pgl_matmul(float C[16], const float A[16], const float B[16]) {
    for (int col=0; col<4; col++)
        for (int row=0; row<4; row++) {
            float s=0;
            for (int k=0; k<4; k++) s += A[k*4+row]*B[col*4+k];
            C[col*4+row]=s;
        }
}

/* Extract upper-left 3×3 of M, compute inverse-transpose → normal matrix (row-major for GLSL mat3) */
inline void obol_pgl_normal_matrix(float N[9], const float M[16]) {
    /* upper-left 3x3 columns */
    float a=M[0], b=M[4], c=M[8];
    float d=M[1], e=M[5], f=M[9];
    float g=M[2], h=M[6], i=M[10];
    float det = a*(e*i-f*h) - b*(d*i-f*g) + c*(d*h-e*g);
    if (fabsf(det) < 1e-10f) {
        /* Degenerate – fall back to upper-left 3x3 */
        N[0]=a; N[3]=b; N[6]=c;
        N[1]=d; N[4]=e; N[7]=f;
        N[2]=g; N[5]=h; N[8]=i;
        return;
    }
    float inv = 1.f/det;
    /* adjugate transpose (= inverse of transpose) */
    N[0] = ( e*i-f*h)*inv;  N[3] = -(b*i-c*h)*inv;  N[6] = ( b*f-c*e)*inv;
    N[1] = -(d*i-f*g)*inv;  N[4] = ( a*i-c*g)*inv;  N[7] = -(a*f-c*d)*inv;
    N[2] = ( d*h-e*g)*inv;  N[5] = -(a*h-b*g)*inv;  N[8] = ( a*e-b*d)*inv;
}

/* Transform 4-vector by column-major matrix */
inline void obol_pgl_mv4(float out[4], const float M[16], const float v[4]) {
    for (int r=0;r<4;r++) {
        out[r]=0;
        for (int c=0;c<4;c++) out[r]+=M[c*4+r]*v[c];
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main compatibility state struct
 * ═══════════════════════════════════════════════════════════════════════════ */

struct ObolPGLCompatState {
    /* ── Matrix stacks ─────────────────────────────────────────────────── */
    float  modelview_stack[OBOL_PGL_MATRIX_STACK][16];
    float  projection_stack[OBOL_PGL_MATRIX_STACK][16];
    float  texture_stack[OBOL_PGL_MATRIX_STACK][16];
    int    modelview_depth;
    int    projection_depth;
    int    texture_depth;
    GLenum matrix_mode;

    /* Derived matrices (updated whenever the stacks change) */
    float  mvp[16];        /* projection × modelview */
    float  normal_mat[9];  /* inverse-transpose of MV upper-left 3×3 */

    /* ── Light state (mirrors gl_LightSource[]) ─────────────────────── */
    ObolPGLLightState lights[OBOL_PGL_MAX_LIGHTS];
    float global_ambient[4];   /* gl_LightModel.ambient */
    int   light_model;         /* coin_light_model: 1=PHONG, 0=BASE_COLOR */
    int   two_sided_lighting;  /* coin_two_sided_lighting */

    /* ── Material state (mirrors gl_FrontMaterial) ───────────────────── */
    ObolPGLMaterialState front_material;
    /* gl_FrontLightModelProduct.sceneColor = emission + global_ambient*ambient */

    /* ── Texture state ───────────────────────────────────────────────── */
    GLuint tex_unit[OBOL_PGL_MAX_TEX_UNITS];    /* bound texture per unit */
    int    texunit_model[OBOL_PGL_MAX_TEX_UNITS]; /* coin_texunit%d_model */

    /* ── Shadow state ────────────────────────────────────────────────── */
    ObolPGLShadowState shadows[OBOL_PGL_MAX_LIGHTS];
    float  camera_transform[16]; /* cameraTransform uniform */

    /* ── Fog ─────────────────────────────────────────────────────────── */
    float  fog_color[4];
    float  fog_density;
    float  fog_start;
    float  fog_end;
    GLenum fog_mode;
    bool   fog_enabled;

    /* ── Immediate-mode current state ───────────────────────────────── */
    float  current_color[4];
    float  current_normal[3];
    float  current_texcoord[2];

    /* ── Initialise to OpenGL defaults ──────────────────────────────── */
    void init() {
        obol_pgl_identity(modelview_stack[0]);
        obol_pgl_identity(projection_stack[0]);
        obol_pgl_identity(texture_stack[0]);
        modelview_depth = projection_depth = texture_depth = 0;
        matrix_mode = GL_MODELVIEW;
        obol_pgl_identity(mvp);
        obol_pgl_normal_matrix(normal_mat, modelview_stack[0]);

        lights[0].diffuse[0]=lights[0].diffuse[1]=lights[0].diffuse[2]=lights[0].diffuse[3]=1.f;
        lights[0].specular[0]=lights[0].specular[1]=lights[0].specular[2]=lights[0].specular[3]=1.f;
        for (int i=1;i<OBOL_PGL_MAX_LIGHTS;i++) {
            lights[i].diffuse[0]=lights[i].diffuse[1]=lights[i].diffuse[2]=0.f;
            lights[i].diffuse[3]=1.f;
            lights[i].specular[0]=lights[i].specular[1]=lights[i].specular[2]=0.f;
            lights[i].specular[3]=1.f;
        }
        global_ambient[0]=global_ambient[1]=global_ambient[2]=0.2f; global_ambient[3]=1.f;
        light_model = 1;   /* PHONG by default */
        two_sided_lighting = 0;

        for (int u=0;u<OBOL_PGL_MAX_TEX_UNITS;u++) { tex_unit[u]=0; texunit_model[u]=0; }
        obol_pgl_identity(camera_transform);

        fog_color[0]=fog_color[1]=fog_color[2]=0.f; fog_color[3]=1.f;
        fog_density=1.f; fog_start=0.f; fog_end=1.f;
        fog_mode=GL_EXP; fog_enabled=false;

        current_color[0]=current_color[1]=current_color[2]=current_color[3]=1.f;
        current_normal[0]=current_normal[1]=0.f; current_normal[2]=1.f;
        current_texcoord[0]=current_texcoord[1]=0.f;
    }

    /* Recompute derived matrices.  Must be called whenever MV or P stacks change. */
    void update_derived() {
        obol_pgl_matmul(mvp, projection_stack[projection_depth],
                        modelview_stack[modelview_depth]);
        obol_pgl_normal_matrix(normal_mat, modelview_stack[modelview_depth]);
    }

    float* current_matrix() {
        switch (matrix_mode) {
        case GL_PROJECTION: return projection_stack[projection_depth];
        case GL_TEXTURE:    return texture_stack[texture_depth];
        default:            return modelview_stack[modelview_depth];
        }
    }

    /* ── glMatrixMode ─────────────────────────────────────────────────── */
    void matrix_mode_set(GLenum mode) { matrix_mode = mode; }

    /* ── glLoadIdentity ───────────────────────────────────────────────── */
    void load_identity() {
        obol_pgl_identity(current_matrix());
        update_derived();
    }

    /* ── glLoadMatrixf ───────────────────────────────────────────────── */
    void load_matrixf(const float m[16]) {
        memcpy(current_matrix(), m, 64);
        update_derived();
    }

    /* ── glMultMatrixf ───────────────────────────────────────────────── */
    void mult_matrixf(const float m[16]) {
        float tmp[16];
        obol_pgl_matmul(tmp, current_matrix(), m);
        memcpy(current_matrix(), tmp, 64);
        update_derived();
    }

    /* ── glPushMatrix ────────────────────────────────────────────────── */
    void push_matrix() {
        switch (matrix_mode) {
        case GL_PROJECTION:
            if (projection_depth < OBOL_PGL_MATRIX_STACK-1) {
                memcpy(projection_stack[projection_depth+1],
                       projection_stack[projection_depth], 64);
                projection_depth++;
            }
            break;
        case GL_TEXTURE:
            if (texture_depth < OBOL_PGL_MATRIX_STACK-1) {
                memcpy(texture_stack[texture_depth+1],
                       texture_stack[texture_depth], 64);
                texture_depth++;
            }
            break;
        default:
            if (modelview_depth < OBOL_PGL_MATRIX_STACK-1) {
                memcpy(modelview_stack[modelview_depth+1],
                       modelview_stack[modelview_depth], 64);
                modelview_depth++;
            }
        }
    }

    /* ── glPopMatrix ─────────────────────────────────────────────────── */
    void pop_matrix() {
        switch (matrix_mode) {
        case GL_PROJECTION: if (projection_depth>0) { projection_depth--; update_derived(); } break;
        case GL_TEXTURE:    if (texture_depth>0)    { texture_depth--; } break;
        default:            if (modelview_depth>0)  { modelview_depth--; update_derived(); } break;
        }
    }
};

/* Convenience accessor – cast the void* uniforms pointer in C shaders */
#define OBOL_PGL_STATE(uniforms) (static_cast<ObolPGLCompatState*>(uniforms))

#endif /* OBOL_PORTABLEGL_COMPAT_STATE_H */
