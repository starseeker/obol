/*
 * portablegl_compat_funcs.cpp
 *
 * OpenGL compatibility-profile interceptors for the PortableGL backend.
 *
 * These implement the fixed-function GL calls that Obol's renderer makes
 * (glMatrixMode, glLightfv, glMaterialfv, glBegin/glEnd, etc.) by updating
 * the current context's ObolPGLCompatState rather than calling into PortableGL
 * (which has no fixed-function support).
 *
 * The thread-local g_cur_compat pointer is set by makeContextCurrent() in
 * SoDBPortableGL.cpp; all interceptors access it.
 *
 * Proposed upstream contributions to PortableGL
 * ──────────────────────────────────────────────
 *  1. Matrix stack (glMatrixMode / glLoadMatrixf / glPushMatrix / glPopMatrix /
 *     glTranslatef / glScalef / glRotatef / glOrtho / glFrustum)
 *     → add to glContext struct: modelview/projection/texture stacks
 *
 *  2. Light state (glLightfv / glLightModelfv)
 *     → add gl_light_source_parameters array to glContext
 *
 *  3. Material state (glMaterialfv / glColorMaterial)
 *     → add gl_material_parameters to glContext
 *
 *  4. Immediate mode (glBegin/glEnd / glVertex3f / glNormal3f / glColor4f /
 *     glTexCoord2f)
 *     → buffer vertices internally, emit glDrawArrays in glEnd
 *
 *  All four additions map cleanly onto PortableGL's existing architecture
 *  (the shader C-functions would read these from the context struct, or from
 *  a new 'compat_state' field added to glContext).
 */

#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#define PGL_PREFIX_GL    1
#include <portablegl/portablegl.h>

#include "portablegl_compat_state.h"
#include "portablegl_obol_shaders.h"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <unordered_set>

/* Current compat state – set by SoDBPortableGL.cpp via makeContextCurrent. */
thread_local ObolPGLCompatState* g_cur_compat = nullptr;

/* Lazily-created default portablegl program (Gouraud shading), bound
 * automatically by pgl_igl_End() when no explicit shader has been bound via
 * pgl_igl_UseProgramObjectARB.  These are per-thread but must be invalidated
 * when a new portablegl context becomes current (its programs table is
 * separate from the previous context's).  Call pgl_igl_reset_context_caches()
 * from makeCurrent() to reset them.                                           */
static thread_local GLuint s_default_gouraud_prog = 0;
static thread_local GLuint s_default_phong_prog = 0;
static thread_local GLuint s_default_flat_prog = 0;
static thread_local GLuint s_default_tex_replace_prog = 0;
static thread_local GLuint s_default_tex_phong_prog = 0;

/* Also reset the immediate-mode VBO state so the new context gets its own
 * VAO/VBO handles (the old ones are gone after a context switch).            */
static thread_local bool   s_imm_init = false;
static thread_local GLuint s_imm_vao  = 0;
static thread_local GLuint s_imm_vbo  = 0;

/* Forward declaration – defined in portablegl_shader_registry.cpp.
 * Clears all cached portablegl program handles in the global ARB shader
 * registry so they are re-created in the new context on next link.          */
extern "C" void pgl_igl_reset_shader_registry_handles();

extern "C" {

/* Called from CoinPortableGLContextData::makeCurrent() whenever the active
 * portablegl context changes.  Resets all caches that hold portablegl object
 * handles (program IDs, VAO/VBO IDs) because those handles belong to the
 * previous context's object namespace and are invalid in the new context.    */
void pgl_igl_reset_context_caches() {
    s_default_gouraud_prog    = 0;
    s_default_phong_prog      = 0;
    s_default_flat_prog       = 0;
    s_default_tex_replace_prog = 0;
    s_default_tex_phong_prog  = 0;
    s_imm_init = false;
    s_imm_vao  = 0;
    s_imm_vbo  = 0;
    /* Also reset ARB shader registry handles – they reference programs in the
     * old portablegl context and must be re-created in the new one.          */
    pgl_igl_reset_shader_registry_handles();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Matrix stack
 * ═══════════════════════════════════════════════════════════════════════════ */

void pgl_igl_MatrixMode(GLenum mode) {
    if (g_cur_compat) g_cur_compat->matrix_mode_set(mode);
}
void pgl_igl_LoadIdentity() {
    if (g_cur_compat) g_cur_compat->load_identity();
}
void pgl_igl_LoadMatrixf(const GLfloat* m) {
    if (g_cur_compat && m) g_cur_compat->load_matrixf(m);
}
void pgl_igl_LoadMatrixd(const GLdouble* m) {
    if (!g_cur_compat || !m) return;
    float mf[16]; for(int i=0;i<16;i++) mf[i]=(float)m[i];
    g_cur_compat->load_matrixf(mf);
}
void pgl_igl_MultMatrixf(const GLfloat* m) {
    if (g_cur_compat && m) g_cur_compat->mult_matrixf(m);
}
void pgl_igl_PushMatrix() {
    if (g_cur_compat) g_cur_compat->push_matrix();
}
void pgl_igl_PopMatrix() {
    if (g_cur_compat) g_cur_compat->pop_matrix();
}
void pgl_igl_Translatef(GLfloat x, GLfloat y, GLfloat z) {
    if (!g_cur_compat) return;
    float m[16]; obol_pgl_identity(m);
    m[12]=x; m[13]=y; m[14]=z;
    g_cur_compat->mult_matrixf(m);
}
void pgl_igl_Scalef(GLfloat x, GLfloat y, GLfloat z) {
    if (!g_cur_compat) return;
    float m[16]; obol_pgl_identity(m);
    m[0]=x; m[5]=y; m[10]=z;
    g_cur_compat->mult_matrixf(m);
}
void pgl_igl_Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    if (!g_cur_compat) return;
    float rad = angle * (3.14159265f/180.f);
    float c = cosf(rad), s = sinf(rad), t = 1.f-c;
    float len = sqrtf(x*x+y*y+z*z);
    if (len>1e-7f){x/=len;y/=len;z/=len;}
    float m[16] = {
        t*x*x+c,   t*x*y+s*z, t*x*z-s*y, 0,
        t*x*y-s*z, t*y*y+c,   t*y*z+s*x, 0,
        t*x*z+s*y, t*y*z-s*x, t*z*z+c,   0,
        0,          0,          0,          1
    };
    g_cur_compat->mult_matrixf(m);
}
void pgl_igl_Ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
                    GLdouble n, GLdouble f) {
    if (!g_cur_compat) return;
    float rl=(float)(r-l), tb=(float)(t-b), fn=(float)(f-n);
    if (rl==0||tb==0||fn==0) return;
    float m[16]={};
    m[0]=2.f/rl; m[5]=2.f/tb; m[10]=-2.f/fn;
    m[12]=-(float)(r+l)/rl; m[13]=-(float)(t+b)/tb; m[14]=-(float)(f+n)/fn; m[15]=1.f;
    g_cur_compat->mult_matrixf(m);
}
void pgl_igl_Frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
                      GLdouble n, GLdouble f) {
    if (!g_cur_compat) return;
    float rl=(float)(r-l), tb=(float)(t-b), fn=(float)(f-n);
    float m[16]={};
    m[0]=(float)(2*n)/rl; m[5]=(float)(2*n)/tb;
    m[8]=(float)(r+l)/rl; m[9]=(float)(t+b)/tb;
    m[10]=-(float)(f+n)/fn; m[11]=-1.f;
    m[14]=-(float)(2*f*n)/fn;
    g_cur_compat->mult_matrixf(m);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Light and material state
 * ═══════════════════════════════════════════════════════════════════════════ */

void pgl_igl_Lightfv(GLenum light, GLenum pname, const GLfloat* p) {
    if (!g_cur_compat || !p) return;
    int i = (int)(light - GL_LIGHT0);
    if (i<0 || i>=OBOL_PGL_MAX_LIGHTS) return;
    ObolPGLLightState& L = g_cur_compat->lights[i];
    switch (pname) {
    case GL_AMBIENT:              memcpy(L.ambient,   p,16); break;
    case GL_DIFFUSE:              memcpy(L.diffuse,   p,16); break;
    case GL_SPECULAR:             memcpy(L.specular,  p,16); break;
    case GL_POSITION:
        /* Transform to eye space */
        if (g_cur_compat->modelview_depth >= 0)
            obol_pgl_mv4(L.position, g_cur_compat->modelview_stack[g_cur_compat->modelview_depth], p);
        else
            memcpy(L.position, p, 16);
        if (p[3]==0.f) { /* directional – compute half-vector */
            float n=sqrtf(L.position[0]*L.position[0]+L.position[1]*L.position[1]+L.position[2]*L.position[2]);
            if (n>1e-7f) { L.half_vector[0]=L.position[0]/n; L.half_vector[1]=L.position[1]/n; L.half_vector[2]=(L.position[2]+1.f)/n; L.half_vector[3]=0.f; }
        }
        break;
    case GL_SPOT_DIRECTION:       memcpy(L.spot_direction, p, 12); break;
    case GL_SPOT_EXPONENT:        L.spot_exponent=p[0]; break;
    case GL_SPOT_CUTOFF:          L.spot_cutoff=p[0]; L.spot_cos_cutoff=cosf(p[0]*3.14159265f/180.f); break;
    case GL_CONSTANT_ATTENUATION: L.attenuation[0]=p[0]; break;
    case GL_LINEAR_ATTENUATION:   L.attenuation[1]=p[0]; break;
    case GL_QUADRATIC_ATTENUATION:L.attenuation[2]=p[0]; break;
    }
}
void pgl_igl_Lightf(GLenum light, GLenum pname, GLfloat param) {
    pgl_igl_Lightfv(light, pname, &param);
}
void pgl_igl_Lighti(GLenum light, GLenum pname, GLint param) {
    float f=(float)param; pgl_igl_Lightfv(light, pname, &f);
}
void pgl_igl_LightModelfv(GLenum pname, const GLfloat* p) {
    if (!g_cur_compat || !p) return;
    if (pname==GL_LIGHT_MODEL_AMBIENT) memcpy(g_cur_compat->global_ambient, p, 16);
}
void pgl_igl_LightModeli(GLenum pname, GLint param) {
    if (!g_cur_compat) return;
    if (pname==0x0B52u /*GL_LIGHT_MODEL_TWO_SIDE*/) g_cur_compat->two_sided_lighting=param;
}
void pgl_igl_Materialfv(GLenum face, GLenum pname, const GLfloat* p) {
    if (!g_cur_compat || !p || face==GL_BACK) return;
    ObolPGLMaterialState& m = g_cur_compat->front_material;
    switch (pname) {
    case GL_AMBIENT:             memcpy(m.ambient, p,16); break;
    case GL_DIFFUSE:             memcpy(m.diffuse, p,16); break;
    case GL_SPECULAR:            memcpy(m.specular,p,16); break;
    case GL_EMISSION:            memcpy(m.emission,p,16); break;
    case GL_SHININESS:           m.shininess=p[0]; break;
    case GL_AMBIENT_AND_DIFFUSE: memcpy(m.ambient,p,16); memcpy(m.diffuse,p,16); break;
    }
}
void pgl_igl_Materialf(GLenum face, GLenum pname, GLfloat param) {
    pgl_igl_Materialfv(face, pname, &param);
}
void pgl_igl_ColorMaterial(GLenum /*face*/, GLenum /*mode*/) { /* intentional no-op */ }
void pgl_igl_GetMaterialfv(GLenum face, GLenum pname, GLfloat* p) {
    if (!g_cur_compat || !p || face==GL_BACK) return;
    ObolPGLMaterialState& m = g_cur_compat->front_material;
    switch (pname) {
    case GL_AMBIENT:   memcpy(p,m.ambient, 16); break;
    case GL_DIFFUSE:   memcpy(p,m.diffuse, 16); break;
    case GL_SPECULAR:  memcpy(p,m.specular,16); break;
    case GL_EMISSION:  memcpy(p,m.emission,16); break;
    case GL_SHININESS: p[0]=m.shininess; break;
    }
}

/* glEnable/glDisable with light intercept */
void pgl_igl_Enable(GLenum cap) {
    if (g_cur_compat) {
        if (cap>=GL_LIGHT0 && cap<=GL_LIGHT0+OBOL_PGL_MAX_LIGHTS-1) {
            g_cur_compat->lights[cap-GL_LIGHT0].enabled=true;
            return;
        }
        if (cap==GL_FOG) { g_cur_compat->fog_enabled=true; return; }
        /* GL_LIGHTING enable/disable maps to light_model PHONG/BASE_COLOR */
        if (cap==GL_LIGHTING) { g_cur_compat->light_model=1; return; }
    }
    /* Only pass caps that PortableGL's glEnable() actually supports;
     * silently drop GL 1.x fixed-function caps (GL_LIGHTING, GL_COLOR_MATERIAL,
     * GL_NORMALIZE, etc.) that would cause GL_INVALID_ENUM.              */
    switch (cap) {
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DEPTH_CLAMP:
    case GL_LINE_SMOOTH:
    case GL_BLEND:
    case GL_COLOR_LOGIC_OP:
    case GL_POLYGON_OFFSET_POINT:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
    case GL_DEBUG_OUTPUT:
        glEnable(cap);
        break;
    default:
        break; /* silently ignore unsupported fixed-function caps */
    }
}
void pgl_igl_Disable(GLenum cap) {
    if (g_cur_compat) {
        if (cap>=GL_LIGHT0 && cap<=GL_LIGHT0+OBOL_PGL_MAX_LIGHTS-1) {
            g_cur_compat->lights[cap-GL_LIGHT0].enabled=false;
            return;
        }
        if (cap==GL_FOG) { g_cur_compat->fog_enabled=false; return; }
        /* GL_LIGHTING enable/disable maps to light_model PHONG/BASE_COLOR */
        if (cap==GL_LIGHTING) { g_cur_compat->light_model=0; return; }
    }
    switch (cap) {
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DEPTH_CLAMP:
    case GL_LINE_SMOOTH:
    case GL_BLEND:
    case GL_COLOR_LOGIC_OP:
    case GL_POLYGON_OFFSET_POINT:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
    case GL_DEBUG_OUTPUT:
        glDisable(cap);
        break;
    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Draw-call interceptors: convert legacy primitive modes to GL 3.x modes
 * PortableGL supports modes 0 (GL_POINTS) through 6 (GL_TRIANGLE_FAN).
 * GL_QUADS (7), GL_QUAD_STRIP (8), and GL_POLYGON (9) must be mapped.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Map legacy mode to the nearest supported PortableGL mode. */
static GLenum pgl_map_draw_mode(GLenum mode) {
    if (mode == GL_POLYGON)    return GL_TRIANGLE_FAN;
    if (mode == GL_QUAD_STRIP) return GL_TRIANGLE_STRIP;
    return mode;  /* GL_QUADS needs index remapping (done below), others pass through */
}

void pgl_igl_DrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (mode == GL_QUADS && count >= 4) {
        /* Each quad (v0,v1,v2,v3) → two triangles (v0,v1,v2) + (v0,v2,v3). */
        std::vector<GLuint> idx;
        idx.reserve((size_t)count / 4 * 6);
        for (GLint q = 0; q + 3 < count; q += 4) {
            GLuint b = (GLuint)(first + q);
            idx.push_back(b);   idx.push_back(b+1); idx.push_back(b+2);
            idx.push_back(b);   idx.push_back(b+2); idx.push_back(b+3);
        }
        GLuint ebo; glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     (GLsizei)(idx.size() * sizeof(GLuint)),
                     idx.data(), GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &ebo);
        return;
    }
    glDrawArrays(pgl_map_draw_mode(mode), first, count);
}

void pgl_igl_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    if (mode == GL_QUADS) {
        /* Convert quad indices: each group of 4 indices → 6 triangle indices. */
        /* For now, fall back to GL_TRIANGLES assuming the caller tessellated. */
        glDrawElements(GL_TRIANGLES, count, type, indices);
        return;
    }
    glDrawElements(pgl_map_draw_mode(mode), count, type, indices);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Fog state interceptors
 * ═══════════════════════════════════════════════════════════════════════════ */

void pgl_igl_Fogf(GLenum pname, GLfloat param) {
    if (!g_cur_compat) return;
    switch (pname) {
    case GL_FOG_DENSITY: g_cur_compat->fog_density = param; break;
    case GL_FOG_START:   g_cur_compat->fog_start   = param; break;
    case GL_FOG_END:     g_cur_compat->fog_end      = param; break;
    default: break;
    }
}
void pgl_igl_Fogi(GLenum pname, GLint param) {
    if (!g_cur_compat) return;
    if (pname == GL_FOG_MODE) g_cur_compat->fog_mode = (GLenum)param;
    else                      pgl_igl_Fogf(pname, (GLfloat)param);
}
void pgl_igl_Fogfv(GLenum pname, const GLfloat* p) {
    if (!g_cur_compat || !p) return;
    if (pname == GL_FOG_COLOR) memcpy(g_cur_compat->fog_color, p, 4*sizeof(GLfloat));
    else                       pgl_igl_Fogf(pname, p[0]);
}
void pgl_igl_Fogiv(GLenum pname, const GLint* p) {
    if (!g_cur_compat || !p) return;
    if (pname == GL_FOG_COLOR) {
        /* Integer fog colour is in [0..255] per channel */
        g_cur_compat->fog_color[0] = p[0]/255.f;
        g_cur_compat->fog_color[1] = p[1]/255.f;
        g_cur_compat->fog_color[2] = p[2]/255.f;
        g_cur_compat->fog_color[3] = p[3]/255.f;
    } else {
        pgl_igl_Fogi(pname, p[0]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Immediate mode
 * ═══════════════════════════════════════════════════════════════════════════ */

struct ImmVertex {
    float pos[4];  /* PGL_ATTR_VERT     */
    float col[4];  /* PGL_ATTR_COLOR    */
    float norm[3]; /* PGL_ATTR_NORMAL   */
    float tc[2];   /* PGL_ATTR_TEXCOORD0*/
};

static GLenum              s_imm_prim = GL_TRIANGLES;
static bool                s_in_begin = false;
static std::vector<ImmVertex> s_imm_verts;
/* s_imm_vao, s_imm_vbo, s_imm_init are declared as thread_local above
 * so they can be reset by pgl_igl_reset_context_caches() on context switch. */

static void imm_ensure_vbo() {
    if (s_imm_init) return;
    glGenVertexArrays(1,&s_imm_vao);
    glGenBuffers(1,&s_imm_vbo);
    glBindVertexArray(s_imm_vao);
    glBindBuffer(GL_ARRAY_BUFFER,s_imm_vbo);
    size_t stride = sizeof(ImmVertex);
    glVertexAttribPointer(PGL_ATTR_VERT,      4,GL_FLOAT,GL_FALSE,(GLsizei)stride,(void*)offsetof(ImmVertex,pos));
    glVertexAttribPointer(PGL_ATTR_COLOR,     4,GL_FLOAT,GL_FALSE,(GLsizei)stride,(void*)offsetof(ImmVertex,col));
    glVertexAttribPointer(PGL_ATTR_NORMAL,    3,GL_FLOAT,GL_FALSE,(GLsizei)stride,(void*)offsetof(ImmVertex,norm));
    glVertexAttribPointer(PGL_ATTR_TEXCOORD0, 2,GL_FLOAT,GL_FALSE,(GLsizei)stride,(void*)offsetof(ImmVertex,tc));
    glEnableVertexAttribArray(PGL_ATTR_VERT);
    glEnableVertexAttribArray(PGL_ATTR_COLOR);
    glEnableVertexAttribArray(PGL_ATTR_NORMAL);
    glEnableVertexAttribArray(PGL_ATTR_TEXCOORD0);
    glBindVertexArray(0);
    s_imm_init = true;
}

/* Ensure the default per-shader-kind program is created (lazy, one per
 * portablegl context) and return its handle. */
static GLuint imm_default_prog_for_kind(ObolPGLShaderKind kind) {
    GLuint* slot = nullptr;
    switch (kind) {
    case OBOL_PGL_SHADER_GOURAUD:          slot = &s_default_gouraud_prog;     break;
    case OBOL_PGL_SHADER_PHONG:            slot = &s_default_phong_prog;        break;
    case OBOL_PGL_SHADER_FLAT:             slot = &s_default_flat_prog;         break;
    case OBOL_PGL_SHADER_TEXTURED_REPLACE: slot = &s_default_tex_replace_prog;  break;
    case OBOL_PGL_SHADER_TEXTURED_PHONG:   slot = &s_default_tex_phong_prog;    break;
    default: return 0;
    }
    if (*slot == 0) *slot = obol_pgl_create_shader(kind);
    return *slot;
}

/* Choose the best default shader for the current compat state. */
static GLuint imm_choose_default_prog() {
    if (!g_cur_compat) return imm_default_prog_for_kind(OBOL_PGL_SHADER_GOURAUD);
    /* Determine if we have textures and lighting based on compat state */
    bool has_tex = (g_cur_compat->texunit_model[0] != 0);
    bool has_light = false;
    for (int i = 0; i < OBOL_PGL_MAX_LIGHTS; ++i) {
        if (g_cur_compat->lights[i].enabled) { has_light = true; break; }
    }
    if (has_tex && has_light) return imm_default_prog_for_kind(OBOL_PGL_SHADER_TEXTURED_PHONG);
    if (has_tex)              return imm_default_prog_for_kind(OBOL_PGL_SHADER_TEXTURED_REPLACE);
    /* Default: Gouraud (per-vertex) lighting */
    return imm_default_prog_for_kind(OBOL_PGL_SHADER_GOURAUD);
}

void pgl_igl_Begin(GLenum prim) {
    s_imm_prim=prim; s_in_begin=true; s_imm_verts.clear();
}
void pgl_igl_End() {
    if (!s_in_begin || s_imm_verts.empty()) { s_in_begin=false; return; }
    s_in_begin=false;

    imm_ensure_vbo();

    /* Ensure a PortableGL shader is bound.  Obol's built-in renderer uses the
     * GL 1.x fixed-function pipeline; we choose the best-matching C-function
     * shader based on current compat state and bind it for this draw call. */
    GLint cur_prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur_prog);
    bool auto_bound = false;
    if (cur_prog == 0) {
        GLuint dflt = imm_choose_default_prog();
        if (dflt != 0) {
            glUseProgram(dflt);
            if (g_cur_compat) pglSetUniform(g_cur_compat);
            auto_bound = true;
        }
    } else {
        /* Refresh uniforms in case matrix/material state changed. */
        if (g_cur_compat) pglSetUniform(g_cur_compat);
    }

    glBindVertexArray(s_imm_vao);
    glBindBuffer(GL_ARRAY_BUFFER,s_imm_vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizei)(s_imm_verts.size()*sizeof(ImmVertex)),
                 s_imm_verts.data(),GL_STREAM_DRAW);
    if (s_imm_prim==GL_QUADS && s_imm_verts.size()>=4) {
        std::vector<GLuint> idx;
        for (size_t q=0;q+3<s_imm_verts.size();q+=4) {
            idx.push_back((GLuint)q);   idx.push_back((GLuint)q+1); idx.push_back((GLuint)q+2);
            idx.push_back((GLuint)q);   idx.push_back((GLuint)q+2); idx.push_back((GLuint)q+3);
        }
        GLuint ebo; glGenBuffers(1,&ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizei)(idx.size()*sizeof(GLuint)),idx.data(),GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES,(GLsizei)idx.size(),GL_UNSIGNED_INT,nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        glDeleteBuffers(1,&ebo);
    } else {
        /* GL_POLYGON → GL_TRIANGLE_FAN; GL_QUAD_STRIP → GL_TRIANGLE_STRIP.
         * Both are convex/equivalent for the simple cases Obol generates.  */
        GLenum draw_mode = s_imm_prim;
        if (draw_mode == GL_POLYGON)    draw_mode = GL_TRIANGLE_FAN;
        if (draw_mode == GL_QUAD_STRIP) draw_mode = GL_TRIANGLE_STRIP;
        glDrawArrays(draw_mode, 0, (GLsizei)s_imm_verts.size());
    }
    glBindVertexArray(0);
    /* Restore unbound state so explicit glUseProgramObjectARB calls in the
     * same frame don't see our auto-bound shader as the current one. */
    if (auto_bound) glUseProgram(0);
}

/* Helpers to emit a vertex with inherited current attribs */
static void imm_push_vertex(float x, float y, float z) {
    ImmVertex v = {};
    if (g_cur_compat) {
        memcpy(v.col,  g_cur_compat->current_color,   16);
        memcpy(v.norm, g_cur_compat->current_normal,   12);
        memcpy(v.tc,   g_cur_compat->current_texcoord,  8);
    }
    v.pos[0]=x; v.pos[1]=y; v.pos[2]=z; v.pos[3]=1.f;
    s_imm_verts.push_back(v);
}

void pgl_igl_Vertex2f (GLfloat x, GLfloat y)          { imm_push_vertex(x,y,0.f); }
void pgl_igl_Vertex2s (GLshort x, GLshort y)           { imm_push_vertex((float)x,(float)y,0.f); }
void pgl_igl_Vertex3f (GLfloat x, GLfloat y, GLfloat z){ imm_push_vertex(x,y,z); }
void pgl_igl_Vertex3fv(const GLfloat* v)               { if(v) imm_push_vertex(v[0],v[1],v[2]); }
void pgl_igl_Vertex4fv(const GLfloat* v)               { if(v) imm_push_vertex(v[0],v[1],v[2]); }
void pgl_igl_Normal3f (GLfloat x, GLfloat y, GLfloat z) {
    if (g_cur_compat) { g_cur_compat->current_normal[0]=x; g_cur_compat->current_normal[1]=y; g_cur_compat->current_normal[2]=z; }
    if (!s_imm_verts.empty()) { s_imm_verts.back().norm[0]=x; s_imm_verts.back().norm[1]=y; s_imm_verts.back().norm[2]=z; }
}
void pgl_igl_Normal3fv(const GLfloat* v) { if(v) pgl_igl_Normal3f(v[0],v[1],v[2]); }

static void set_color(float r,float g,float b,float a) {
    if (!g_cur_compat) return;
    g_cur_compat->current_color[0]=r; g_cur_compat->current_color[1]=g;
    g_cur_compat->current_color[2]=b; g_cur_compat->current_color[3]=a;
    /* GL_COLOR_MATERIAL: glColor* also updates the current material diffuse.
     * Obol uses sendPackedDiffuse (glColor4ub) to pass per-draw-call colour;
     * updating front_material.diffuse lets the BASE_COLOR shader see it.   */
    g_cur_compat->front_material.diffuse[0]=r;
    g_cur_compat->front_material.diffuse[1]=g;
    g_cur_compat->front_material.diffuse[2]=b;
    g_cur_compat->front_material.diffuse[3]=a;
}
void pgl_igl_Color3f  (GLfloat r, GLfloat g, GLfloat b)                   { set_color(r,g,b,1.f); }
void pgl_igl_Color3fv (const GLfloat* v)                                   { if(v) set_color(v[0],v[1],v[2],1.f); }
void pgl_igl_Color4f  (GLfloat r, GLfloat g, GLfloat b, GLfloat a)        { set_color(r,g,b,a); }
void pgl_igl_Color4fv (const GLfloat* v)                                   { if(v) set_color(v[0],v[1],v[2],v[3]); }
void pgl_igl_Color3ub (GLubyte r, GLubyte g, GLubyte b)                    { set_color(r/255.f,g/255.f,b/255.f,1.f); }
void pgl_igl_Color3ubv(const GLubyte* v)                                   { if(v) set_color(v[0]/255.f,v[1]/255.f,v[2]/255.f,1.f); }
void pgl_igl_Color4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a)        { set_color(r/255.f,g/255.f,b/255.f,a/255.f); }
void pgl_igl_Color4ubv(const GLubyte* v)                                   { if(v) set_color(v[0]/255.f,v[1]/255.f,v[2]/255.f,v[3]/255.f); }
void pgl_igl_TexCoord2f (GLfloat s_, GLfloat t_) {
    if (g_cur_compat) { g_cur_compat->current_texcoord[0]=s_; g_cur_compat->current_texcoord[1]=t_; }
    if (!s_imm_verts.empty()) { s_imm_verts.back().tc[0]=s_; s_imm_verts.back().tc[1]=t_; }
}
void pgl_igl_TexCoord2fv(const GLfloat* v)              { if(v) pgl_igl_TexCoord2f(v[0],v[1]); }
void pgl_igl_TexCoord3f (GLfloat s_,GLfloat t_,GLfloat) { pgl_igl_TexCoord2f(s_,t_); }
void pgl_igl_TexCoord3fv(const GLfloat* v)              { if(v) pgl_igl_TexCoord2f(v[0],v[1]); }
void pgl_igl_TexCoord4fv(const GLfloat* v)              { if(v) pgl_igl_TexCoord2f(v[0],v[1]); }

/* ═══════════════════════════════════════════════════════════════════════════
 * glPixelStorei  –  track GL_PACK_ROW_LENGTH for ReadPixels
 * ═══════════════════════════════════════════════════════════════════════════ */

static thread_local GLint g_pack_row_length = 0;   /* 0 = tightly packed */

void pgl_igl_PixelStorei(GLenum pname, GLint param) {
    if (pname == GL_PACK_ROW_LENGTH) {
        g_pack_row_length = (param >= 0) ? param : 0;
    } else {
        glPixelStorei(pname, param);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * glReadPixels  (not implemented in PortableGL)
 * ═══════════════════════════════════════════════════════════════════════════ */

void pgl_igl_ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, GLvoid* pixels)
{
    if (!pixels) return;

    /* PortableGL's back buffer is always RGBA8. Map to requested format. */
    const pix_t* src = static_cast<const pix_t*>(pglGetBackBuffer());
    if (!src) {
        /* No current context or back buffer not yet allocated. */
        const size_t bpp = (format == GL_RGBA) ? 4 : 3;
        memset(pixels, 0, (size_t)width * height * bpp);
        return;
    }

    GLsizei dst_stride = (g_pack_row_length > 0) ? g_pack_row_length : width;
    unsigned char* dst = static_cast<unsigned char*>(pixels);

    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
        /* Direct RGBA → RGBA copy with vertical flip. */
        for (GLsizei row = 0; row < height; ++row) {
            GLint src_row = y + (height - 1 - row);
            const unsigned char* sptr = reinterpret_cast<const unsigned char*>(
                src + src_row * width) + x * 4;
            memcpy(dst + (size_t)row * dst_stride * 4, sptr, (size_t)width * 4);
        }
    } else if (format == GL_RGB && type == GL_UNSIGNED_BYTE) {
        /* RGBA → RGB conversion with vertical flip. */
        for (GLsizei row = 0; row < height; ++row) {
            GLint src_row = y + (height - 1 - row);
            const unsigned char* sptr = reinterpret_cast<const unsigned char*>(
                src + src_row * width) + x * 4;
            unsigned char* dptr = dst + (size_t)row * dst_stride * 3;
            for (GLsizei col = 0; col < width; ++col) {
                dptr[0] = sptr[0];
                dptr[1] = sptr[1];
                dptr[2] = sptr[2];
                sptr += 4;
                dptr += 3;
            }
        }
    } else {
        /* Unsupported format/type combination: zero-fill. */
        const size_t bpp = (format == GL_RGBA) ? 4 : 3;
        memset(pixels, 0, (size_t)width * height * bpp);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FBO (render-to-texture via pglSetTexBackBuffer)
 *
 * Design:
 *   • GL_COLOR_ATTACHMENT0: redirect PGL back buffer to the color texture
 *     using pglSetTexBackBuffer().
 *   • GL_DEPTH_ATTACHMENT  with a texture: redirect PGL back buffer to
 *     the depth texture.  OBOL_PGL_SHADER_DEPTH writes depth-as-greyscale
 *     RGBA, so the depth texture's pixels hold the depth in their R channel.
 *   • GL_DEPTH_ATTACHMENT  with a renderbuffer: tracked for FBO-completeness
 *     reporting only; PortableGL uses its own internal depth buffer during
 *     rasterisation, which is sufficient.
 *   • When binding FBO 0 (default), the PGL back buffer is restored to the
 *     per-context default allocation saved when the context was made current.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Saved default back-buffer pointer/dimensions for the currently-bound PGL
 * context.  Updated by pgl_fbo_register_context() on each makeCurrent().  */
static thread_local pix_t*  g_default_backbuf_ptr = nullptr;
static thread_local GLsizei g_default_backbuf_w   = 0;
static thread_local GLsizei g_default_backbuf_h   = 0;

/* Called from SoDBPortableGL.cpp PGLContextData::makeCurrent() so we know
 * the default (non-FBO) back-buffer pointer and dimensions for this context. */
void pgl_fbo_register_context(int w, int h) {
    g_default_backbuf_ptr = static_cast<pix_t*>(pglGetBackBuffer());
    g_default_backbuf_w   = (GLsizei)w;
    g_default_backbuf_h   = (GLsizei)h;
}

struct FakeFBO {
    GLuint color_tex  = 0;   /* GL_COLOR_ATTACHMENT0 texture (if any) */
    GLuint depth_tex  = 0;   /* GL_DEPTH_ATTACHMENT  texture (redirected as RGBA back buf) */
    GLuint depth_rbo  = 0;   /* GL_DEPTH_ATTACHMENT  renderbuffer (for completeness only) */
};

static GLuint s_next_fbo = 1;
static std::unordered_map<GLuint,FakeFBO> s_fbos;
static GLuint s_cur_fbo  = 0;

/* Renderbuffer object bookkeeping (depth storage only; not sampled later). */
static GLuint s_next_rbo = 1;
static std::unordered_set<GLuint> s_rbos;
static GLuint s_cur_rbo  = 0;

static bool fbo_is_complete(const FakeFBO& f) {
    return f.color_tex || f.depth_tex || f.depth_rbo;
}

static void fbo_apply_backbuf(const FakeFBO& f) {
    if (f.color_tex) {
        pglSetTexBackBuffer(f.color_tex);
    } else if (f.depth_tex) {
        /* Depth-only FBO: OBOL_PGL_SHADER_DEPTH writes depth-as-greyscale
         * RGBA into the depth texture.  pglSetTexBackBuffer redirects the
         * colour output there.  PortableGL still maintains its own internal
         * depth buffer for rasterisation correctness.                       */
        pglSetTexBackBuffer(f.depth_tex);
    }
    /* If depth_rbo only: keep default back buffer; renderbuffer not sampled. */
}

void pgl_igl_GenFramebuffers(GLsizei n, GLuint* ids) {
    if (!ids) return;
    for (GLsizei i=0; i<n; i++) { ids[i]=s_next_fbo++; s_fbos[ids[i]]=FakeFBO{}; }
}
void pgl_igl_BindFramebuffer(GLenum /*tgt*/, GLuint fbo) {
    s_cur_fbo = fbo;
    if (fbo == 0) {
        /* Restore the default context back buffer. */
        if (g_default_backbuf_ptr)
            pglSetBackBuffer(g_default_backbuf_ptr, g_default_backbuf_w, g_default_backbuf_h);
    } else {
        auto it = s_fbos.find(fbo);
        if (it != s_fbos.end()) fbo_apply_backbuf(it->second);
    }
}
void pgl_igl_DeleteFramebuffers(GLsizei n, const GLuint* ids) {
    if (!ids) return;
    for (GLsizei i=0; i<n; i++) {
        if (s_cur_fbo == ids[i]) { s_cur_fbo = 0; }
        s_fbos.erase(ids[i]);
    }
}
void pgl_igl_FramebufferTexture2D(GLenum /*tgt*/, GLenum att,
                                   GLenum /*texTgt*/, GLuint tex, GLint /*lvl*/) {
    if (!s_cur_fbo) return;
    auto it = s_fbos.find(s_cur_fbo);
    if (it == s_fbos.end()) return;
    FakeFBO& f = it->second;
    if (att == GL_COLOR_ATTACHMENT0 || att == GL_COLOR_ATTACHMENT0_EXT) {
        f.color_tex = tex;
        if (tex) pglSetTexBackBuffer(tex);
    } else if (att == GL_DEPTH_ATTACHMENT || att == GL_DEPTH_ATTACHMENT_EXT) {
        f.depth_tex = tex;
        /* If this FBO has no colour attachment, redirect back buffer to the
         * depth texture so OBOL_PGL_SHADER_DEPTH fills it with depth data.  */
        if (tex && !f.color_tex) pglSetTexBackBuffer(tex);
    }
}
void pgl_igl_FramebufferRenderbuffer(GLenum /*tgt*/, GLenum att,
                                      GLenum /*rboTgt*/, GLuint rbo) {
    if (!s_cur_fbo) return;
    auto it = s_fbos.find(s_cur_fbo);
    if (it == s_fbos.end()) return;
    if (att == GL_DEPTH_ATTACHMENT || att == GL_DEPTH_ATTACHMENT_EXT ||
        att == GL_STENCIL_ATTACHMENT || att == GL_STENCIL_ATTACHMENT_EXT)
        it->second.depth_rbo = rbo;
}
GLenum pgl_igl_CheckFramebufferStatus(GLenum /*tgt*/) {
    if (!s_cur_fbo) return GL_FRAMEBUFFER_COMPLETE;
    auto it = s_fbos.find(s_cur_fbo);
    if (it == s_fbos.end()) return GL_FRAMEBUFFER_UNDEFINED;
    return fbo_is_complete(it->second) ? GL_FRAMEBUFFER_COMPLETE
                                       : GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
}
GLboolean pgl_igl_IsFramebuffer(GLuint fbo) {
    return s_fbos.count(fbo) ? GL_TRUE : GL_FALSE;
}

/* ── Renderbuffer objects ─────────────────────────────────────────────────── */

void pgl_igl_GenRenderbuffers(GLsizei n, GLuint* ids) {
    if (!ids) return;
    for (GLsizei i=0; i<n; i++) { ids[i]=s_next_rbo++; s_rbos.insert(ids[i]); }
}
void pgl_igl_BindRenderbuffer(GLenum /*tgt*/, GLuint rbo) {
    s_cur_rbo = rbo;
}
void pgl_igl_DeleteRenderbuffers(GLsizei n, const GLuint* ids) {
    if (!ids) return;
    for (GLsizei i=0; i<n; i++) {
        if (s_cur_rbo == ids[i]) s_cur_rbo = 0;
        s_rbos.erase(ids[i]);
    }
}
void pgl_igl_RenderbufferStorage(GLenum /*tgt*/, GLenum /*fmt*/,
                                   GLsizei /*w*/, GLsizei /*h*/) {
    /* PortableGL manages its own depth buffer; this is a no-op.
     * The depth_rbo attachment is tracked for FBO-completeness only.  */
}
GLboolean pgl_igl_IsRenderbuffer(GLuint rbo) {
    return s_rbos.count(rbo) ? GL_TRUE : GL_FALSE;
}

} /* extern "C" */
