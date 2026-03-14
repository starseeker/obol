/*
 * portablegl_shader_registry.cpp
 *
 * GLSL shader registry and ARB shader interceptors for the PortableGL backend.
 *
 * When Obol calls the ARB shader object API (glCreateShaderObjectARB,
 * glShaderSourceARB, glLinkProgramARB, …) our interceptors:
 *  1. Store the GLSL source per shader handle
 *  2. At link time, classify the GLSL and pick the appropriate pre-written
 *     PortableGL C-function shader pair from portablegl_obol_shaders.h
 *  3. Create the PortableGL C program via pglCreateProgram
 *  4. At glUseProgramObjectARB wire the current ObolPGLCompatState via pglSetUniform
 *  5. Return magic integers from glGetUniformLocationARB for known coin_* uniforms
 *  6. Update ObolPGLCompatState on each glUniform*ARB call
 *
 * Proposed upstream changes to PortableGL
 * ────────────────────────────────────────
 * The stub glUniform*() functions in PortableGL should be replaced with a
 * per-program uniform-value store that C shaders can query.  The simplest
 * upstream approach would be to add a fixed-size union array to glProgram:
 *
 *   typedef union { GLfloat f; GLint i; } pgl_uniform_val;
 *   pgl_uniform_val uniforms[PGL_MAX_UNIFORMS];   // indexed by location
 *
 * and implement glGetUniformLocation / glUniform* to populate it.  C shaders
 * would call pglGetUniformf(location) / pglGetUniformi(location) to read values.
 *
 * For Obol specifically the ObolPGLCompatState approach is more practical
 * because it models the GLSL compat-profile built-in state (gl_LightSource[],
 * gl_ModelViewMatrix, …) rather than named GLSL uniforms.
 */

#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#include <portablegl/portablegl.h>

#include "portablegl_compat_state.h"
#include "portablegl_obol_shaders.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <mutex>

/* Thread-local current compat state – shared with portablegl_compat_funcs.cpp
 * and SoDBPortableGL.cpp via extern declaration.                              */
extern thread_local ObolPGLCompatState* g_cur_compat;

/* ═══════════════════════════════════════════════════════════════════════════
 * Fake handle ranges (must not overlap PortableGL's real program handles)
 * ═══════════════════════════════════════════════════════════════════════════ */

static constexpr GLuint FAKE_SHADER_BASE  = 0x10000000u;
static constexpr GLuint FAKE_PROGRAM_BASE = 0x20000000u;

/* ═══════════════════════════════════════════════════════════════════════════
 * Shader / program tables
 * ═══════════════════════════════════════════════════════════════════════════ */

struct FakeShaderObj {
    GLenum            type;
    std::string       source;
    bool              compiled    = false;
    bool              is_fragment = false;
};

struct FakeProgramObj {
    std::vector<GLuint> attached_shaders;
    bool                linked     = false;
    GLuint              pgl_handle = 0;    /* real PortableGL program handle */
    ObolPGLShaderKind   kind       = OBOL_PGL_SHADER_FLAT;
};

static std::mutex                                s_reg_mutex;
static std::unordered_map<GLuint, FakeShaderObj>  s_shaders;
static std::unordered_map<GLuint, FakeProgramObj> s_programs;
static GLuint s_next_shader_id  = FAKE_SHADER_BASE  + 1;
static GLuint s_next_program_id = FAKE_PROGRAM_BASE + 1;

/* ── Classifier ──────────────────────────────────────────────────────────── */

static bool src_contains(const std::string& src, const char* kw) {
    return src.find(kw) != std::string::npos;
}

static ObolPGLShaderKind classify_glsl(const std::string& vs, const std::string& fs) {
    if (src_contains(vs,"shadowMatrix") || src_contains(vs,"shadowMap") ||
        src_contains(fs,"shadowMap"))
        return OBOL_PGL_SHADER_DEPTH;

    bool has_tex   = src_contains(vs,"gl_MultiTexCoord") ||
                     src_contains(fs,"texture2D")        ||
                     src_contains(fs,"textureMap");
    bool has_light = src_contains(vs,"gl_LightSource")   ||
                     src_contains(fs,"gl_LightSource")   ||
                     src_contains(vs,"fragmentNormal")   ||
                     src_contains(fs,"fragmentNormal");
    bool per_pixel = src_contains(vs,"ecPosition")       ||
                     src_contains(fs,"ecPosition");

    if (has_tex && has_light) return OBOL_PGL_SHADER_TEXTURED_PHONG;
    if (has_tex)              return OBOL_PGL_SHADER_TEXTURED_REPLACE;
    if (has_light && per_pixel) return OBOL_PGL_SHADER_PHONG;
    if (has_light)            return OBOL_PGL_SHADER_GOURAUD;
    return OBOL_PGL_SHADER_FLAT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ARB Shader object interceptors
 * ═══════════════════════════════════════════════════════════════════════════ */

extern "C" {

GLuint pgl_igl_CreateShaderObjectARB(GLenum type) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    GLuint id = s_next_shader_id++;
    FakeShaderObj obj;
    obj.type        = type;
    obj.is_fragment = (type == GL_FRAGMENT_SHADER_ARB || type == 0x8B30u);
    s_shaders[id]   = std::move(obj);
    return id;
}

void pgl_igl_ShaderSourceARB(GLuint obj, GLsizei count,
                               const GLchar** strs, const GLint* lens) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto it = s_shaders.find(obj);
    if (it == s_shaders.end()) return;
    it->second.source.clear();
    for (GLsizei i = 0; i < count; i++) {
        if (!strs || !strs[i]) continue;
        if (lens && lens[i] >= 0)
            it->second.source.append(strs[i], (size_t)lens[i]);
        else
            it->second.source += strs[i];
    }
}

void pgl_igl_CompileShaderARB(GLuint obj) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto it = s_shaders.find(obj);
    if (it != s_shaders.end()) it->second.compiled = true;
}

void pgl_igl_GetObjectParameterivARB(GLuint obj, GLenum pname, GLint* params) {
    if (!params) return;
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    { auto it = s_shaders.find(obj);
      if (it != s_shaders.end()) {
        *params = (pname == GL_OBJECT_COMPILE_STATUS_ARB) ? GL_TRUE : 0;
        return; } }
    { auto it = s_programs.find(obj);
      if (it != s_programs.end()) {
        *params = (pname == GL_OBJECT_LINK_STATUS_ARB)
                  ? (it->second.linked ? GL_TRUE : GL_FALSE) : 0;
        return; } }
    *params = 0;
}

void pgl_igl_GetInfoLogARB(GLuint, GLsizei max, GLsizei* len, GLchar* log) {
    if (len) *len = 0;
    if (log && max > 0) log[0] = '\0';
}

void pgl_igl_DeleteObjectARB(GLuint obj) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    s_shaders.erase(obj);
    s_programs.erase(obj);
}

GLuint pgl_igl_CreateProgramObjectARB() {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    GLuint id = s_next_program_id++;
    s_programs[id] = FakeProgramObj{};
    return id;
}

void pgl_igl_AttachObjectARB(GLuint prog, GLuint sh) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto it = s_programs.find(prog);
    if (it != s_programs.end())
        it->second.attached_shaders.push_back(sh);
}

void pgl_igl_DetachObjectARB(GLuint prog, GLuint sh) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto it = s_programs.find(prog);
    if (it == s_programs.end()) return;
    auto& v = it->second.attached_shaders;
    v.erase(std::remove(v.begin(), v.end(), sh), v.end());
}

void pgl_igl_LinkProgramARB(GLuint prog) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto pit = s_programs.find(prog);
    if (pit == s_programs.end()) return;

    std::string vs_src, fs_src;
    for (GLuint sh_id : pit->second.attached_shaders) {
        auto sit = s_shaders.find(sh_id);
        if (sit == s_shaders.end()) continue;
        (sit->second.is_fragment ? fs_src : vs_src) += sit->second.source;
    }

    ObolPGLShaderKind kind = classify_glsl(vs_src, fs_src);
    GLuint pgl_prog = obol_pgl_create_shader(kind);

    pit->second.kind       = kind;
    pit->second.pgl_handle = pgl_prog;
    pit->second.linked     = (pgl_prog != 0);
}

GLboolean pgl_igl_IsProgram(GLuint prog) {
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto it = s_programs.find(prog);
    return (it != s_programs.end() && it->second.linked) ? GL_TRUE : GL_FALSE;
}

void pgl_igl_UseProgramObjectARB(GLuint prog) {
    if (prog == 0) { glUseProgram(0); return; }
    std::lock_guard<std::mutex> lk(s_reg_mutex);
    auto it = s_programs.find(prog);
    if (it == s_programs.end() || it->second.pgl_handle == 0) {
        glUseProgram(0); return;
    }
    glUseProgram(it->second.pgl_handle);
    if (g_cur_compat) pglSetUniform(g_cur_compat);
}

/* ── Uniform location dispatch ───────────────────────────────────────────── */

GLint pgl_igl_GetUniformLocationARB(GLuint /*prog*/, const GLchar* name) {
    if (!name) return UPGL_INVALID;
    if (!strcmp(name,"coin_light_model"))       return UPGL_LIGHT_MODEL;
    if (!strcmp(name,"coin_two_sided_lighting")) return UPGL_TWO_SIDED;
    if (!strcmp(name,"coin_texunit0_model"))    return UPGL_TEXUNIT0_MODEL;
    if (!strcmp(name,"coin_texunit1_model"))    return UPGL_TEXUNIT1_MODEL;
    if (!strcmp(name,"coin_texunit2_model"))    return UPGL_TEXUNIT2_MODEL;
    if (!strcmp(name,"coin_texunit3_model"))    return UPGL_TEXUNIT3_MODEL;
    if (!strcmp(name,"textureMap0"))            return UPGL_TEXMAP0;
    if (!strcmp(name,"textureMap1"))            return UPGL_TEXMAP1;
    if (!strcmp(name,"cameraTransform"))        return UPGL_CAMERA_TRANSFORM;
    if (strncmp(name,"shadowMap",9)==0     && name[9]>='0' && name[9]<='7')
        return (GLint)(UPGL_SHADOW_MAP_BASE    + (name[9]-'0'));
    if (strncmp(name,"shadowMatrix",12)==0 && name[12]>='0' && name[12]<='7')
        return (GLint)(UPGL_SHADOW_MATRIX_BASE + (name[12]-'0'));
    if (strncmp(name,"farval",6)==0        && name[6]>='0' && name[6]<='7')
        return (GLint)(UPGL_SHADOW_FARVAL_BASE + (name[6]-'0'));
    if (strncmp(name,"nearval",7)==0       && name[7]>='0' && name[7]<='7')
        return (GLint)(UPGL_SHADOW_NEARVAL_BASE+ (name[7]-'0'));
    return UPGL_INVALID;
}

void pgl_igl_GetActiveUniformARB(GLuint, GLuint, GLsizei, GLsizei* l, GLint* sz, GLenum* t, GLchar* n) {
    if (l)  *l = 0; if (sz) *sz = 0;
    if (t)  *t = GL_FLOAT; if (n) n[0] = '\0';
}

/* ── Uniform setters ─────────────────────────────────────────────────────── */

void pgl_igl_Uniform1iARB(GLint loc, GLint v0) {
    if (!g_cur_compat || loc == UPGL_INVALID) return;
    switch ((ObolPGLUniformLocation)loc) {
    case UPGL_LIGHT_MODEL:    g_cur_compat->light_model=v0; break;
    case UPGL_TWO_SIDED:      g_cur_compat->two_sided_lighting=v0; break;
    case UPGL_TEXUNIT0_MODEL: g_cur_compat->texunit_model[0]=v0; break;
    case UPGL_TEXUNIT1_MODEL: g_cur_compat->texunit_model[1]=v0; break;
    case UPGL_TEXUNIT2_MODEL: g_cur_compat->texunit_model[2]=v0; break;
    case UPGL_TEXUNIT3_MODEL: g_cur_compat->texunit_model[3]=v0; break;
    case UPGL_TEXMAP0:        g_cur_compat->tex_unit[0]=(GLuint)v0; break;
    case UPGL_TEXMAP1:        g_cur_compat->tex_unit[1]=(GLuint)v0; break;
    default:
        { int i=loc-UPGL_SHADOW_MAP_BASE;
          if (i>=0&&i<OBOL_PGL_MAX_LIGHTS) { g_cur_compat->shadows[i].shadow_map=(GLuint)v0; } }
        break;
    }
}

void pgl_igl_Uniform1fARB(GLint loc, GLfloat v0) {
    if (!g_cur_compat) return;
    int i = loc - (int)UPGL_SHADOW_FARVAL_BASE;
    if (i>=0 && i<OBOL_PGL_MAX_LIGHTS) { g_cur_compat->shadows[i].farval  = v0; return; }
    i = loc - (int)UPGL_SHADOW_NEARVAL_BASE;
    if (i>=0 && i<OBOL_PGL_MAX_LIGHTS) { g_cur_compat->shadows[i].nearval = v0; return; }
}

void pgl_igl_UniformMatrix4fvARB(GLint loc, GLsizei, GLboolean, const GLfloat* m) {
    if (!g_cur_compat || !m) return;
    int i = loc - (int)UPGL_SHADOW_MATRIX_BASE;
    if (i>=0 && i<OBOL_PGL_MAX_LIGHTS) { memcpy(g_cur_compat->shadows[i].shadow_matrix, m, 64); return; }
    if (loc == UPGL_CAMERA_TRANSFORM)   { memcpy(g_cur_compat->camera_transform, m, 64); }
}

/* Stubs for uniforms not routed through ObolPGLCompatState */
void pgl_igl_Uniform2fARB (GLint,GLfloat,GLfloat)                    {}
void pgl_igl_Uniform3fARB (GLint,GLfloat,GLfloat,GLfloat)            {}
void pgl_igl_Uniform4fARB (GLint,GLfloat,GLfloat,GLfloat,GLfloat)    {}
void pgl_igl_Uniform2iARB (GLint,GLint,GLint)                        {}
void pgl_igl_Uniform3iARB (GLint,GLint,GLint,GLint)                  {}
void pgl_igl_Uniform4iARB (GLint,GLint,GLint,GLint,GLint)            {}
void pgl_igl_Uniform1fvARB(GLint,GLsizei,const GLfloat*)             {}
void pgl_igl_Uniform2fvARB(GLint,GLsizei,const GLfloat*)             {}
void pgl_igl_Uniform3fvARB(GLint,GLsizei,const GLfloat*)             {}
void pgl_igl_Uniform4fvARB(GLint,GLsizei,const GLfloat*)             {}
void pgl_igl_Uniform1ivARB(GLint,GLsizei,const GLint*)               {}
void pgl_igl_Uniform2ivARB(GLint,GLsizei,const GLint*)               {}
void pgl_igl_Uniform3ivARB(GLint,GLsizei,const GLint*)               {}
void pgl_igl_Uniform4ivARB(GLint,GLsizei,const GLint*)               {}
void pgl_igl_UniformMatrix2fvARB(GLint,GLsizei,GLboolean,const GLfloat*){}
void pgl_igl_UniformMatrix3fvARB(GLint,GLsizei,GLboolean,const GLfloat*){}

} /* extern "C" */
