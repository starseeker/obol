/*
 * SoGLModernState.cpp  --  Implementation of SoGLModernState
 *
 * See SoGLModernState.h for the full description.
 */

#include "rendering/SoGLModernState.h"
#include "rendering/obol_modern_shaders.h"
#ifdef OBOL_PORTABLEGL_BUILD
#  include "rendering/obol_portablegl_shaders.h"
#endif

#include <Inventor/misc/SoContextHandler.h>
#include <Inventor/SbMatrix.h>

#include <cstring>
#include <cstdio>
#include <cmath>

/* =========================================================================
 * Static registry
 * ===================================================================== */

std::unordered_map<uint32_t, SoGLModernState*> SoGLModernState::instances;
std::mutex                                      SoGLModernState::instances_mutex;

/* =========================================================================
 * Light default constructor
 * ===================================================================== */

SoGLModernState::Light::Light()
{
    position[0] = 0.0f; position[1] = 0.0f; position[2] = 1.0f; position[3] = 0.0f;
    ambient[0]  = 0.0f; ambient[1]  = 0.0f; ambient[2]  = 0.0f; ambient[3]  = 1.0f;
    diffuse[0]  = 1.0f; diffuse[1]  = 1.0f; diffuse[2]  = 1.0f; diffuse[3]  = 1.0f;
    specular[0] = 1.0f; specular[1] = 1.0f; specular[2] = 1.0f; specular[3] = 1.0f;
    spotDirection[0] = 0.0f; spotDirection[1] = 0.0f; spotDirection[2] = -1.0f;
    spotCutoff           = -1.0f; /* non-spot */
    spotExponent         = 0.0f;
    constantAttenuation  = 1.0f;
    linearAttenuation    = 0.0f;
    quadraticAttenuation = 0.0f;
}

/* =========================================================================
 * Constructor / destructor
 * ===================================================================== */

SoGLModernState::SoGLModernState(uint32_t contextId)
    : context_id(contextId)
    , phong_prog(0)
    , basecolor_prog(0)
    /* Phong locations -- initialised to -1 (not found) */
    , p_uModelView(-1), p_uProjection(-1), p_uNormalMatrix(-1)
    , p_uHasNormals(-1), p_uHasColors(-1), p_uHasTexCoords(-1)
    , p_uNumLights(-1)
    , p_uMatAmbient(-1), p_uMatDiffuse(-1), p_uMatSpecular(-1)
    , p_uMatEmission(-1), p_uMatShininess(-1), p_uTwoSided(-1)
    , p_uTexture(-1), p_uHasTexture(-1)
    /* BaseColor locations */
    , bc_uModelView(-1), bc_uProjection(-1)
    , bc_uHasColors(-1), bc_uHasTexCoords(-1)
    , bc_uBaseColor(-1), bc_uTexture(-1), bc_uHasTexture(-1)
    /* material defaults */
    , mat_shininess(0.2f), mat_twoSided(false)
    , num_lights(0)
{
    /* Identity model-view and projection */
    static const float identity16[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    static const float identity9[9] = {
        1,0,0, 0,1,0, 0,0,1
    };
    std::memcpy(mv_gl,   identity16, sizeof(mv_gl));
    std::memcpy(proj_gl, identity16, sizeof(proj_gl));
    std::memcpy(nm_gl,   identity9,  sizeof(nm_gl));

    /* White material */
    for (int i = 0; i < 4; ++i) {
        mat_ambient[i]  = 0.2f;
        mat_diffuse[i]  = 0.8f;
        mat_specular[i] = 0.0f;
        mat_emission[i] = 0.0f;
    }
    mat_ambient[3] = mat_diffuse[3] = mat_specular[3] = mat_emission[3] = 1.0f;

    for (int k = 0; k < OBOL_MODERN_MAX_LIGHTS; ++k) {
        p_uLights_position[k]      = -1;
        p_uLights_ambient[k]       = -1;
        p_uLights_diffuse[k]       = -1;
        p_uLights_specular[k]      = -1;
        p_uLights_spotDirection[k] = -1;
        p_uLights_spotCutoff[k]    = -1;
        p_uLights_spotExponent[k]  = -1;
        p_uLights_constantAtten[k] = -1;
        p_uLights_linearAtten[k]   = -1;
        p_uLights_quadraticAtten[k]= -1;
    }

    initShaders();
    SoContextHandler::addContextDestructionCallback(contextDestructionCB, this);
}

SoGLModernState::~SoGLModernState()
{
    SoContextHandler::removeContextDestructionCallback(contextDestructionCB, this);
    if (phong_prog)     { glDeleteProgram(phong_prog);     phong_prog = 0; }
    if (basecolor_prog) { glDeleteProgram(basecolor_prog); basecolor_prog = 0; }
}

/* =========================================================================
 * Static factory: forContext()
 * ===================================================================== */

SoGLModernState *
SoGLModernState::forContext(uint32_t contextId)
{
    std::lock_guard<std::mutex> lk(instances_mutex);
    auto it = instances.find(contextId);
    if (it != instances.end())
        return it->second;

    /* Create a new instance.  The GL context must be current. */
    SoGLModernState * ms = new SoGLModernState(contextId);
    if (!ms->isAvailable()) {
        delete ms;
        return nullptr;
    }
    instances[contextId] = ms;
    return ms;
}

/* =========================================================================
 * Context destruction callback
 * ===================================================================== */

void
SoGLModernState::contextDestructionCB(uint32_t contextId, void * userdata)
{
    SoGLModernState * ms = static_cast<SoGLModernState *>(userdata);
    std::lock_guard<std::mutex> lk(instances_mutex);
    instances.erase(contextId);
    delete ms;
}

/* =========================================================================
 * Shader compilation
 * ===================================================================== */

static GLuint
sogl_compile_shader_program_local(const char * vert_src, const char * frag_src)
{
    auto compile_shader = [](GLenum type, const char * src) -> GLuint {
        GLuint sh = glCreateShader(type);
        if (!sh) return 0;
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            std::fprintf(stderr,
                         "SoGLModernState: shader compile error:\n%s\n", log);
            glDeleteShader(sh);
            return 0u;
        }
        return sh;
    };

    GLuint vs = compile_shader(GL_VERTEX_SHADER,   vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0u;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr,
                     "SoGLModernState: shader link error:\n%s\n", log);
        glDeleteProgram(prog);
        return 0u;
    }
    return prog;
}

void
SoGLModernState::initShaders()
{
#ifdef OBOL_PORTABLEGL_BUILD
    /* PortableGL does not support GLSL; use C-function-based shaders instead. */
    phong_prog     = pgl_create_phong_program();
    basecolor_prog = pgl_create_basecolor_program();

    if (!phong_prog || !basecolor_prog) {
        if (phong_prog)     { glDeleteProgram(phong_prog);     phong_prog = 0; }
        if (basecolor_prog) { glDeleteProgram(basecolor_prog); basecolor_prog = 0; }
        return;
    }
    /* Uniform locations are unused in portablegl (pglSetUniform is used instead).
     * Leave all location fields at their default -1. */
#else
    phong_prog     = sogl_compile_shader_program_local(
                         OBOL_SHADER_PHONG_VERT,
                         OBOL_SHADER_PHONG_FRAG);
    basecolor_prog = sogl_compile_shader_program_local(
                         OBOL_SHADER_BASECOLOR_VERT,
                         OBOL_SHADER_BASECOLOR_FRAG);

    if (!phong_prog || !basecolor_prog) {
        /* Partial failure -- clean up and leave isAvailable() = false */
        if (phong_prog)     { glDeleteProgram(phong_prog);     phong_prog = 0; }
        if (basecolor_prog) { glDeleteProgram(basecolor_prog); basecolor_prog = 0; }
        return;
    }

    cachePhongLocations();
    cacheBaseColorLocations();
#endif
}

/* =========================================================================
 * Uniform location caching
 * ===================================================================== */

void
SoGLModernState::cachePhongLocations()
{
    GLuint p = phong_prog;
    p_uModelView    = glGetUniformLocation(p, "uModelView");
    p_uProjection   = glGetUniformLocation(p, "uProjection");
    p_uNormalMatrix = glGetUniformLocation(p, "uNormalMatrix");
    p_uHasNormals   = glGetUniformLocation(p, "uHasNormals");
    p_uHasColors    = glGetUniformLocation(p, "uHasColors");
    p_uHasTexCoords = glGetUniformLocation(p, "uHasTexCoords");
    p_uNumLights    = glGetUniformLocation(p, "uNumLights");
    p_uMatAmbient   = glGetUniformLocation(p, "uMatAmbient");
    p_uMatDiffuse   = glGetUniformLocation(p, "uMatDiffuse");
    p_uMatSpecular  = glGetUniformLocation(p, "uMatSpecular");
    p_uMatEmission  = glGetUniformLocation(p, "uMatEmission");
    p_uMatShininess = glGetUniformLocation(p, "uMatShininess");
    p_uTwoSided     = glGetUniformLocation(p, "uTwoSided");
    p_uTexture      = glGetUniformLocation(p, "uTexture");
    p_uHasTexture   = glGetUniformLocation(p, "uHasTexture");

    char buf[64];
    for (int i = 0; i < OBOL_MODERN_MAX_LIGHTS; ++i) {
        std::snprintf(buf, sizeof(buf), "uLights[%d].position",              i);
        p_uLights_position[i]      = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].ambient",               i);
        p_uLights_ambient[i]       = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].diffuse",               i);
        p_uLights_diffuse[i]       = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].specular",              i);
        p_uLights_specular[i]      = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].spotDirection",         i);
        p_uLights_spotDirection[i] = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].spotCutoff",            i);
        p_uLights_spotCutoff[i]    = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].spotExponent",          i);
        p_uLights_spotExponent[i]  = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].constantAttenuation",   i);
        p_uLights_constantAtten[i] = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].linearAttenuation",     i);
        p_uLights_linearAtten[i]   = glGetUniformLocation(p, buf);
        std::snprintf(buf, sizeof(buf), "uLights[%d].quadraticAttenuation",  i);
        p_uLights_quadraticAtten[i]= glGetUniformLocation(p, buf);
    }
}

void
SoGLModernState::cacheBaseColorLocations()
{
    GLuint p = basecolor_prog;
    bc_uModelView    = glGetUniformLocation(p, "uModelView");
    bc_uProjection   = glGetUniformLocation(p, "uProjection");
    bc_uHasColors    = glGetUniformLocation(p, "uHasColors");
    bc_uHasTexCoords = glGetUniformLocation(p, "uHasTexCoords");
    bc_uBaseColor    = glGetUniformLocation(p, "uBaseColor");
    bc_uTexture      = glGetUniformLocation(p, "uTexture");
    bc_uHasTexture   = glGetUniformLocation(p, "uHasTexture");
}

/* =========================================================================
 * State setters
 * ===================================================================== */

/*
 * Matrix layout convention:
 *
 * Open Inventor SbMatrix is row-major: mat[row][col].
 * When passed as float[16], the layout is:
 *   mat[0][0], mat[0][1], mat[0][2], mat[0][3],   <- row 0
 *   mat[1][0], mat[1][1], mat[1][2], mat[1][3],   <- row 1
 *   ...
 *
 * OpenGL / GLSL expects column-major matrices.
 * glLoadMatrixf(mat) interprets the float array as column-major, so
 * OpenGL's column 0 = OI row 0.  This means OpenGL sees M_gl = M_oi^T.
 *
 * For glUniformMatrix4fv we use GL_TRUE (transpose flag) so that we can
 * pass the OI row-major data directly and GL will convert to column-major.
 * This makes the shader's M_gl = M_oi^T exactly as with glLoadMatrixf.
 *
 * For the normal matrix we compute M_oi^{-1} (which equals M_gl^{-T}, the
 * correct inverse-transpose for transforming normals), extract its upper-
 * left 3x3, and upload with GL_TRUE.
 */

void
SoGLModernState::setModelViewMatrix(const float mat[16])
{
    /* Store a copy of the OI row-major data */
    std::memcpy(mv_gl, mat, sizeof(mv_gl));
    fprintf(stderr, "DBG setMV ctxId=%u [0]=[%.2f %.2f %.2f %.2f] [3]=[%.2f %.2f %.2f %.2f]\n",
            context_id, mat[0], mat[1], mat[2], mat[3], mat[12], mat[13], mat[14], mat[15]);

    /* Compute the normal matrix: upper-left 3x3 of M_oi^{-1}.
     * SbMatrix stores its data as float[4][4] row-major.
     * We reconstruct an SbMatrix from the float[16] array. */
    SbMatrix m(mat[0], mat[1], mat[2],  mat[3],
               mat[4], mat[5], mat[6],  mat[7],
               mat[8], mat[9], mat[10], mat[11],
               mat[12], mat[13], mat[14], mat[15]);

    SbMatrix inv = m.inverse();
    const float (*inv_rows)[4] = inv.getValue();

    /* Store the 3x3 in row-major order (for GL_TRUE upload). */
    nm_gl[0] = inv_rows[0][0]; nm_gl[1] = inv_rows[0][1]; nm_gl[2] = inv_rows[0][2];
    nm_gl[3] = inv_rows[1][0]; nm_gl[4] = inv_rows[1][1]; nm_gl[5] = inv_rows[1][2];
    nm_gl[6] = inv_rows[2][0]; nm_gl[7] = inv_rows[2][1]; nm_gl[8] = inv_rows[2][2];
}

void
SoGLModernState::setProjectionMatrix(const float mat[16])
{
    std::memcpy(proj_gl, mat, sizeof(proj_gl));
    fprintf(stderr, "DBG setProjectionMatrix ctxId=%u [0]=[%.2f %.2f %.2f %.2f] [3]=[%.2f %.2f %.2f %.2f]\n",
            context_id, mat[0], mat[1], mat[2], mat[3], mat[12], mat[13], mat[14], mat[15]);
}

void
SoGLModernState::setMaterial(const float ambient[4],
                              const float diffuse[4],
                              const float specular[4],
                              const float emission[4],
                              float shininess, bool twoSided)
{
    fprintf(stderr, "DBG setMaterial ctxId=%u diffuse=(%.2f %.2f %.2f %.2f)\n",
            context_id, diffuse[0], diffuse[1], diffuse[2], diffuse[3]);
    std::memcpy(mat_ambient,  ambient,  4 * sizeof(float));
    std::memcpy(mat_diffuse,  diffuse,  4 * sizeof(float));
    std::memcpy(mat_specular, specular, 4 * sizeof(float));
    std::memcpy(mat_emission, emission, 4 * sizeof(float));
    mat_shininess = shininess;
    mat_twoSided  = twoSided;
}

void
SoGLModernState::setDiffuseColor(float r, float g, float b, float a)
{
    mat_diffuse[0] = r;
    mat_diffuse[1] = g;
    mat_diffuse[2] = b;
    mat_diffuse[3] = a;
}

void
SoGLModernState::resetLights()
{
    num_lights = 0;
}

int
SoGLModernState::addLight(const Light & light)
{
    if (num_lights >= OBOL_MODERN_MAX_LIGHTS)
        return -1;
    fprintf(stderr, "DBG addLight ctxId=%u diffuse=(%.2f %.2f %.2f %.2f) pos=(%.2f %.2f %.2f %.2f)\n",
            context_id, light.diffuse[0], light.diffuse[1], light.diffuse[2], light.diffuse[3],
            light.position[0], light.position[1], light.position[2], light.position[3]);
    lights[num_lights] = light;
    return num_lights++;
}

/* =========================================================================
 * Shader activation and uniform upload
 * ===================================================================== */

void
SoGLModernState::uploadMatricesToPhong() const
{
    if (p_uModelView    >= 0)
        glUniformMatrix4fv(p_uModelView,    1, GL_TRUE, mv_gl);
    if (p_uProjection   >= 0)
        glUniformMatrix4fv(p_uProjection,   1, GL_TRUE, proj_gl);
    if (p_uNormalMatrix >= 0)
        glUniformMatrix3fv(p_uNormalMatrix, 1, GL_TRUE, nm_gl);
}

void
SoGLModernState::uploadMatricesToBaseColor() const
{
    if (bc_uModelView  >= 0)
        glUniformMatrix4fv(bc_uModelView,  1, GL_TRUE, mv_gl);
    if (bc_uProjection >= 0)
        glUniformMatrix4fv(bc_uProjection, 1, GL_TRUE, proj_gl);
}

void
SoGLModernState::uploadMaterialToPhong() const
{
    if (p_uMatAmbient   >= 0) glUniform4fv(p_uMatAmbient,   1, mat_ambient);
    if (p_uMatDiffuse   >= 0) glUniform4fv(p_uMatDiffuse,   1, mat_diffuse);
    if (p_uMatSpecular  >= 0) glUniform4fv(p_uMatSpecular,  1, mat_specular);
    if (p_uMatEmission  >= 0) glUniform4fv(p_uMatEmission,  1, mat_emission);
    if (p_uMatShininess >= 0) glUniform1f (p_uMatShininess,    mat_shininess * 128.0f);
    if (p_uTwoSided     >= 0) glUniform1i (p_uTwoSided,        mat_twoSided ? 1 : 0);
}

void
SoGLModernState::uploadLightsToPhong() const
{
    if (p_uNumLights >= 0)
        glUniform1i(p_uNumLights, num_lights);

    for (int i = 0; i < num_lights && i < OBOL_MODERN_MAX_LIGHTS; ++i) {
        const Light & L = lights[i];
        if (p_uLights_position[i]      >= 0)
            glUniform4fv(p_uLights_position[i],      1, L.position);
        if (p_uLights_ambient[i]       >= 0)
            glUniform4fv(p_uLights_ambient[i],       1, L.ambient);
        if (p_uLights_diffuse[i]       >= 0)
            glUniform4fv(p_uLights_diffuse[i],       1, L.diffuse);
        if (p_uLights_specular[i]      >= 0)
            glUniform4fv(p_uLights_specular[i],      1, L.specular);
        if (p_uLights_spotDirection[i] >= 0)
            glUniform3fv(p_uLights_spotDirection[i], 1, L.spotDirection);
        if (p_uLights_spotCutoff[i]    >= 0)
            glUniform1f (p_uLights_spotCutoff[i],       L.spotCutoff);
        if (p_uLights_spotExponent[i]  >= 0)
            glUniform1f (p_uLights_spotExponent[i],     L.spotExponent);
        if (p_uLights_constantAtten[i] >= 0)
            glUniform1f (p_uLights_constantAtten[i],    L.constantAttenuation);
        if (p_uLights_linearAtten[i]   >= 0)
            glUniform1f (p_uLights_linearAtten[i],      L.linearAttenuation);
        if (p_uLights_quadraticAtten[i]>= 0)
            glUniform1f (p_uLights_quadraticAtten[i],   L.quadraticAttenuation);
    }
}

void
SoGLModernState::activatePhong(bool hasNormals, bool hasColors,
                                bool hasTexCoords, bool hasTexture)
{
    glUseProgram(phong_prog);
#ifdef OBOL_PORTABLEGL_BUILD
    /* Build the uniform struct and upload it via pglSetUniform.
     *
     * Coin/OI uses row-vector convention: points transform as p' = p * M,
     * so translation lives in the last ROW (M[3][0..2]).  pgl_rm4_v4 expects
     * the OpenGL column-vector convention: p' = M * p, translation in last
     * COLUMN.  These two conventions are related by transposition:
     *   M_OpenGL = M_Coin^T
     * Transpose mv_gl and proj_gl before copying so the shader math is correct.
     * The normal matrix (nm_gl) is already M_Coin^{-1}_{3x3} which equals
     * N_OpenGL, so it does NOT need transposing. */
    static PGLPhongUniforms pu;
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        pu.mv  [i*4+j] = mv_gl  [j*4+i];
        pu.proj[i*4+j] = proj_gl[j*4+i];
    }
    std::memcpy(pu.nm,   nm_gl,   sizeof(pu.nm));
    pu.numLights   = num_lights;
    pu.hasNormals  = hasNormals   ? 1 : 0;
    pu.hasColors   = hasColors    ? 1 : 0;
    pu.hasTexCoords= hasTexCoords ? 1 : 0;
    std::memcpy(pu.matAmbient,  mat_ambient,  4 * sizeof(float));
    std::memcpy(pu.matDiffuse,  mat_diffuse,  4 * sizeof(float));
    std::memcpy(pu.matSpecular, mat_specular, 4 * sizeof(float));
    std::memcpy(pu.matEmission, mat_emission, 4 * sizeof(float));
    pu.matShininess = mat_shininess * 128.0f;
    pu.twoSided     = mat_twoSided ? 1 : 0;
    for (int i = 0; i < num_lights && i < OBOL_MODERN_MAX_LIGHTS; ++i) {
        const Light& L = lights[i];
        std::memcpy(pu.lights[i].position,      L.position,      4 * sizeof(float));
        std::memcpy(pu.lights[i].ambient,        L.ambient,       4 * sizeof(float));
        std::memcpy(pu.lights[i].diffuse,        L.diffuse,       4 * sizeof(float));
        std::memcpy(pu.lights[i].specular,       L.specular,      4 * sizeof(float));
        std::memcpy(pu.lights[i].spotDirection,  L.spotDirection, 3 * sizeof(float));
        pu.lights[i].spotCutoff            = L.spotCutoff;
        pu.lights[i].spotExponent          = L.spotExponent;
        pu.lights[i].constantAttenuation   = L.constantAttenuation;
        pu.lights[i].linearAttenuation     = L.linearAttenuation;
        pu.lights[i].quadraticAttenuation  = L.quadraticAttenuation;
    }
    pglSetUniform(&pu);
#else
    uploadMatricesToPhong();
    uploadMaterialToPhong();
    uploadLightsToPhong();
    if (p_uHasNormals   >= 0) glUniform1i(p_uHasNormals,   hasNormals   ? 1 : 0);
    if (p_uHasColors    >= 0) glUniform1i(p_uHasColors,    hasColors    ? 1 : 0);
    if (p_uHasTexCoords >= 0) glUniform1i(p_uHasTexCoords, hasTexCoords ? 1 : 0);
    if (p_uHasTexture   >= 0) glUniform1i(p_uHasTexture,   hasTexture   ? 1 : 0);
    if (p_uTexture      >= 0) glUniform1i(p_uTexture, 0);
#endif
    (void)hasTexture;
}

void
SoGLModernState::activateBaseColor(bool hasColors, bool hasTexCoords,
                                    bool hasTexture)
{
    glUseProgram(basecolor_prog);
#ifdef OBOL_PORTABLEGL_BUILD
    /* Transpose Coin row-major matrices to OpenGL column-vector convention;
     * see activatePhong for detailed rationale. */
    static PGLBaseColorUniforms bu;
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        bu.mv  [i*4+j] = mv_gl  [j*4+i];
        bu.proj[i*4+j] = proj_gl[j*4+i];
    }
    bu.hasColors    = hasColors    ? 1 : 0;
    bu.hasTexCoords = hasTexCoords ? 1 : 0;
    std::memcpy(bu.baseColor, mat_diffuse, 4 * sizeof(float));
    pglSetUniform(&bu);
#else
    uploadMatricesToBaseColor();
    if (bc_uHasColors    >= 0) glUniform1i(bc_uHasColors,    hasColors    ? 1 : 0);
    if (bc_uHasTexCoords >= 0) glUniform1i(bc_uHasTexCoords, hasTexCoords ? 1 : 0);
    if (bc_uHasTexture   >= 0) glUniform1i(bc_uHasTexture,   hasTexture   ? 1 : 0);
    if (bc_uTexture      >= 0) glUniform1i(bc_uTexture, 0);
    if (bc_uBaseColor    >= 0)
        glUniform4fv(bc_uBaseColor, 1, mat_diffuse);
#endif
    (void)hasTexture;
}

void
SoGLModernState::deactivate()
{
    glUseProgram(0);
}
