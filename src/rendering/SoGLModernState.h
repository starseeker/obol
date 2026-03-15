/*
 * SoGLModernState.h  --  Per-context modern OpenGL state manager
 *
 * This header is part of Obol's Phase 1 modernization: replacing the
 * fixed-function GL pipeline (glBegin/glEnd, glMatrixMode, glMaterialfv,
 * glLightfv, ...) with explicit VAO/VBO geometry and shader uniforms.
 *
 * SoGLModernState is a per-context object.  One instance is created the
 * first time SoGLModernState::forContext() is called with a given context
 * ID and is destroyed when that context is destroyed (via SoContextHandler).
 *
 * API summary:
 *
 *   // Get or create the state for the current context:
 *   SoGLModernState * ms = SoGLModernState::forContext(contextId);
 *
 *   // Check availability (false if shader compilation failed):
 *   if (!ms || !ms->isAvailable()) { // fall back to legacy path
 *   }
 *
 *   // Called by matrix elements when the model-view / projection changes:
 *   ms->setModelViewMatrix(mat4_row_major);
 *   ms->setProjectionMatrix(mat4_row_major);
 *
 *   // Called by SoGLLazyElement when material changes:
 *   ms->setMaterial(ambient, diffuse, specular, emission, shininess, twoSided);
 *
 *   // Called by light nodes (reset once per frame, then addLight per light):
 *   ms->resetLights();
 *   ms->addLight(light);
 *
 *   // Called by shape-rendering code to activate the built-in shader:
 *   ms->activatePhong();  // or ms->activateBaseColor()
 *   // ... draw with glDrawArrays / glDrawElements ...
 *   ms->deactivate();     // restore glUseProgram(0)
 *
 * Matrix convention:
 *   Matrices are passed in Open Inventor's native row-major order.
 *   The state manager handles the OI-to-GL transpose convention
 *   internally.  The GLSL uniforms see column-major data as expected.
 */

#ifndef OBOL_SOGLMODERNSTATE_H
#define OBOL_SOGLMODERNSTATE_H

#ifndef OBOL_INTERNAL
#error this is a private header file
#endif

#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <Inventor/system/gl.h>

/* Maximum lights matching OBOL_MAX_LIGHTS in obol_modern_shaders.h */
#define OBOL_MODERN_MAX_LIGHTS 8

/*
 * SoGLModernState -- per-context modern GL rendering state.
 */
class SoGLModernState {
public:

    /* ----------------------------------------------------------------
     * Light descriptor.  Light positions / spot directions must be
     * in eye space (already transformed by the view matrix) when stored
     * here.  The light nodes are responsible for the transformation.
     * -------------------------------------------------------------- */
    struct Light {
        float position[4];           /* w=0 directional, w=1 positional */
        float ambient[4];
        float diffuse[4];
        float specular[4];
        float spotDirection[3];
        float spotCutoff;            /* cos of half-angle; -1 = non-spot */
        float spotExponent;
        float constantAttenuation;
        float linearAttenuation;
        float quadraticAttenuation;

        Light();  /* initialises to a disabled directional white light */
    };

    /* ----------------------------------------------------------------
     * Life cycle
     * -------------------------------------------------------------- */

    /* Get (or create) the state for the given context ID.
     * The GL context must be current when first called for a given ID.
     * Returns nullptr if shader compilation fails. */
    static SoGLModernState * forContext(uint32_t contextId);

    /* Returns true if the built-in shader programs compiled without error. */
    bool isAvailable() const { return phong_prog != 0; }

    /* ----------------------------------------------------------------
     * State setters -- called by elements when their state changes.
     * The matrices are given in Open Inventor row-major order.
     * -------------------------------------------------------------- */

    void setModelViewMatrix(const float mat[16]);
    void setProjectionMatrix(const float mat[16]);

    /* Set all material parameters at once. */
    void setMaterial(const float ambient[4],
                     const float diffuse[4],
                     const float specular[4],
                     const float emission[4],
                     float shininess,
                     bool  twoSided = false);

    /* Convenience: set just the diffuse colour (for SoBaseColor / glColor). */
    void setDiffuseColor(float r, float g, float b, float a = 1.0f);

    /* Light management -- call resetLights() at frame start, then
     * addLight() for each active light in the scene. */
    void resetLights();
    int  addLight(const Light & light);  /* returns index, or -1 if full */

    /* ----------------------------------------------------------------
     * Shader activation
     * -------------------------------------------------------------- */

    /* Activate the built-in Phong lighting shader and upload the current
     * matrix / material / light state as uniforms. */
    void activatePhong(bool hasNormals   = true,
                       bool hasColors    = false,
                       bool hasTexCoords = false,
                       bool hasTexture   = false);

    /* Activate the built-in base-colour shader (no lighting). */
    void activateBaseColor(bool hasColors    = false,
                           bool hasTexCoords = false,
                           bool hasTexture   = false);

    /* Restore the default shader state (glUseProgram(0)). */
    void deactivate();

    /* ----------------------------------------------------------------
     * Accessors
     * -------------------------------------------------------------- */
    GLuint getPhongProgram()     const { return phong_prog; }
    GLuint getBaseColorProgram() const { return basecolor_prog; }

    /* Access the cached modelview/projection matrices (column-major float[16]
     * in GL layout, derived from the OI row-major data via transpose). */
    const float * getModelViewGL()  const { return mv_gl; }
    const float * getProjectionGL() const { return proj_gl; }

private:
    explicit SoGLModernState(uint32_t contextId);
    ~SoGLModernState();

    /* Compiles the two built-in shader programs from obol_modern_shaders.h. */
    void initShaders();

    /* Caches uniform locations after program link. */
    void cachePhongLocations();
    void cacheBaseColorLocations();

    /* Upload helpers (called inside activatePhong / activateBaseColor). */
    void uploadMatricesToPhong() const;
    void uploadMatricesToBaseColor() const;
    void uploadMaterialToPhong() const;
    void uploadLightsToPhong() const;

    /* Context destruction callback registered with SoContextHandler. */
    static void contextDestructionCB(uint32_t contextId, void * userdata);

    /* ----------------------------------------------------------------
     * Per-context data
     * -------------------------------------------------------------- */
    uint32_t context_id;

    /* Built-in shader programs */
    GLuint phong_prog;
    GLuint basecolor_prog;

    /* Phong uniform locations */
    GLint p_uModelView;
    GLint p_uProjection;
    GLint p_uNormalMatrix;
    GLint p_uHasNormals;
    GLint p_uHasColors;
    GLint p_uHasTexCoords;
    GLint p_uNumLights;
    /* Per-light uniform locations (indexed [0..OBOL_MODERN_MAX_LIGHTS-1]) */
    GLint p_uLights_position[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_ambient[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_diffuse[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_specular[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_spotDirection[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_spotCutoff[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_spotExponent[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_constantAtten[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_linearAtten[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uLights_quadraticAtten[OBOL_MODERN_MAX_LIGHTS];
    GLint p_uMatAmbient;
    GLint p_uMatDiffuse;
    GLint p_uMatSpecular;
    GLint p_uMatEmission;
    GLint p_uMatShininess;
    GLint p_uTwoSided;
    GLint p_uTexture;
    GLint p_uHasTexture;

    /* BaseColor uniform locations */
    GLint bc_uModelView;
    GLint bc_uProjection;
    GLint bc_uHasColors;
    GLint bc_uHasTexCoords;
    GLint bc_uBaseColor;
    GLint bc_uTexture;
    GLint bc_uHasTexture;

    /* Current state (GL column-major float[16]) */
    float mv_gl[16];      /* model-view in GL layout */
    float proj_gl[16];    /* projection in GL layout  */
    float nm_gl[9];       /* normal matrix (3x3, GL layout) */

    /* Material */
    float mat_ambient[4];
    float mat_diffuse[4];
    float mat_specular[4];
    float mat_emission[4];
    float mat_shininess;
    bool  mat_twoSided;

    /* Lights */
    int   num_lights;
    Light lights[OBOL_MODERN_MAX_LIGHTS];

    /* ----------------------------------------------------------------
     * Static registry: one instance per context.
     * -------------------------------------------------------------- */
    static std::unordered_map<uint32_t, SoGLModernState*> instances;
    static std::mutex                                      instances_mutex;
};

#endif /* OBOL_SOGLMODERNSTATE_H */
