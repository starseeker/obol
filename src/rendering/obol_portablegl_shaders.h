/*
 * obol_portablegl_shaders.h -- PortableGL C-function Phong & BaseColor shaders
 *
 * PortableGL does not compile GLSL; shader programs are C function pairs
 * (vertex + fragment) created via pglCreateProgram().  This header provides
 * equivalents of OBOL_SHADER_PHONG and OBOL_SHADER_BASECOLOR using
 * portablegl's native C-shader interface.
 *
 * Attrib layout (must match SoPrimitiveVertexCache / SoGL shape renderers):
 *   index 0 -- position  (vec3 / vec4)
 *   index 1 -- normal    (vec3)
 *   index 2 -- texcoord  (vec2)
 *   index 3 -- color     (vec4 normalized u8 or float)
 *
 * Uniform structs are passed via pglSetUniform().  SoGLModernState fills in
 * the fields from its internal state before calling glDrawArrays/Elements.
 *
 * The matrices mv_gl, proj_gl, nm_gl are stored row-major in SoGLModernState
 * (matching Open Inventor convention; uploaded to GLSL with GL_TRUE transpose).
 * The C shaders therefore use a row-major mat-vec multiply.
 *
 * Include only from files compiled with OBOL_PORTABLEGL_BUILD defined and
 * with portablegl.h already included.
 */

#ifndef OBOL_PORTABLEGL_SHADERS_H
#define OBOL_PORTABLEGL_SHADERS_H

#ifdef OBOL_PORTABLEGL_BUILD

#include <cmath>
#include <cstring>
#include "rendering/SoGLModernState.h"   /* OBOL_MODERN_MAX_LIGHTS */

/* =========================================================================
 * Row-major matrix helpers (OpenInventor / Coin convention)
 * ======================================================================= */

/* Multiply a 4×4 row-major matrix by a column vec4 */
static inline vec4 pgl_rm4_v4(const float* M, vec4 v)
{
    vec4 r;
    r.x = M[0]*v.x  + M[1]*v.y  + M[2]*v.z  + M[3]*v.w;
    r.y = M[4]*v.x  + M[5]*v.y  + M[6]*v.z  + M[7]*v.w;
    r.z = M[8]*v.x  + M[9]*v.y  + M[10]*v.z + M[11]*v.w;
    r.w = M[12]*v.x + M[13]*v.y + M[14]*v.z + M[15]*v.w;
    return r;
}

/* Multiply a 3×3 row-major matrix by a column vec3; return as vec3 via xyz */
static inline vec4 pgl_rm3_v3(const float* M, vec4 v)
{
    vec4 r;
    r.x = M[0]*v.x + M[1]*v.y + M[2]*v.z;
    r.y = M[3]*v.x + M[4]*v.y + M[5]*v.z;
    r.z = M[6]*v.x + M[7]*v.y + M[8]*v.z;
    r.w = 0.0f;
    return r;
}

/* Normalize the xyz portion of a vec4 in place */
static inline void pgl_normalize_xyz(vec4* v)
{
    float len = std::sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
    if (len > 1e-7f) { float inv = 1.0f / len; v->x *= inv; v->y *= inv; v->z *= inv; }
}

/* Dot product of two vec4 (xyz only) */
static inline float pgl_dot3(vec4 a, vec4 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

/* =========================================================================
 * Phong shader uniforms
 * ======================================================================= */

struct PGLPhongLight {
    float position[4];
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float spotDirection[3];
    float spotCutoff;
    float spotExponent;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
    float _pad[2];
};

struct PGLPhongUniforms {
    float mv[16];        /* row-major model-view */
    float proj[16];      /* row-major projection */
    float nm[9];         /* row-major 3x3 normal matrix */
    float _pad_nm[3];    /* pad to 16-byte boundary */
    int   numLights;
    int   hasNormals;
    int   hasColors;
    int   hasTexCoords;
    float matAmbient[4];
    float matDiffuse[4];
    float matSpecular[4];
    float matEmission[4];
    float matShininess;
    int   twoSided;
    float _pad2[2];
    PGLPhongLight lights[OBOL_MODERN_MAX_LIGHTS];
};

/* vs_output layout:  0-2=normalEye xyz  3-5=positionEye xyz  6-7=texcoord xy  8-11=color xyzw */
#define PGL_PHONG_VS_OUT 12

static void pgl_phong_vs(float* vs_output,
                         vec4*  vertex_attribs,
                         Shader_Builtins* builtins,
                         void*  uniforms_ptr)
{
    const PGLPhongUniforms* u = static_cast<const PGLPhongUniforms*>(uniforms_ptr);

    /* Position */
    vec4 pos4; pos4.x = vertex_attribs[0].x; pos4.y = vertex_attribs[0].y;
               pos4.z = vertex_attribs[0].z; pos4.w = 1.0f;
    vec4 posEye = pgl_rm4_v4(u->mv, pos4);

    /* Normal — only read attrib[1] when per-vertex normals are provided.
     * When hasNormals==0 the normal array may not be valid; use (0,0,1). */
    vec4 normEye;
    if (u->hasNormals) {
        vec4 nrm4; nrm4.x = vertex_attribs[1].x; nrm4.y = vertex_attribs[1].y;
                   nrm4.z = vertex_attribs[1].z; nrm4.w = 0.0f;
        normEye = pgl_rm3_v3(u->nm, nrm4);
        pgl_normalize_xyz(&normEye);
    } else {
        normEye.x = 0.0f; normEye.y = 0.0f; normEye.z = 1.0f; normEye.w = 0.0f;
    }

    /* Clip position */
    builtins->gl_Position = pgl_rm4_v4(u->proj, posEye);

    /* normalEye */
    vs_output[0] = normEye.x;
    vs_output[1] = normEye.y;
    vs_output[2] = normEye.z;
    /* positionEye */
    vs_output[3] = posEye.x;
    vs_output[4] = posEye.y;
    vs_output[5] = posEye.z;
    /* texcoord */
    if (u->hasTexCoords) {
        vs_output[6] = vertex_attribs[2].x;
        vs_output[7] = vertex_attribs[2].y;
    } else {
        vs_output[6] = 0.0f;
        vs_output[7] = 0.0f;
    }
    /* color (attrib 3 normalized u8→float or float4) */
    if (u->hasColors) {
        vs_output[8]  = vertex_attribs[3].x;
        vs_output[9]  = vertex_attribs[3].y;
        vs_output[10] = vertex_attribs[3].z;
        vs_output[11] = vertex_attribs[3].w;
    } else {
        vs_output[8] = vs_output[9] = vs_output[10] = vs_output[11] = 1.0f;
    }
}

static void pgl_phong_fs(float* fs_input,
                         Shader_Builtins* builtins,
                         void*  uniforms_ptr)
{
    const PGLPhongUniforms* u = static_cast<const PGLPhongUniforms*>(uniforms_ptr);

    vec4 normEye;
    normEye.x = fs_input[0]; normEye.y = fs_input[1];
    normEye.z = fs_input[2]; normEye.w = 0.0f;
    pgl_normalize_xyz(&normEye);
    if (!builtins->gl_FrontFacing && u->twoSided) {
        normEye.x = -normEye.x; normEye.y = -normEye.y; normEye.z = -normEye.z;
    }

    vec4 posEye;
    posEye.x = fs_input[3]; posEye.y = fs_input[4];
    posEye.z = fs_input[5]; posEye.w = 0.0f;

    vec4 vColor;
    vColor.x = fs_input[8]; vColor.y = fs_input[9];
    vColor.z = fs_input[10]; vColor.w = fs_input[11];

    vec4 fragColor;

    if (u->numLights > 0) {
        float amb[4] = {0,0,0,0}, dif[4] = {0,0,0,0}, spc[4] = {0,0,0,0};
        /* View direction (toward eye) */
        vec4 V; V.x = -posEye.x; V.y = -posEye.y; V.z = -posEye.z; V.w = 0.0f;
        pgl_normalize_xyz(&V);

        for (int i = 0; i < u->numLights && i < OBOL_MODERN_MAX_LIGHTS; ++i) {
            const PGLPhongLight& li = u->lights[i];
            vec4 L;
            float att = 1.0f;
            if (li.position[3] == 0.0f) {
                /* Directional: SoDirectionalLight::GLRender stores
                 * ml.position = -light.direction (i.e. toward the light),
                 * matching the OpenGL fixed-function GL_POSITION convention.
                 * Use it directly as L – do NOT negate again. */
                L.x = li.position[0]; L.y = li.position[1];
                L.z = li.position[2]; L.w = 0.0f;
                pgl_normalize_xyz(&L);
            } else {
                /* Positional */
                L.x = li.position[0] - posEye.x;
                L.y = li.position[1] - posEye.y;
                L.z = li.position[2] - posEye.z;
                L.w = 0.0f;
                float dist = std::sqrt(L.x*L.x + L.y*L.y + L.z*L.z);
                pgl_normalize_xyz(&L);
                float ca = li.constantAttenuation;
                float la = li.linearAttenuation;
                float qa = li.quadraticAttenuation;
                att = 1.0f / (ca + la*dist + qa*dist*dist);
            }

            /* Spotlight factor */
            float spot = 1.0f;
            if (li.spotCutoff > -0.5f) {
                vec4 sd; sd.x = li.spotDirection[0]; sd.y = li.spotDirection[1];
                         sd.z = li.spotDirection[2]; sd.w = 0.0f;
                pgl_normalize_xyz(&sd);
                vec4 negL; negL.x = -L.x; negL.y = -L.y; negL.z = -L.z; negL.w = 0.0f;
                float cosA = pgl_dot3(negL, sd);
                spot = (cosA >= li.spotCutoff)
                    ? std::pow(cosA, li.spotExponent) : 0.0f;
            }

            float diff = pgl_dot3(normEye, L);
            if (diff < 0.0f) diff = 0.0f;
            float spec = 0.0f;
            if (diff > 0.0f) {
                /* Reflect: R = 2*(N·L)*N - L */
                float ndl2 = 2.0f * pgl_dot3(normEye, L);
                vec4 R; R.x = ndl2*normEye.x - L.x; R.y = ndl2*normEye.y - L.y;
                         R.z = ndl2*normEye.z - L.z; R.w = 0.0f;
                pgl_normalize_xyz(&R);
                float rdv = pgl_dot3(R, V);
                if (rdv > 0.0f) spec = std::pow(rdv, u->matShininess);
            }

            float f = att * spot;
            for (int c = 0; c < 4; ++c) {
                amb[c] += f * li.ambient[c];
                dif[c] += f * diff * li.diffuse[c];
                spc[c] += f * spec * li.specular[c];
            }
        }

        fragColor.x = u->matEmission[0] + amb[0]*u->matAmbient[0] + dif[0]*(u->hasColors ? vColor.x : u->matDiffuse[0]) + spc[0]*u->matSpecular[0];
        fragColor.y = u->matEmission[1] + amb[1]*u->matAmbient[1] + dif[1]*(u->hasColors ? vColor.y : u->matDiffuse[1]) + spc[1]*u->matSpecular[1];
        fragColor.z = u->matEmission[2] + amb[2]*u->matAmbient[2] + dif[2]*(u->hasColors ? vColor.z : u->matDiffuse[2]) + spc[2]*u->matSpecular[2];
        fragColor.w = u->matEmission[3] + amb[3]*u->matAmbient[3] + dif[3]*(u->hasColors ? vColor.w : u->matDiffuse[3]) + spc[3]*u->matSpecular[3];
    } else {
        /* No lights: emission + diffuse * vertex color */
        fragColor.x = u->matEmission[0] + u->matDiffuse[0] * vColor.x;
        fragColor.y = u->matEmission[1] + u->matDiffuse[1] * vColor.y;
        fragColor.z = u->matEmission[2] + u->matDiffuse[2] * vColor.z;
        fragColor.w = u->matEmission[3] + u->matDiffuse[3] * vColor.w;
    }

    /* Clamp */
    if (fragColor.x < 0.0f) fragColor.x = 0.0f; else if (fragColor.x > 1.0f) fragColor.x = 1.0f;
    if (fragColor.y < 0.0f) fragColor.y = 0.0f; else if (fragColor.y > 1.0f) fragColor.y = 1.0f;
    if (fragColor.z < 0.0f) fragColor.z = 0.0f; else if (fragColor.z > 1.0f) fragColor.z = 1.0f;
    if (fragColor.w < 0.0f) fragColor.w = 0.0f; else if (fragColor.w > 1.0f) fragColor.w = 1.0f;

    builtins->gl_FragColor = fragColor;
}

/* =========================================================================
 * BaseColor shader uniforms
 * ======================================================================= */

struct PGLBaseColorUniforms {
    float mv[16];
    float proj[16];
    int   hasColors;
    int   hasTexCoords;
    float _pad[2];
    float baseColor[4];
};

/* vs_output layout:  0-1=texcoord xy  2-5=color xyzw */
#define PGL_BASECOLOR_VS_OUT 6

static void pgl_basecolor_vs(float* vs_output,
                              vec4*  vertex_attribs,
                              Shader_Builtins* builtins,
                              void*  uniforms_ptr)
{
    const PGLBaseColorUniforms* u = static_cast<const PGLBaseColorUniforms*>(uniforms_ptr);

    vec4 pos4; pos4.x = vertex_attribs[0].x; pos4.y = vertex_attribs[0].y;
               pos4.z = vertex_attribs[0].z; pos4.w = 1.0f;
    vec4 posEye = pgl_rm4_v4(u->mv, pos4);
    builtins->gl_Position = pgl_rm4_v4(u->proj, posEye);

    /* texcoord */
    if (u->hasTexCoords) {
        vs_output[0] = vertex_attribs[2].x;
        vs_output[1] = vertex_attribs[2].y;
    } else {
        vs_output[0] = vs_output[1] = 0.0f;
    }
    /* color */
    if (u->hasColors) {
        vs_output[2] = vertex_attribs[3].x;
        vs_output[3] = vertex_attribs[3].y;
        vs_output[4] = vertex_attribs[3].z;
        vs_output[5] = vertex_attribs[3].w;
    } else {
        vs_output[2] = u->baseColor[0];
        vs_output[3] = u->baseColor[1];
        vs_output[4] = u->baseColor[2];
        vs_output[5] = u->baseColor[3];
    }
}

static void pgl_basecolor_fs(float* fs_input,
                              Shader_Builtins* builtins,
                              void*  uniforms_ptr)
{
    (void)uniforms_ptr;
    vec4 fragColor;
    fragColor.x = fs_input[2]; fragColor.y = fs_input[3];
    fragColor.z = fs_input[4]; fragColor.w = fs_input[5];
    if (fragColor.x < 0.0f) fragColor.x = 0.0f; else if (fragColor.x > 1.0f) fragColor.x = 1.0f;
    if (fragColor.y < 0.0f) fragColor.y = 0.0f; else if (fragColor.y > 1.0f) fragColor.y = 1.0f;
    if (fragColor.z < 0.0f) fragColor.z = 0.0f; else if (fragColor.z > 1.0f) fragColor.z = 1.0f;
    if (fragColor.w < 0.0f) fragColor.w = 0.0f; else if (fragColor.w > 1.0f) fragColor.w = 1.0f;
    builtins->gl_FragColor = fragColor;
}

/* =========================================================================
 * Program creation helpers
 * ======================================================================= */

/* Create portablegl Phong shader program.  Returns the GLuint program handle
 * (as returned by pglCreateProgram), or 0 on failure. */
static inline GLuint pgl_create_phong_program()
{
    static GLenum interp[PGL_PHONG_VS_OUT];
    for (int i = 0; i < PGL_PHONG_VS_OUT; ++i)
        interp[i] = (GLenum)PGL_SMOOTH;
    return pglCreateProgram(pgl_phong_vs, pgl_phong_fs,
                            PGL_PHONG_VS_OUT, interp, GL_FALSE);
}

static inline GLuint pgl_create_basecolor_program()
{
    static GLenum interp[PGL_BASECOLOR_VS_OUT];
    for (int i = 0; i < PGL_BASECOLOR_VS_OUT; ++i)
        interp[i] = (GLenum)PGL_SMOOTH;
    return pglCreateProgram(pgl_basecolor_vs, pgl_basecolor_fs,
                            PGL_BASECOLOR_VS_OUT, interp, GL_FALSE);
}

#endif /* OBOL_PORTABLEGL_BUILD */
#endif /* OBOL_PORTABLEGL_SHADERS_H */
