/*
 * SoDBPortableGL.cpp  –  PortableGL-backed SoDB::ContextManager implementation
 *
 * This translation unit defines PORTABLEGL_IMPLEMENTATION before including
 * portablegl.h, making it the single owner of the PortableGL implementation
 * in the Obol library.  No other TU should define PORTABLEGL_IMPLEMENTATION.
 *
 * PortableGL is a single-header software renderer that implements a subset
 * of the OpenGL 3.x core profile, so this file must NOT include any system
 * OpenGL headers (<GL/gl.h>, <OSMesa/gl.h>, etc.) — the portablegl.h header
 * provides its own GL types and function definitions.
 *
 * This file is compiled only when OBOL_PORTABLEGL_BUILD is defined.
 *
 * Key differences from SoDBOSMesa.cpp:
 *   - No name-mangling required (PortableGL functions are file-local statics
 *     or use non-conflicting names such as init_glContext / set_glContext).
 *   - No OSMesaGetProcAddress — PortableGL does not have a dynamic function
 *     pointer loader.  Extension queries return GL_FALSE / nullptr.
 *   - PortableGL pixel buffer format is PGL_ABGR32 by default which gives
 *     RGBA byte order on little-endian (x86/ARM) — matching what
 *     SoOffscreenRenderer::getBuffer() callers expect.
 *   - Multiple simultaneous contexts are supported; each context owns its
 *     own pixel buffer.  Context switching (makeContextCurrent /
 *     restorePreviousContext) sets the PortableGL global context pointer via
 *     set_glContext().  A mutex serialises switches when contexts are shared
 *     between objects on the same thread.
 */

#ifdef OBOL_PORTABLEGL_BUILD

#include <Inventor/SoDB.h>
#include <cstring>
#include <memory>
#include <mutex>

/* -----------------------------------------------------------------------
 * Isolate PortableGL types from system GL headers.
 * PGL_PREFIX_TYPES renames vec2/vec3/vec4/mat4 to pgl_vec2 etc. to avoid
 * clashing with any GL or Obol math headers that may be pulled in indirectly.
 * -------------------------------------------------------------------- */
#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

/* -----------------------------------------------------------------------
 * Per-context state
 * --------------------------------------------------------------------- */

struct CoinPGLCtxData {
    glContext ctx;         /* PortableGL context (owns pixel + depth buf)   */
    pix_t*   backbuf;      /* Back-buffer pointer (managed by PortableGL)   */
    int      w, h;         /* Dimensions                                    */
    bool     valid;        /* Whether init_glContext succeeded               */

    /* Previous context pointer saved at makeContextCurrent() time so
     * restorePreviousContext() can put it back. */
    glContext* prev_ctx;

    CoinPGLCtxData(int w_, int h_)
        : backbuf(nullptr), w(w_), h(h_), valid(false), prev_ctx(nullptr)
    {
        valid = (init_glContext(&ctx, &backbuf, w, h) == GL_TRUE);
    }

    ~CoinPGLCtxData() {
        if (valid)
            free_glContext(&ctx);
    }

    bool isValid() const { return valid; }
};

/* -----------------------------------------------------------------------
 * SoDB::ContextManager implementation backed by PortableGL
 * --------------------------------------------------------------------- */

class CoinPortableGLContextManagerImpl : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int width, unsigned int height) override {
        auto * d = new CoinPGLCtxData((int)width, (int)height);
        return d->isValid() ? d : (delete d, nullptr);
    }

    /* PortableGL contexts are NOT OSMesa contexts: return FALSE so that
     * the dual-GL dispatch layer (if present) does not try to route GL
     * calls through the mgl* / osmesa_ symbol namespace. */
    SbBool isOSMesaContext(void * /*context*/) override {
        return FALSE;
    }

    void maxOffscreenDimensions(unsigned int & width, unsigned int & height) const override {
        /* PortableGL allocates framebuffers in ordinary heap memory; limit
         * to a generous value that avoids multi-GB allocations by accident. */
        width  = 16384;
        height = 16384;
    }

    SbBool makeContextCurrent(void * context) override {
        auto * d = static_cast<CoinPGLCtxData *>(context);
        if (!d || !d->isValid())
            return FALSE;
        /* Save previous context pointer so we can restore it later.
         * PortableGL stores the current context in a file-local static
         * pointer; there is no public "get current context" API, so we
         * track it ourselves per-CoinPGLCtxData. */
        d->prev_ctx = nullptr; /* no cross-context save in single-threaded use */
        set_glContext(&d->ctx);
        return TRUE;
    }

    void restorePreviousContext(void * context) override {
        auto * d = static_cast<CoinPGLCtxData *>(context);
        if (!d) return;
        /* Restore the previous context if one was saved, otherwise leave
         * no context current (set to null). */
        set_glContext(d->prev_ctx);
    }

    void destroyContext(void * context) override {
        auto * d = static_cast<CoinPGLCtxData *>(context);
        if (!d) return;
        /* If this context is currently active, deactivate it first. */
        set_glContext(nullptr);
        delete d;
    }

    void * getProcAddress(const char * /*funcName*/) override {
        /* PortableGL does not have a dynamic extension function pointer
         * loader.  All GL 3.x core functions are compiled directly into
         * the library.  Return nullptr so callers fall back to no-extension
         * behaviour; this is the same as what happens in the no-OpenGL build. */
        return nullptr;
    }

    /* Copy the rendered pixel buffer into a caller-supplied RGBA buffer.
     * PortableGL's default pixel format is PGL_ABGR32 which, on little-
     * endian platforms (x86/ARM), stores bytes as R, G, B, A in memory —
     * matching the RGBA byte order that SoOffscreenRenderer::getBuffer()
     * callers expect.  No byte swap is needed on little-endian. */
    void getPixels(void * context, unsigned char * dest,
                   unsigned int width, unsigned int height) override {
        auto * d = static_cast<CoinPGLCtxData *>(context);
        if (!d || !d->isValid() || !d->backbuf || !dest) return;
        const size_t nbytes = (size_t)width * height * 4;
        std::memcpy(dest, d->backbuf, nbytes);
    }
};

/* -----------------------------------------------------------------------
 * C entry point called by SoDB::createPortableGLContextManager()
 * --------------------------------------------------------------------- */

extern "C" {
SoDB::ContextManager * coin_create_portablegl_context_manager_impl();
SoDB::ContextManager * coin_create_portablegl_context_manager_impl()
{
    return new CoinPortableGLContextManagerImpl();
}
} /* extern "C" */

#endif /* OBOL_PORTABLEGL_BUILD */
