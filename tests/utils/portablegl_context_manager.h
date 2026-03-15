/*
 * portablegl_context_manager.h  –  reusable PortableGL SoDB::ContextManager
 *
 * Provides CoinPortableGLContextManager: a SoDB::ContextManager implementation
 * that creates offscreen rendering contexts using PortableGL, the single-header
 * software renderer that implements a subset of the OpenGL 3.x core profile.
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
 *   // The rendered RGBA pixels are available via:
 *   const unsigned char* buf = renderer.getBuffer();
 *
 * Requires:
 *   - external/portablegl/portablegl.h (available via git submodule)
 *   - OBOL_PORTABLEGL_BUILD defined (set by CMake when OBOL_USE_PORTABLEGL=ON)
 *
 * IMPORTANT: Define PORTABLEGL_IMPLEMENTATION in exactly ONE translation unit
 * before including portablegl.h.  This header does NOT define it, so the
 * caller (or a dedicated .cpp) must do so.  In practice the implementation
 * lives in src/misc/SoDBPortableGL.cpp which provides
 * coin_create_portablegl_context_manager_impl().
 *
 * Thread safety:
 *   PortableGL stores its "current context" in a file-local static pointer
 *   (not thread_local).  Concurrent use from multiple threads requires
 *   external serialisation.  For single-threaded use this class is safe.
 *
 * Pixel format:
 *   PortableGL's default format (PGL_ABGR32) on little-endian platforms
 *   (x86 / ARM) gives RGBA byte order in memory, matching what
 *   SoOffscreenRenderer::getBuffer() callers expect.
 */

#ifndef OBOL_PORTABLEGL_CONTEXT_MANAGER_H
#define OBOL_PORTABLEGL_CONTEXT_MANAGER_H

#ifdef OBOL_PORTABLEGL_BUILD

#include <Inventor/SoDB.h>
#include <cstring>
#include <memory>

/* PGL_PREFIX_TYPES avoids clashes between PortableGL's built-in vec2/vec3/
 * vec4/mat4 types and any system GL or Obol math headers. */
#define PGL_PREFIX_TYPES
#include <portablegl.h>

/* -------------------------------------------------------------------------
 * Per-context data for one PortableGL offscreen surface
 * ----------------------------------------------------------------------- */

/* Thread-local pointer to the currently active CoinPGLContextData.
 * Updated by makeCurrent() / restorePreviousContext() to allow nested
 * context switches (e.g. SoSceneTexture2 creating a sub-context). */
static CoinPGLContextData* s_pgl_current_ctx = nullptr;

struct CoinPGLContextData {
    glContext  ctx;       /* PortableGL context struct (owns its buffers)  */
    pix_t*     backbuf;   /* Pointer to the rendered pixel buffer          */
    int        width, height;
    bool       valid;
    /* Previous context saved at makeContextCurrent() for later restore.  */
    CoinPGLContextData* prev_ctx_data;

    CoinPGLContextData(int w, int h)
        : backbuf(nullptr), width(w), height(h), valid(false), prev_ctx_data(nullptr)
    {
        valid = (init_glContext(&ctx, &backbuf, w, h) == GL_TRUE);
    }

    ~CoinPGLContextData() {
        if (valid)
            free_glContext(&ctx);
    }

    bool isValid() const { return valid; }

    bool makeCurrent() {
        if (!valid) return false;
        /* Save the currently active context so restorePreviousContext can
         * reinstate it.  We track this via s_pgl_current_ctx rather than
         * trying to read PortableGL's internal 'c' global (which is static
         * to the PORTABLEGL_IMPLEMENTATION TU and not accessible here). */
        prev_ctx_data = s_pgl_current_ctx;
        s_pgl_current_ctx = this;
        set_glContext(&ctx);
        return true;
    }
};

/* -------------------------------------------------------------------------
 * SoDB::ContextManager implementation for PortableGL
 * ----------------------------------------------------------------------- */

class CoinPortableGLContextManager : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new CoinPGLContextData((int)width, (int)height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }

    /* PortableGL contexts are not OSMesa contexts — return FALSE so that
     * dual-GL dispatch routes to system GL, not the osmesa_ prefix. */
    SbBool isOSMesaContext(void * /*context*/) override {
        return FALSE;
    }

    void maxOffscreenDimensions(unsigned int & width, unsigned int & height) const override {
        width  = 16384;
        height = 16384;
    }

    SbBool makeContextCurrent(void * context) override {
        auto * d = static_cast<CoinPGLContextData *>(context);
        return (d && d->makeCurrent()) ? TRUE : FALSE;
    }

    void restorePreviousContext(void * context) override {
        auto * d = static_cast<CoinPGLContextData *>(context);
        if (!d) return;
        /* Restore the context that was active when makeContextCurrent was
         * last called for 'd'.  If no outer context existed, set_glContext(nullptr)
         * deactivates portablegl — that is the correct state since no context
         * should be current outside of a render. */
        s_pgl_current_ctx = d->prev_ctx_data;
        set_glContext(d->prev_ctx_data ? &d->prev_ctx_data->ctx : nullptr);
    }

    void destroyContext(void * context) override {
        auto * d = static_cast<CoinPGLContextData *>(context);
        if (!d) return;
        set_glContext(nullptr);
        delete d;
    }

    /* PortableGL has no dynamic function pointer loader; all GL 3.x core
     * functions are compiled in.  Return nullptr so callers degrade
     * gracefully. */
    void * getProcAddress(const char * /*funcName*/) override {
        return nullptr;
    }
};

#endif /* OBOL_PORTABLEGL_BUILD */
#endif /* OBOL_PORTABLEGL_CONTEXT_MANAGER_H */
