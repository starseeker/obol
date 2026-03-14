/*
 * portablegl_obol_shaders.h  –  PortableGL C-function shaders for Obol
 *
 * Obol's scene graph renderer uses GLSL 1.10 compatibility-profile shaders
 * that access OpenGL fixed-function state (gl_ModelViewMatrix, gl_LightSource,
 * gl_FrontMaterial, etc.).  PortableGL cannot compile GLSL; instead, shaders
 * are pairs of C functions matching vert_func / frag_func signatures.
 *
 * This header provides pre-written C-function shaders that faithfully
 * reimplement Obol's primary GLSL shader variants, reading from an
 * ObolPGLCompatState passed as the 'void* uniforms' pointer.
 *
 * Shader set
 * ──────────
 *
 *  OBOL_PGL_SHADER_GOURAUD
 *    Vertex: transforms with MVP, computes per-vertex Phong lighting
 *    Fragment: outputs interpolated lit colour
 *    Maps to Obol's default SoShader / SoShadowGroup per-vertex path
 *
 *  OBOL_PGL_SHADER_PHONG   (per-pixel Phong)
 *    Vertex: transforms with MVP, passes eye-space position + normal
 *    Fragment: per-pixel Phong lighting for up to OBOL_PGL_MAX_LIGHTS lights
 *    Maps to Obol's perpixel_vertex/fragment.glsl shader pair
 *
 *  OBOL_PGL_SHADER_TEXTURED_REPLACE
 *    Vertex: transforms with MVP, passes UV
 *    Fragment: texture lookup, no lighting
 *
 *  OBOL_PGL_SHADER_TEXTURED_PHONG  (per-pixel Phong + texture modulate)
 *    Vertex: transforms with MVP, passes eye-space pos + normal + UV
 *    Fragment: per-pixel Phong × diffuse texture
 *
 *  OBOL_PGL_SHADER_FLAT  (BASE_COLOR mode, no lighting)
 *    Vertex/Fragment: transform + output per-vertex colour unchanged
 *
 *  OBOL_PGL_SHADER_DEPTH  (depth-only pass for shadow maps)
 *    Vertex: transform with shadow MVP
 *    Fragment: output depth as greyscale
 *
 * Vertex attribute slots used (matching Obol's VBO layout conventions)
 * ─────────────────────────────────────────────────────────────────────
 *   attrib 0 (PGL_ATTR_VERT)     : vec4 vertex position
 *   attrib 1 (PGL_ATTR_COLOR)    : vec4 per-vertex colour (or diffuse material)
 *   attrib 2 (PGL_ATTR_NORMAL)   : vec3 vertex normal
 *   attrib 3 (PGL_ATTR_TEXCOORD0): vec2 texture coordinate 0
 *
 * Proposed upstream change to PortableGL
 * ──────────────────────────────────────
 * These shaders depend on the ObolPGLCompatState "compat layer" that is
 * proposed as an addition to PortableGL (see portablegl_compat_state.h).
 * Upstream PortableGL already ships pgl_init_std_shaders() with similar
 * shaders (pgl_dflt_light_vs, pgl_pnt_light_diff_vs, pgl_tex_rplc_vs, etc.)
 * using a pgl_uniforms struct.  Obol's shaders extend those with:
 *   • Multi-light Phong (up to OBOL_PGL_MAX_LIGHTS directional/point lights)
 *   • Full gl_FrontMaterial support (ambient, diffuse, specular, emission, shininess)
 *   • coin_light_model / coin_two_sided_lighting flags
 *   • Texture-modulated Phong
 *   • Depth-only pass for shadow map generation
 *   (FBO support for shadow map rendering is tracked separately)
 */

#ifndef OBOL_PORTABLEGL_OBOL_SHADERS_H
#define OBOL_PORTABLEGL_OBOL_SHADERS_H

#include "portablegl_compat_state.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * VS output layout (shared across shaders)
 * ═══════════════════════════════════════════════════════════════════════════
 *  vs_output[0] vec4: eye-space position (xyz/w)   → passed to fragment
 *  vs_output[1] vec4: interpolated colour           → passed to fragment
 *  vs_output[2] vec4: world normal (xyz) + spare w  → passed to fragment
 *  vs_output[3] vec2: texture coordinates           → packed in vec4[0..1]
 *
 * Counts for pglCreateProgram:
 *   Gouraud / Flat / Phong (no tex): 8 floats (2 × vec4)
 *   Phong with texture:              12 floats (3 × vec4)
 *
 * PortableGL interpolation arrays (PGL_SMOOTH) are listed per-float.
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Shared lighting helper
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Compute Phong contribution for one light.
 * normal, eye_dir, ec_pos3 are in eye space. */
static inline void obol_pgl_one_light(const ObolPGLLightState& L,
                                      const float normal[3],
                                      const float ec_pos3[3],
                                      float& out_diff_r, float& out_diff_g, float& out_diff_b,
                                      float& out_spec_r, float& out_spec_g, float& out_spec_b,
                                      float shininess)
{
    /* Light direction in eye space */
    float ldir[3];
    if (L.position[3] == 0.f) {
        /* Directional */
        float len = sqrtf(L.position[0]*L.position[0] +
                          L.position[1]*L.position[1] +
                          L.position[2]*L.position[2]);
        if (len < 1e-7f) len = 1.f;
        ldir[0] = L.position[0]/len;
        ldir[1] = L.position[1]/len;
        ldir[2] = L.position[2]/len;
    } else {
        /* Point */
        ldir[0] = L.position[0] - ec_pos3[0];
        ldir[1] = L.position[1] - ec_pos3[1];
        ldir[2] = L.position[2] - ec_pos3[2];
        float len = sqrtf(ldir[0]*ldir[0]+ldir[1]*ldir[1]+ldir[2]*ldir[2]);
        if (len < 1e-7f) len = 1.f;
        ldir[0]/=len; ldir[1]/=len; ldir[2]/=len;
    }

    float nDotL = normal[0]*ldir[0] + normal[1]*ldir[1] + normal[2]*ldir[2];
    if (nDotL < 0.f) nDotL = 0.f;

    out_diff_r += L.diffuse[0] * nDotL;
    out_diff_g += L.diffuse[1] * nDotL;
    out_diff_b += L.diffuse[2] * nDotL;

    /* Blinn-Phong half-vector */
    float eye[3] = {-ec_pos3[0], -ec_pos3[1], -ec_pos3[2]};
    float elen = sqrtf(eye[0]*eye[0]+eye[1]*eye[1]+eye[2]*eye[2]);
    if (elen > 1e-7f) { eye[0]/=elen; eye[1]/=elen; eye[2]/=elen; }
    float hv[3] = {ldir[0]+eye[0], ldir[1]+eye[1], ldir[2]+eye[2]};
    float hvlen = sqrtf(hv[0]*hv[0]+hv[1]*hv[1]+hv[2]*hv[2]);
    if (hvlen > 1e-7f) { hv[0]/=hvlen; hv[1]/=hvlen; hv[2]/=hvlen; }
    float nDotHV = normal[0]*hv[0] + normal[1]*hv[1] + normal[2]*hv[2];
    if (nDotHV < 0.f) nDotHV = 0.f;
    float pf = (nDotL > 0.f && shininess > 0.f) ? powf(nDotHV, shininess) : 0.f;

    out_spec_r += L.specular[0] * pf;
    out_spec_g += L.specular[1] * pf;
    out_spec_b += L.specular[2] * pf;
}

static inline float obol_pgl_clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OBOL_PGL_SHADER_GOURAUD  –  per-vertex Phong (matches Obol's default VS path)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * vs_output layout (8 floats):
 *   [0..3]  vec4: interpolated lit RGBA colour
 *   [4..7]  vec4: eye-space position (for fog etc, not currently used)
 */
static void obol_gouraud_vs(float* vs_output, pgl_vec4* v_attrs,
                             Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);

    /* Position in clip space */
    float pos4[4] = {v_attrs[PGL_ATTR_VERT].x, v_attrs[PGL_ATTR_VERT].y,
                     v_attrs[PGL_ATTR_VERT].z, v_attrs[PGL_ATTR_VERT].w};
    float cp[4];
    obol_pgl_mv4(cp, s->mvp, pos4);
    builtins->gl_Position = make_v4(cp[0], cp[1], cp[2], cp[3]);

    /* Eye-space position */
    float ep[4];
    obol_pgl_mv4(ep, s->modelview_stack[s->modelview_depth], pos4);
    float ec3[3] = {ep[0]/ep[3], ep[1]/ep[3], ep[2]/ep[3]};

    /* Transform normal */
    float nx = v_attrs[PGL_ATTR_NORMAL].x;
    float ny = v_attrs[PGL_ATTR_NORMAL].y;
    float nz = v_attrs[PGL_ATTR_NORMAL].z;
    float* N = s->normal_mat;
    float tn[3] = {
        N[0]*nx + N[3]*ny + N[6]*nz,
        N[1]*nx + N[4]*ny + N[7]*nz,
        N[2]*nx + N[5]*ny + N[8]*nz
    };
    float nlen = sqrtf(tn[0]*tn[0]+tn[1]*tn[1]+tn[2]*tn[2]);
    if (nlen > 1e-7f) { tn[0]/=nlen; tn[1]/=nlen; tn[2]/=nlen; }

    /* Per-vertex diffuse colour (attrib slot 1 or material diffuse) */
    float cr = v_attrs[PGL_ATTR_COLOR].x;
    float cg = v_attrs[PGL_ATTR_COLOR].y;
    float cb = v_attrs[PGL_ATTR_COLOR].z;
    float ca = v_attrs[PGL_ATTR_COLOR].w;

    /* Scene colour = emission + globalAmbient * material.ambient */
    const ObolPGLMaterialState& mat = s->front_material;
    float sr = mat.emission[0] + s->global_ambient[0]*mat.ambient[0];
    float sg = mat.emission[1] + s->global_ambient[1]*mat.ambient[1];
    float sb = mat.emission[2] + s->global_ambient[2]*mat.ambient[2];

    float acc_diff_r=0, acc_diff_g=0, acc_diff_b=0;
    float acc_spec_r=0, acc_spec_g=0, acc_spec_b=0;

    for (int i = 0; i < OBOL_PGL_MAX_LIGHTS; i++) {
        if (!s->lights[i].enabled) continue;
        obol_pgl_one_light(s->lights[i], tn, ec3,
                           acc_diff_r, acc_diff_g, acc_diff_b,
                           acc_spec_r, acc_spec_g, acc_spec_b,
                           mat.shininess);
        acc_diff_r += s->lights[i].ambient[0] * mat.ambient[0];
        acc_diff_g += s->lights[i].ambient[1] * mat.ambient[1];
        acc_diff_b += s->lights[i].ambient[2] * mat.ambient[2];
    }

    /* Final colour = sceneColour + diffuse*perVertexColor + specular*material.specular */
    float fr = obol_pgl_clamp01(sr + acc_diff_r*cr + acc_spec_r*mat.specular[0]);
    float fg = obol_pgl_clamp01(sg + acc_diff_g*cg + acc_spec_g*mat.specular[1]);
    float fb = obol_pgl_clamp01(sb + acc_diff_b*cb + acc_spec_b*mat.specular[2]);

    ((pgl_vec4*)vs_output)[0] = make_v4(fr, fg, fb, ca);
    /* Eye-space position for fog */
    ((pgl_vec4*)vs_output)[1] = make_v4(ec3[0], ec3[1], ec3[2], 1.f);
}

static void obol_gouraud_fs(float* fs_input, Shader_Builtins* builtins, void* /*uniforms*/)
{
    builtins->gl_FragColor = ((pgl_vec4*)fs_input)[0];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OBOL_PGL_SHADER_PHONG  –  per-pixel Phong (perpixel_vertex/fragment.glsl)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * vs_output layout (12 floats):
 *   [0..3]  vec4: eye-space position xyz + 1 (ecPosition3 + pad)
 *   [4..7]  vec4: interpolated normal xyz + 1
 *   [8..11] vec4: per-vertex colour (gl_Color / gl_FrontColor)
 */
static void obol_phong_vs(float* vs_output, pgl_vec4* v_attrs,
                           Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);

    float pos4[4] = {v_attrs[PGL_ATTR_VERT].x, v_attrs[PGL_ATTR_VERT].y,
                     v_attrs[PGL_ATTR_VERT].z, v_attrs[PGL_ATTR_VERT].w};
    float cp[4];
    obol_pgl_mv4(cp, s->mvp, pos4);
    builtins->gl_Position = make_v4(cp[0],cp[1],cp[2],cp[3]);

    float ep[4];
    obol_pgl_mv4(ep, s->modelview_stack[s->modelview_depth], pos4);
    float w = (ep[3] != 0.f) ? ep[3] : 1.f;

    /* Eye-space position */
    ((pgl_vec4*)vs_output)[0] = make_v4(ep[0]/w, ep[1]/w, ep[2]/w, 1.f);

    /* Transformed normal */
    float nx=v_attrs[PGL_ATTR_NORMAL].x, ny=v_attrs[PGL_ATTR_NORMAL].y, nz=v_attrs[PGL_ATTR_NORMAL].z;
    float* N = s->normal_mat;
    float tnx = N[0]*nx + N[3]*ny + N[6]*nz;
    float tny = N[1]*nx + N[4]*ny + N[7]*nz;
    float tnz = N[2]*nx + N[5]*ny + N[8]*nz;
    float nlen = sqrtf(tnx*tnx+tny*tny+tnz*tnz);
    if (nlen > 1e-7f) { tnx/=nlen; tny/=nlen; tnz/=nlen; }
    ((pgl_vec4*)vs_output)[1] = make_v4(tnx, tny, tnz, 1.f);

    /* Per-vertex colour */
    ((pgl_vec4*)vs_output)[2] = v_attrs[PGL_ATTR_COLOR];
}

static void obol_phong_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    pgl_vec4* inputs = (pgl_vec4*)fs_input;

    float ec3[3]   = {inputs[0].x, inputs[0].y, inputs[0].z};
    float normal[3]= {inputs[1].x, inputs[1].y, inputs[1].z};
    float nlen = sqrtf(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    if (nlen > 1e-7f) { normal[0]/=nlen; normal[1]/=nlen; normal[2]/=nlen; }

    /* Two-sided lighting */
    if (s->two_sided_lighting && !builtins->gl_FrontFacing) {
        normal[0]=-normal[0]; normal[1]=-normal[1]; normal[2]=-normal[2];
    }

    /* BASE_COLOR mode: no lighting */
    if (s->light_model == 0) {
        builtins->gl_FragColor = inputs[2];
        return;
    }

    const ObolPGLMaterialState& mat = s->front_material;
    /* Scene colour */
    float sr = mat.emission[0] + s->global_ambient[0]*mat.ambient[0];
    float sg = mat.emission[1] + s->global_ambient[1]*mat.ambient[1];
    float sb = mat.emission[2] + s->global_ambient[2]*mat.ambient[2];

    float acc_diff_r=0, acc_diff_g=0, acc_diff_b=0;
    float acc_spec_r=0, acc_spec_g=0, acc_spec_b=0;

    for (int i=0; i<OBOL_PGL_MAX_LIGHTS; i++) {
        if (!s->lights[i].enabled) continue;
        obol_pgl_one_light(s->lights[i], normal, ec3,
                           acc_diff_r, acc_diff_g, acc_diff_b,
                           acc_spec_r, acc_spec_g, acc_spec_b,
                           mat.shininess);
        acc_diff_r += s->lights[i].ambient[0] * mat.ambient[0];
        acc_diff_g += s->lights[i].ambient[1] * mat.ambient[1];
        acc_diff_b += s->lights[i].ambient[2] * mat.ambient[2];
    }

    float cr = inputs[2].x, cg = inputs[2].y, cb = inputs[2].z, ca = inputs[2].w;
    float fr = obol_pgl_clamp01(sr + acc_diff_r*cr + acc_spec_r*mat.specular[0]);
    float fg = obol_pgl_clamp01(sg + acc_diff_g*cg + acc_spec_g*mat.specular[1]);
    float fb = obol_pgl_clamp01(sb + acc_diff_b*cb + acc_spec_b*mat.specular[2]);
    builtins->gl_FragColor = make_v4(fr, fg, fb, ca);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OBOL_PGL_SHADER_TEXTURED_REPLACE  –  texture replace, no lighting
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * vs_output layout (8 floats):
 *   [0..3]  vec4: per-vertex colour (used by modulate variant)
 *   [4..7]  vec4: uv.xy padded to vec4 (PGL needs vec4 alignment)
 */
static void obol_tex_replace_vs(float* vs_output, pgl_vec4* v_attrs,
                                  Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    float pos4[4] = {v_attrs[PGL_ATTR_VERT].x, v_attrs[PGL_ATTR_VERT].y,
                     v_attrs[PGL_ATTR_VERT].z, v_attrs[PGL_ATTR_VERT].w};
    float cp[4];
    obol_pgl_mv4(cp, s->mvp, pos4);
    builtins->gl_Position = make_v4(cp[0],cp[1],cp[2],cp[3]);

    ((pgl_vec4*)vs_output)[0] = v_attrs[PGL_ATTR_COLOR];
    float u = v_attrs[PGL_ATTR_TEXCOORD0].x;
    float v_ = v_attrs[PGL_ATTR_TEXCOORD0].y;
    ((pgl_vec4*)vs_output)[1] = make_v4(u, v_, 0.f, 0.f);
}

static void obol_tex_replace_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    pgl_vec4* inputs = (pgl_vec4*)fs_input;
    float u = inputs[1].x, v = inputs[1].y;
    GLuint tex = s->tex_unit[0];
    if (tex) {
        builtins->gl_FragColor = texture2D(tex, u, v);
    } else {
        builtins->gl_FragColor = inputs[0]; /* fallback: vertex colour */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OBOL_PGL_SHADER_TEXTURED_PHONG  –  per-pixel Phong × diffuse texture
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * vs_output layout (12 floats):
 *   [0..3]  vec4: eye-space position
 *   [4..7]  vec4: interpolated normal
 *   [8..11] vec4: uv.xy + vertex colour alpha in z,w
 */
static void obol_tex_phong_vs(float* vs_output, pgl_vec4* v_attrs,
                                Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    float pos4[4] = {v_attrs[PGL_ATTR_VERT].x, v_attrs[PGL_ATTR_VERT].y,
                     v_attrs[PGL_ATTR_VERT].z, v_attrs[PGL_ATTR_VERT].w};
    float cp[4], ep[4];
    obol_pgl_mv4(cp, s->mvp, pos4);
    obol_pgl_mv4(ep, s->modelview_stack[s->modelview_depth], pos4);
    builtins->gl_Position = make_v4(cp[0],cp[1],cp[2],cp[3]);
    float w = (ep[3]!=0.f)?ep[3]:1.f;
    ((pgl_vec4*)vs_output)[0] = make_v4(ep[0]/w, ep[1]/w, ep[2]/w, 1.f);

    float nx=v_attrs[PGL_ATTR_NORMAL].x, ny=v_attrs[PGL_ATTR_NORMAL].y, nz=v_attrs[PGL_ATTR_NORMAL].z;
    float* N = s->normal_mat;
    float tnx=N[0]*nx+N[3]*ny+N[6]*nz;
    float tny=N[1]*nx+N[4]*ny+N[7]*nz;
    float tnz=N[2]*nx+N[5]*ny+N[8]*nz;
    float nlen=sqrtf(tnx*tnx+tny*tny+tnz*tnz);
    if (nlen>1e-7f){tnx/=nlen;tny/=nlen;tnz/=nlen;}
    ((pgl_vec4*)vs_output)[1] = make_v4(tnx, tny, tnz, 1.f);

    float u=v_attrs[PGL_ATTR_TEXCOORD0].x, vv=v_attrs[PGL_ATTR_TEXCOORD0].y;
    float ca = v_attrs[PGL_ATTR_COLOR].w;
    ((pgl_vec4*)vs_output)[2] = make_v4(u, vv, ca, 0.f);
}

static void obol_tex_phong_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    pgl_vec4* inputs = (pgl_vec4*)fs_input;

    float ec3[3]   = {inputs[0].x, inputs[0].y, inputs[0].z};
    float normal[3]= {inputs[1].x, inputs[1].y, inputs[1].z};
    float nlen = sqrtf(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    if (nlen > 1e-7f) { normal[0]/=nlen; normal[1]/=nlen; normal[2]/=nlen; }
    if (s->two_sided_lighting && !builtins->gl_FrontFacing) {
        normal[0]=-normal[0]; normal[1]=-normal[1]; normal[2]=-normal[2];
    }

    float u = inputs[2].x, v = inputs[2].y, ca = inputs[2].z;

    /* Sample texture */
    pgl_vec4 texcol = {1.f,1.f,1.f,1.f};
    if (s->tex_unit[0])
        texcol = texture2D(s->tex_unit[0], u, v);

    const ObolPGLMaterialState& mat = s->front_material;
    float sr = mat.emission[0] + s->global_ambient[0]*mat.ambient[0];
    float sg = mat.emission[1] + s->global_ambient[1]*mat.ambient[1];
    float sb = mat.emission[2] + s->global_ambient[2]*mat.ambient[2];

    float acc_diff_r=0, acc_diff_g=0, acc_diff_b=0;
    float acc_spec_r=0, acc_spec_g=0, acc_spec_b=0;
    for (int i=0;i<OBOL_PGL_MAX_LIGHTS;i++) {
        if (!s->lights[i].enabled) continue;
        obol_pgl_one_light(s->lights[i], normal, ec3,
                           acc_diff_r, acc_diff_g, acc_diff_b,
                           acc_spec_r, acc_spec_g, acc_spec_b,
                           mat.shininess);
        acc_diff_r += s->lights[i].ambient[0]*mat.ambient[0];
        acc_diff_g += s->lights[i].ambient[1]*mat.ambient[1];
        acc_diff_b += s->lights[i].ambient[2]*mat.ambient[2];
    }

    /* Modulate diffuse by texture colour */
    float fr = obol_pgl_clamp01(sr + acc_diff_r*texcol.x + acc_spec_r*mat.specular[0]);
    float fg = obol_pgl_clamp01(sg + acc_diff_g*texcol.y + acc_spec_g*mat.specular[1]);
    float fb = obol_pgl_clamp01(sb + acc_diff_b*texcol.z + acc_spec_b*mat.specular[2]);
    builtins->gl_FragColor = make_v4(fr, fg, fb, ca);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OBOL_PGL_SHADER_FLAT  –  BASE_COLOR mode (no lighting, vertex colour only)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void obol_flat_vs(float* vs_output, pgl_vec4* v_attrs,
                          Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    float pos4[4]={v_attrs[PGL_ATTR_VERT].x,v_attrs[PGL_ATTR_VERT].y,
                   v_attrs[PGL_ATTR_VERT].z,v_attrs[PGL_ATTR_VERT].w};
    float cp[4];
    obol_pgl_mv4(cp, s->mvp, pos4);
    builtins->gl_Position = make_v4(cp[0],cp[1],cp[2],cp[3]);
    ((pgl_vec4*)vs_output)[0] = v_attrs[PGL_ATTR_COLOR];
    ((pgl_vec4*)vs_output)[1] = make_v4(0,0,0,0); /* eye-space pos unused */
}

static void obol_flat_fs(float* fs_input, Shader_Builtins* builtins, void* /*uniforms*/)
{
    builtins->gl_FragColor = ((pgl_vec4*)fs_input)[0];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OBOL_PGL_SHADER_DEPTH  –  depth-only pass (shadow map generation)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void obol_depth_vs(float* /*vs_output*/, pgl_vec4* v_attrs,
                           Shader_Builtins* builtins, void* uniforms)
{
    ObolPGLCompatState* s = OBOL_PGL_STATE(uniforms);
    float pos4[4]={v_attrs[PGL_ATTR_VERT].x,v_attrs[PGL_ATTR_VERT].y,
                   v_attrs[PGL_ATTR_VERT].z,v_attrs[PGL_ATTR_VERT].w};
    float cp[4];
    /* Use shadow matrix from first shadow light */
    obol_pgl_mv4(cp, s->shadows[0].shadow_matrix, pos4);
    builtins->gl_Position = make_v4(cp[0],cp[1],cp[2],cp[3]);
}

static void obol_depth_fs(float* /*fs_input*/, Shader_Builtins* builtins, void* /*uniforms*/)
{
    /* Write normalised depth as greyscale; the actual depth is in gl_FragDepth */
    float d = builtins->gl_FragCoord.z;
    builtins->gl_FragColor = make_v4(d, d, d, 1.f);
    builtins->gl_FragDepth = d;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shader programme descriptors (for registration in the shader registry)
 * ═══════════════════════════════════════════════════════════════════════════ */

enum ObolPGLShaderKind {
    OBOL_PGL_SHADER_GOURAUD          = 0,
    OBOL_PGL_SHADER_PHONG            = 1,
    OBOL_PGL_SHADER_TEXTURED_REPLACE = 2,
    OBOL_PGL_SHADER_TEXTURED_PHONG   = 3,
    OBOL_PGL_SHADER_FLAT             = 4,
    OBOL_PGL_SHADER_DEPTH            = 5,
    OBOL_PGL_NUM_SHADERS             = 6
};

struct ObolPGLShaderDesc {
    vert_func  vs;
    frag_func  fs;
    GLsizei    n_interp;           /* number of floats in vs_output */
    GLenum     interp[12];         /* interpolation array (PGL_SMOOTH / PGL_FLAT) */
    GLboolean  fragdepth_or_discard;
};

static const ObolPGLShaderDesc s_obol_shader_descs[OBOL_PGL_NUM_SHADERS] = {
    /* GOURAUD:  8 floats = 2×vec4 (colour + eye-pos) */
    { obol_gouraud_vs, obol_gouraud_fs,       8,
      {PGL_SMOOTH4, PGL_SMOOTH4},             GL_FALSE },
    /* PHONG:   12 floats = 3×vec4 (eye-pos + normal + colour) */
    { obol_phong_vs,   obol_phong_fs,          12,
      {PGL_SMOOTH4, PGL_SMOOTH4, PGL_SMOOTH4},GL_FALSE },
    /* TEX_REPLACE: 8 floats = 2×vec4 (colour + uv) */
    { obol_tex_replace_vs, obol_tex_replace_fs, 8,
      {PGL_SMOOTH4, PGL_SMOOTH4},             GL_FALSE },
    /* TEX_PHONG: 12 floats = 3×vec4 (eye-pos + normal + uv/alpha) */
    { obol_tex_phong_vs, obol_tex_phong_fs,    12,
      {PGL_SMOOTH4, PGL_SMOOTH4, PGL_SMOOTH4},GL_FALSE },
    /* FLAT:    8 floats = 2×vec4 (colour + dummy eye-pos) */
    { obol_flat_vs,    obol_flat_fs,            8,
      {PGL_SMOOTH4, PGL_FLAT4},               GL_FALSE },
    /* DEPTH:   0 floats (no interpolated outputs) */
    { obol_depth_vs,   obol_depth_fs,           0,
      {},                                      GL_TRUE  },
};

/* Create a PortableGL program for the given kind.
 * Returns the PortableGL program handle (GLuint), or 0 on failure. */
static inline GLuint obol_pgl_create_shader(ObolPGLShaderKind kind)
{
    if (kind < 0 || kind >= OBOL_PGL_NUM_SHADERS) return 0;
    const ObolPGLShaderDesc& d = s_obol_shader_descs[kind];
    GLenum interp_buf[12];
    memcpy(interp_buf, d.interp, d.n_interp * sizeof(GLenum));
    return pglCreateProgram(d.vs, d.fs, d.n_interp, interp_buf, d.fragdepth_or_discard);
}

#endif /* OBOL_PORTABLEGL_OBOL_SHADERS_H */
