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
#include <portablegl/portablegl.h>

/* ─────────────────────────────────────────────────────────────────────────── */
/* Per-context data                                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

struct CoinPortableGLContextData {
    glContext pgl_ctx;
    pix_t*    backbuf;
    int       width;
    int       height;
    glContext* saved_prev;

    static glContext*& current_context() {
        /* Meyer's singleton for the current-context tracker.  One instance
         * per TU that includes this header; for single-threaded use this is
         * safe and consistent. */
        static glContext* s_current = nullptr;
        return s_current;
    }

    CoinPortableGLContextData(int w, int h)
        : backbuf(nullptr), width(w), height(h), saved_prev(nullptr)
    {}

    ~CoinPortableGLContextData() {
        if (current_context() == &pgl_ctx) {
            set_glContext(nullptr);
            current_context() = nullptr;
        }
        free_glContext(&pgl_ctx);
    }

    bool init() {
        return init_glContext(&pgl_ctx, &backbuf, width, height) != GL_FALSE;
    }

    bool isValid() const { return backbuf != nullptr; }

    void makeCurrent() {
        saved_prev = current_context();
        current_context() = &pgl_ctx;
        set_glContext(&pgl_ctx);
    }

    void restorePrev() {
        current_context() = saved_prev;
        set_glContext(saved_prev);
        saved_prev = nullptr;
    }
};

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

    /* getProcAddress delegates to the static table in SoDBPortableGL.cpp.
     * The symbol is declared extern "C" so this header does not need to pull
     * in SoDBPortableGL.cpp directly. */
    virtual void * getProcAddress(const char * funcName) override {
        extern "C" void* obol_portablegl_getprocaddress(const char*);
        return obol_portablegl_getprocaddress(funcName);
    }
};

#endif /* OBOL_PORTABLEGL_CONTEXT_MANAGER_H */
