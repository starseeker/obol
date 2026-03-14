/*
 * portablegl_context_manager.h  –  reusable PortableGL SoDB::ContextManager
 *
 * Provides CoinPortableGLContextManager: a SoDB::ContextManager implementation
 * that creates offscreen GL contexts using PortableGL.  This header-only class
 * is analogous to osmesa_context_manager.h and can be included independently of
 * headless_utils.h so that individual panels in a multi-backend viewer can own
 * their own PortableGL context manager.
 *
 * Usage:
 *
 *   #include "portablegl_context_manager.h"
 *
 *   // Create a dedicated PortableGL manager per renderer:
 *   static CoinPortableGLContextManager pgl_mgr;
 *   SoOffscreenRenderer renderer(&pgl_mgr, viewport);
 *   renderer.render(root);
 *
 * Known obstacles
 * ───────────────
 * This context manager is functional (context creation/switching/destruction
 * works correctly) but rendered output will be incorrect because PortableGL
 * has the following critical limitations when used with Obol:
 *
 *  1. GLSL (BLOCKER): glShaderSource / glCompileShader / glLinkProgram are
 *     no-ops.  All GLSL-based lighting, materials, shadows and textures are
 *     silently broken.
 *
 *  2. Uniforms (BLOCKER): All glUniform*() and glUniformMatrix*() functions
 *     are empty stubs.  Shader parameters (MVP matrices, lighting, materials)
 *     are never set.
 *
 *  3. FBOs (BLOCKER): glGenFramebuffers, glBindFramebuffer, etc. are stubs.
 *     Render-to-texture (SoSceneTexture2) and shadow-map passes will fail.
 *
 *  4. glReadPixels is missing; a workaround reading from pglGetBackBuffer()
 *     is provided in SoDBPortableGL.cpp and exposed via getProcAddress().
 *
 * Requires:
 *   - Obol built with OBOL_USE_PORTABLEGL=ON
 *   - external/portablegl/portablegl.h present
 */

#ifndef OBOL_PORTABLEGL_CONTEXT_MANAGER_H
#define OBOL_PORTABLEGL_CONTEXT_MANAGER_H

#include <Inventor/SoDB.h>
#include <cstring>

/* Include PortableGL declarations (no implementation here). */
#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#define PGL_PREFIX_GL    1
#include <portablegl/portablegl.h>
#include <portablegl/portablegl_compat_consts.h>

/* Include the compat state so makeCurrent can set g_cur_compat. */
#include "portablegl_compat_state.h"

/* g_cur_compat is defined in portablegl_compat_funcs.cpp; declared here so
 * CoinPortableGLContextData::makeCurrent() can set it. */
extern thread_local ObolPGLCompatState* g_cur_compat;

/* Reset per-context caches (VAO/VBO/program handles) on context switch.
 * Defined in portablegl_compat_funcs.cpp. */
extern "C" void pgl_igl_reset_context_caches();

/* ─────────────────────────────────────────────────────────────────────────── */
/* Per-context data                                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

struct CoinPortableGLContextData {
    glContext          pgl_ctx;
    pix_t*             backbuf;
    int                width;
    int                height;
    glContext*         saved_prev;
    ObolPGLCompatState compat;
    ObolPGLCompatState* saved_compat;

    static glContext*& current_context() {
        static glContext* s_current = nullptr;
        return s_current;
    }

    CoinPortableGLContextData(int w, int h)
        : backbuf(nullptr), width(w), height(h), saved_prev(nullptr), saved_compat(nullptr)
    { compat.init(); }

    ~CoinPortableGLContextData() {
        if (current_context() == &pgl_ctx) {
            set_glContext(nullptr);
            current_context() = nullptr;
            g_cur_compat = nullptr;
        }
        free_glContext(&pgl_ctx);
    }

    bool init() {
        return init_glContext(&pgl_ctx, &backbuf, width, height) != GL_FALSE;
    }

    bool isValid() const { return backbuf != nullptr; }

    void makeCurrent() {
        saved_prev   = current_context();
        saved_compat = g_cur_compat;
        current_context() = &pgl_ctx;
        set_glContext(&pgl_ctx);
        g_cur_compat = &compat;
        pglSetUniform(&compat);
        /* Reset per-context caches whenever the portablegl context changes.
         * Each context has its own object-name namespace.                    */
        pgl_igl_reset_context_caches();
    }

    void restorePrev() {
        current_context() = saved_prev;
        set_glContext(saved_prev);
        g_cur_compat = saved_compat;
        saved_prev   = nullptr;
        saved_compat = nullptr;
    }
};

/* getProcAddress delegates to the static table in SoDBPortableGL.cpp.
 * Declared at namespace scope because extern "C" is not allowed inside
 * a member function body (C++ linkage specifications are namespace-scope only). */
#ifdef __cplusplus
extern "C" {
#endif
void* obol_portablegl_getprocaddress(const char* funcName);
#ifdef __cplusplus
}
#endif

/* ─────────────────────────────────────────────────────────────────────────── */
/* SoDB::ContextManager implementation                                         */
/* ─────────────────────────────────────────────────────────────────────────── */

class CoinPortableGLContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width,
                                         unsigned int height) override {
        auto* d = new CoinPortableGLContextData((int)width, (int)height);
        if (!d->init()) {
            delete d;
            return nullptr;
        }
        return d;
    }

    virtual SbBool isOSMesaContext(void * /*context*/) override {
        return FALSE;
    }

    virtual SbBool isPortableGLContext(void * /*context*/) override {
        return TRUE;
    }

    virtual void maxOffscreenDimensions(unsigned int & width,
                                        unsigned int & height) const override {
        width  = 16384;
        height = 16384;
    }

    virtual SbBool makeContextCurrent(void * context) override {
        if (!context) return FALSE;
        static_cast<CoinPortableGLContextData*>(context)->makeCurrent();
        return TRUE;
    }

    virtual void restorePreviousContext(void * context) override {
        if (!context) return;
        static_cast<CoinPortableGLContextData*>(context)->restorePrev();
    }

    virtual void destroyContext(void * context) override {
        delete static_cast<CoinPortableGLContextData*>(context);
    }

    /* getProcAddress delegates to the static table in SoDBPortableGL.cpp. */
    virtual void * getProcAddress(const char * funcName) override {
        return obol_portablegl_getprocaddress(funcName);
    }
};

#endif /* OBOL_PORTABLEGL_CONTEXT_MANAGER_H */
