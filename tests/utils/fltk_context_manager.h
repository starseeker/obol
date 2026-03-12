/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*
 * FLTKContextManager: Cross-platform OpenGL context manager for Obol
 * that delegates context creation to FLTK's Fl_Gl_Window.
 *
 * FLTK abstracts the platform-specific GL context setup:
 *   - WGL on Windows
 *   - CGL / NSOpenGL on macOS
 *   - GLX on X11 / Linux
 *
 * Preferred mode (setExternalWindow):
 *   The caller (obol_viewer) passes its own Fl_Gl_Window – typically the
 *   CoinPanel rendering widget – via setExternalWindow() before the first
 *   render.  The manager then calls make_current() on that window instead
 *   of creating a separate hidden context window.  This mirrors the pattern
 *   used by cube.cxx in the FLTK test suite: a real, visible Fl_Gl_Window
 *   is created and shown by the caller; FLTK's GL context lifetime is then
 *   tied to that visible widget rather than to a synthetic off-screen window.
 *
 * Fallback mode (own hidden window):
 *   If setExternalWindow() is never called the manager creates a small
 *   hidden 1×1 Fl_Gl_Window lazily on the first render request, as before.
 *   This preserves backward compatibility for environments that do not use
 *   the external-window path.
 *
 * Offscreen rendering uses framebuffer objects (FBOs) managed by the Obol
 * library; the FLTK window surface itself is never drawn to directly by
 * this context manager.
 *
 * This header provides drop-in replacements for the same functions that
 * headless_utils.h exposes for the system-GL / GLX path:
 *
 *   initCoinHeadless()              – initialise SoDB with the FLTK manager
 *   getCoinHeadlessContextManager() – return the installed context manager
 *   getSharedRenderer()             – return a shared SoOffscreenRenderer
 *
 * Compile obol_viewer with -DOBOL_VIEWER_FLTK_GL to select this header
 * instead of headless_utils.h for the system-GL panel.  The GLX header
 * (included by headless_utils.h) is then not required by the viewer itself,
 * which means the viewer can build and run on any platform that FLTK
 * supports without needing Xvfb or a direct X11 connection.
 *
 * Note: on Linux/X11 the getProcAddress() implementation still calls
 * glXGetProcAddress() directly because that is the standard GL extension
 * function loader on that platform regardless of which code created the
 * GL context.  The libGL / OpenGL::GL link target is therefore still
 * required on Linux even when using this FLTK-based manager.
 */

#ifndef FLTK_CONTEXT_MANAGER_H
#define FLTK_CONTEXT_MANAGER_H

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>

#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>

#include <cstdio>
#include <cstdlib>

/* Platform-specific GL function pointer loader */
#if defined(_WIN32)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <dlfcn.h>
#  include <OpenGL/gl.h>
/* CGL (Core OpenGL) provides the C-level context save/restore API needed
 * by restorePreviousContext() to undo the makeCurrentContext call that
 * make_current() performs on the NSOpenGLContext.  CGLGetCurrentContext()
 * and CGLSetCurrentContext() are available on every macOS version that
 * FLTK supports and do not require Objective-C code. */
#  include <OpenGL/OpenGL.h>
#else
/* Linux / X11: glXGetProcAddress is always available via libGL. */
#  include <GL/glx.h>
/* FL/platform.H exposes fl_display (the X11 Display* FLTK uses) which is
 * needed for XSync() in ensureWindow(). */
#  include <FL/platform.H>
#endif

/* Match default dimensions used in headless_utils.h */
#ifndef DEFAULT_WIDTH
#  define DEFAULT_WIDTH  800
#  define DEFAULT_HEIGHT 600
#endif

/* =========================================================================
 * GL diagnostic helper
 *
 * reportGL(where) checks whether a valid OpenGL context is current by
 * calling glGetString(GL_VERSION).
 *
 * Printing policy:
 *   - Always prints for the first kAlwaysPrintCalls invocations so that
 *     GL context lifecycle problems appear in crash logs unconditionally.
 *   - Always prints when glGetString returns NULL (that is always a bug).
 *   - Prints on all subsequent calls when OBOL_GL_DIAG=1 is set.
 *
 * The output goes to stderr immediately (flushed) so it appears even if
 * the process crashes shortly after.
 *
 * Returns true if a GL context is current, false otherwise.
 * ======================================================================= */
inline bool reportGL(const char* where)
{
    static const bool diag_env = (getenv("OBOL_GL_DIAG") != nullptr);
    static int call_count = 0;
    ++call_count;
    /* Always print the first few calls and any NULL-context event. */
    static const int kAlwaysPrintCalls = 5;
    const GLubyte* ver = glGetString(GL_VERSION);
    const bool always = (call_count <= kAlwaysPrintCalls) || !ver;
    if (diag_env || always) {
        if (ver) {
            const GLubyte* ren  = glGetString(GL_RENDERER);
            const GLubyte* vend = glGetString(GL_VENDOR);
            const GLubyte* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
            fprintf(stderr,
                    "[GL diag] %s:\n"
                    "          GL_VERSION                  = \"%s\"\n"
                    "          GL_RENDERER                 = \"%s\"\n"
                    "          GL_VENDOR                   = \"%s\"\n"
                    "          GL_SHADING_LANGUAGE_VERSION = \"%s\"\n",
                    where,
                    (const char*)ver,
                    ren  ? (const char*)ren  : "(null)",
                    vend ? (const char*)vend : "(null)",
                    glsl ? (const char*)glsl : "(null)");
        } else {
            fprintf(stderr,
                    "[GL diag] %s:\n"
                    "          glGetString(GL_VERSION) = NULL"
                    "\n          (no current GL context!)\n",
                    where);
        }
        fflush(stderr);
    }
    return (ver != nullptr);
}

/* =========================================================================
 * FLTKGLContextWindow
 *
 * Minimal hidden GL window.  Its only purpose is to give Obol a valid
 * OpenGL context to render into FBOs.  FLTK's Fl_Gl_Window::show()
 * creates the platform GL context; make_current() activates it.
 * ======================================================================= */
class FLTKGLContextWindow : public Fl_Gl_Window {
public:
    FLTKGLContextWindow()
        : Fl_Gl_Window(1, 1, "obol_ctx")
    {
        /* Single-buffered RGBA8 context with a 24-bit depth buffer.
         * We use FBOs for all offscreen rendering and never draw to the
         * window surface, so double-buffering is unnecessary.  Using
         * FL_SINGLE avoids FLTK calling glXSwapBuffers() on this hidden
         * window which can leave stale GL errors in the queue that are
         * then mis-attributed to subsequent glUseProgramObjectARB calls
         * (manifesting as spurious GL_INVALID_OPERATION after a camera
         * drag in scenes that use SoShadowGroup). */
        mode(FL_RGB8 | FL_DEPTH | FL_SINGLE);
    }

    /* Required override; rendering targets FBOs so nothing is drawn here. */
    void draw() override {}

    /* Suppress FLTK's automatic flush/swap cycle for this hidden window.
     * We never render to the window surface; all rendering is FBO-based.
     * Without this override, FLTK's expose-event handling calls flush()
     * which calls make_current() + swap_buffers(), potentially corrupting
     * the GL context state between offscreen renders. */
    void flush() override { clear_damage(0); }
};

/* =========================================================================
 * FLTKOffscreenCtx  –  per-context bookkeeping
 *
 * All FLTKOffscreenCtx instances share the same underlying GL context
 * (the hidden FLTKGLContextWindow).  This struct records the dimensions
 * that were requested when the context was created.
 * ======================================================================= */
struct FLTKOffscreenCtx {
    unsigned int width;
    unsigned int height;
    bool         saved = false;  /* true when prev_* fields hold a valid saved state */
#if defined(_WIN32)
    /* Saved WGL context state so restorePreviousContext() can undo the
     * wglMakeCurrent() call made by win_->make_current() (which calls
     * Fl_WinAPI_Gl_Window_Driver::set_gl_context → wglMakeCurrent).
     * Without this save/restore, a temporary makeContextCurrent() (e.g.
     * from SoGLContext_context_max_dimensions) leaves the FLTK context
     * as the current WGL context, which can interfere with other GL
     * backends (e.g. OSMesa) that were active before the call. */
    HGLRC        prev_wgl_ctx  = nullptr;
    HDC          prev_wgl_dc   = nullptr;
#elif defined(__APPLE__)
    /* Saved CGL context handle so restorePreviousContext() can undo the
     * NSOpenGLContext::makeCurrentContext call made by make_current().
     * CGLGetCurrentContext / CGLSetCurrentContext are the C-level API
     * that underpins NSOpenGLContext on all macOS versions FLTK supports,
     * and can be used from C++ without Objective-C syntax. */
    CGLContextObj prev_cgl_ctx = nullptr;
#else
    /* Saved GLX context state so restorePreviousContext() can undo the
     * glXMakeCurrent() call made by makeContextCurrent().  Without this
     * save/restore, a temporary makeContextCurrent() (e.g. from
     * SoGLContext_context_max_dimensions) leaves the FLTK window as the
     * current GLX context and interferes with any OSMesa or other GLX
     * context that was current before the call. */
    GLXContext   prev_glx_ctx  = nullptr;
    GLXDrawable  prev_glx_draw = 0;
    Display *    prev_glx_dpy  = nullptr;
#endif
};

/* =========================================================================
 * FLTKContextManager
 *
 * Implements SoDB::ContextManager using FLTK's Fl_Gl_Window so that
 * context creation is handled by FLTK on every supported platform.
 *
 * Two operating modes:
 *
 *   External-window mode (preferred):
 *     The caller invokes setExternalWindow(w) before the first render,
 *     passing a real Fl_Gl_Window that is already part of the application's
 *     visible widget hierarchy.  On the first makeContextCurrent() call the
 *     manager simply calls w->make_current() on that window.  This mirrors
 *     the cube.cxx FLTK example pattern and avoids the off-screen hidden-
 *     window approach that can fail in certain CI / headless environments.
 *
 *   Fallback / hidden-window mode:
 *     If setExternalWindow() is never called a small hidden 1×1
 *     FLTKGLContextWindow is created lazily on the first render request
 *     (the original behaviour), preserving backward compatibility.
 * ======================================================================= */
class FLTKContextManager : public SoDB::ContextManager {
public:
    FLTKContextManager() : win_(nullptr), own_win_(nullptr) {}

    virtual ~FLTKContextManager() {
        /* Only destroy the window we created ourselves; the external window
         * is owned by the caller (e.g. CoinPanel in obol_viewer). */
        if (own_win_) {
            own_win_->hide();
            delete own_win_;
            own_win_ = nullptr;
        }
        win_ = nullptr;
    }

    /**
     * Register an externally-owned Fl_Gl_Window as the GL context source.
     *
     * Must be called before the first render attempt (i.e., before the
     * first makeContextCurrent() call).  The window must remain alive for
     * the lifetime of this manager.  The caller retains ownership.
     *
     * Typical usage in obol_viewer: call this in CoinPanel's constructor
     * so the panel's own GL context is used instead of a hidden window.
     */
    void setExternalWindow(Fl_Gl_Window* w) {
        /* Ignore if the fallback hidden window was already created. */
        if (own_win_) {
            fprintf(stderr,
                    "[FLTKContextManager::setExternalWindow] ignored: own_win_=%p"
                    " already created (fallback mode active); w=%p rejected.\n",
                    (void*)own_win_, (void*)w);
            fflush(stderr);
            return;
        }
        fprintf(stderr,
                "[FLTKContextManager::setExternalWindow] registered external"
                " window %p (replacing %p).\n",
                (void*)w, (void*)win_);
        fflush(stderr);
        win_ = w;
    }

    virtual void* createOffscreenContext(unsigned int width,
                                         unsigned int height) override
    {
        static const bool diag = (getenv("OBOL_GL_DIAG") != nullptr);
        static int ctx_count = 0;
        ++ctx_count;
        if (diag || ctx_count <= 3) {
            fprintf(stderr,
                    "[FLTKContextManager::createOffscreenContext #%d]"
                    " %ux%u  win_=%p\n",
                    ctx_count, width, height, (void*)win_);
            fflush(stderr);
        }
        if (!ensureWindow()) return nullptr;
        return new FLTKOffscreenCtx{width, height};
    }

    virtual SbBool makeContextCurrent(void* context) override {
        static const bool diag = (getenv("OBOL_GL_DIAG") != nullptr);
        static int mcc_count = 0;
        ++mcc_count;
        const bool verbose = diag || (mcc_count <= 5);

        if (!ensureWindow()) return FALSE;

        if (verbose) {
            const bool is_own = (win_ == own_win_);
            fprintf(stderr,
                    "[FLTKContextManager::makeContextCurrent #%d]"
                    " win_=%p (%s) context=%p shown=%d valid=%d\n",
                    mcc_count, (void*)win_,
                    is_own ? "own/fallback" : "external",
                    (void*)win_->context(), (int)win_->shown(),
                    (int)win_->valid());
#if !defined(_WIN32) && !defined(__APPLE__)
            fprintf(stderr,
                    "[FLTKContextManager::makeContextCurrent #%d]"
                    " fl_xid(win_)=0x%lx\n",
                    mcc_count, (unsigned long)fl_xid(win_));
#endif
            fflush(stderr);
        }

#if defined(_WIN32)
        /* Save the current WGL context so restorePreviousContext() can
         * reinstate it.  Same rationale as the GLX path below. */
        if (FLTKOffscreenCtx* fctx = static_cast<FLTKOffscreenCtx*>(context)) {
            fctx->prev_wgl_ctx = wglGetCurrentContext();
            fctx->prev_wgl_dc  = wglGetCurrentDC();
            fctx->saved        = true;
        }
#elif defined(__APPLE__)
        /* Save the current CGL context so restorePreviousContext() can
         * reinstate it.  Same rationale as the GLX path below. */
        if (FLTKOffscreenCtx* fctx = static_cast<FLTKOffscreenCtx*>(context)) {
            fctx->prev_cgl_ctx = CGLGetCurrentContext();
            fctx->saved        = true;
        }
#else
        /* Save the current GLX context so restorePreviousContext() can
         * reinstate it.  This is critical in dual-GL builds: when code
         * running on behalf of an OSMesa render (e.g.
         * SoGLContext_context_max_dimensions) temporarily activates the
         * FLTK window context via this manager, the previous GLX state must
         * be restored afterwards so that subsequent glXMakeCurrent calls
         * for the system GL panel start from a known baseline.  Without this
         * save/restore the CoinPanel context leaks into OSMesa operation
         * windows and can prevent the system GL context from initialising
         * correctly when coin_panel_->make_current() is called later. */
        if (FLTKOffscreenCtx* fctx = static_cast<FLTKOffscreenCtx*>(context)) {
            fctx->prev_glx_ctx  = glXGetCurrentContext();
            fctx->prev_glx_draw = glXGetCurrentDrawable();
            fctx->prev_glx_dpy  = fl_display;
            fctx->saved         = true;
        }
#endif

        win_->make_current();
#if defined(_WIN32)
        if (verbose) {
            HGLRC wglctx = wglGetCurrentContext();
            fprintf(stderr,
                    "[FLTKContextManager::makeContextCurrent #%d]"
                    " after make_current(): wglGetCurrentContext()=%p\n",
                    mcc_count, (void*)wglctx);
            fflush(stderr);
        }
#elif !defined(__APPLE__)
        if (verbose) {
            GLXContext  glxctx  = glXGetCurrentContext();
            GLXDrawable glxdraw = glXGetCurrentDrawable();
            fprintf(stderr,
                    "[FLTKContextManager::makeContextCurrent #%d]"
                    " after make_current():"
                    " glXGetCurrentContext()=%p"
                    " glXGetCurrentDrawable()=0x%lx\n",
                    mcc_count, (void*)glxctx, (unsigned long)glxdraw);
            fflush(stderr);
        }
#endif
        /* Verify the context is actually current after make_current().
         * On some display servers (including Xvfb used in CI) the external
         * Fl_Gl_Window context may not be available at this point – e.g.,
         * glXMakeCurrent() fails silently because the window is not yet
         * fully realised by the X server.  If that happens, fall back to
         * the hidden 1×1 window approach (same as the no-external-window
         * path) so that offscreen FBO rendering can still proceed.
         *
         * Returning FALSE – instead of the previous unconditional TRUE –
         * when the context is genuinely unavailable prevents the segfault
         * in SoOffscreenRenderer::renderFromBase() that occurs when Coin
         * calls glGetString(GL_VERSION) on a NULL context and then passes
         * the result to sscanf(). */
        if (!glGetString(GL_VERSION) && win_ != own_win_) {
            fprintf(stderr,
                    "[FLTKContextManager::makeContextCurrent #%d]"
                    " external window context unavailable after make_current(),"
                    " falling back to hidden window.\n",
                    mcc_count);
            fflush(stderr);
            /* External window failed.  Clear win_ so ensureWindow() will
             * create the hidden 1×1 fallback window on the next call. */
            win_ = nullptr;
            if (!ensureWindow()) {
                reportGL("FLTKContextManager::makeContextCurrent (fallback failed)");
                return FALSE;
            }
            /* ensureWindow() has already called make_current() on own_win_;
             * the context (if available) is already current. */
        }
        reportGL("FLTKContextManager::makeContextCurrent");
        return glGetString(GL_VERSION) ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* context) override {
        /* Restore the platform GL context that was current before
         * makeContextCurrent() was called.  This undoes the platform
         * MakeCurrent call performed by win_->make_current() so that
         * callers using a different GL backend (e.g. OSMesa) are not
         * surprised by the FLTK window context being left active after
         * a temporary context activation.
         *
         * FLTK does not expose a cross-platform "get/restore current
         * context" API — Fl_Gl_Window only provides make_current() on
         * a given window object, with no way to query what context is
         * currently active or to make an arbitrary saved context current
         * again.  We therefore fall through to the native platform API
         * (WGL / CGL / GLX) for the save/restore operation. */
        FLTKOffscreenCtx* fctx = static_cast<FLTKOffscreenCtx*>(context);
        if (!fctx || !fctx->saved) return;
#if defined(_WIN32)
        /* Restore the WGL context.  wglMakeCurrent(nullptr, nullptr)
         * releases any current context when prev_wgl_ctx is null. */
        wglMakeCurrent(fctx->prev_wgl_dc, fctx->prev_wgl_ctx);
#elif defined(__APPLE__)
        /* CGLSetCurrentContext(nullptr) releases the current context when
         * prev_cgl_ctx is null, which is the correct "no context was active
         * before our call" behaviour. */
        CGLSetCurrentContext(fctx->prev_cgl_ctx);
#else
        /* GLX: if the previous context was None (no context was current
         * before our makeContextCurrent call), release the binding
         * entirely.  Otherwise restore the previous context + drawable. */
        if (fctx->prev_glx_dpy) {
            if (fctx->prev_glx_ctx) {
                glXMakeCurrent(fctx->prev_glx_dpy,
                               fctx->prev_glx_draw,
                               fctx->prev_glx_ctx);
            } else {
                glXMakeCurrent(fctx->prev_glx_dpy, None, nullptr);
            }
        }
#endif
        fctx->saved = false;
    }

    virtual void destroyContext(void* context) override {
        delete static_cast<FLTKOffscreenCtx*>(context);
    }

    virtual void* getProcAddress(const char* funcName) override {
#if defined(_WIN32)
        void* p = reinterpret_cast<void*>(wglGetProcAddress(funcName));
        if (!p) {
            HMODULE mod = GetModuleHandleA("opengl32.dll");
            if (mod)
                p = reinterpret_cast<void*>(GetProcAddress(mod, funcName));
        }
        return p;
#elif defined(__APPLE__)
        /* The OpenGL framework is always present on macOS.  The static handle
         * is intentional: dlopen with RTLD_LAZY is idempotent and the
         * framework either exists at this path or it does not – retrying
         * would not help. */
        static void* lib = dlopen(
            "/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
        return lib ? dlsym(lib, funcName) : nullptr;
#else
        return reinterpret_cast<void*>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>(funcName)));
#endif
    }

private:
    /* Active window used by makeContextCurrent() – points to either
     * own_win_ (fallback) or the externally-provided window (preferred). */
    Fl_Gl_Window*        win_;

    /* Hidden 1×1 fallback window created by ensureWindow() when no
     * external window has been registered.  Owned by this manager.
     * Non-null only in fallback mode; null when an external window is used. */
    FLTKGLContextWindow* own_win_;

    /* Ensure a valid GL context window is available.
     *
     * If an external window was registered via setExternalWindow() it is
     * used directly – the caller is responsible for showing it before the
     * first render (main() calls wait_for_expose() after win->show()).
     *
     * Otherwise, fall back to creating a hidden 1×1 FLTKGLContextWindow
     * (original behaviour). */
    bool ensureWindow() {
        static const bool diag = (getenv("OBOL_GL_DIAG") != nullptr);
        if (win_) {
            if (diag) {
                const bool is_own = (win_ == own_win_);
                fprintf(stderr,
                        "[FLTKContextManager::ensureWindow] win_=%p (%s)"
                        " already set, skipping window creation.\n",
                        (void*)win_, is_own ? "own/fallback" : "external");
                fflush(stderr);
            }
            return true;
        }

        /* Fallback: create a hidden 1×1 context window. */
        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] no window set;"
                " creating fallback hidden 1x1 FLTKGLContextWindow.\n");
        fflush(stderr);

        own_win_ = new FLTKGLContextWindow();
        win_     = own_win_;
        /* Position the 1×1 window off-screen.  show() creates the native
         * window handle and the platform GL context without making the
         * window visible to the user. */
        own_win_->position(-100, -100);

        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] calling own_win_->show()"
                " on %p...\n", (void*)own_win_);
        fflush(stderr);
        own_win_->show();
#if !defined(_WIN32) && !defined(__APPLE__)
        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] after show():"
                " fl_xid(own_win_)=0x%lx context=%p valid=%d shown=%d\n",
                (unsigned long)fl_xid(own_win_),
                (void*)own_win_->context(),
                (int)own_win_->valid(),
                (int)own_win_->shown());
        fflush(stderr);
#endif

        /* Flush the FLTK event queue so the native GL context is fully
         * initialised before the first make_current() call. */
        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] calling Fl::check()...\n");
        fflush(stderr);
        Fl::check();
#if !defined(_WIN32) && !defined(__APPLE__)
        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] after check():"
                " fl_xid(own_win_)=0x%lx context=%p valid=%d\n",
                (unsigned long)fl_xid(own_win_),
                (void*)own_win_->context(),
                (int)own_win_->valid());
        fflush(stderr);
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
        /* On X11/GLX, synchronise with the X server before calling
         * make_current().  Fl::check() processes FLTK events but does NOT
         * call XSync, so the X server may not have processed the XMapWindow
         * that show() issued internally.  Without this sync, Mesa's software
         * renderer (used with Xvfb in CI) returns True from glXMakeCurrent
         * but leaves glGetString(GL_VERSION) returning NULL because the
         * window's rendering surface has not been allocated yet.
         *
         * This mirrors the pattern used by headless_utils.h which calls
         * XMapWindow() followed by XSync(dpy, False) before glXCreateContext
         * – the combination that reliably initialises GL on Xvfb/Mesa. */
        if (fl_display) {
            fprintf(stderr,
                    "[FLTKContextManager::ensureWindow] calling"
                    " XSync(fl_display=%p, False)...\n", (void*)fl_display);
            fflush(stderr);
            XSync(fl_display, False);
        }
        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] after XSync():"
                " fl_xid(own_win_)=0x%lx context=%p valid=%d\n",
                (unsigned long)fl_xid(own_win_),
                (void*)own_win_->context(),
                (int)own_win_->valid());
        fflush(stderr);
#endif
        /* Activate the context once to confirm it is valid and to
         * trigger any deferred GL initialisation inside FLTK. */
        fprintf(stderr,
                "[FLTKContextManager::ensureWindow] calling own_win_->make_current()"
                " on %p...\n", (void*)own_win_);
        fflush(stderr);
        own_win_->make_current();
#if !defined(_WIN32) && !defined(__APPLE__)
        {
            GLXContext  glxctx  = glXGetCurrentContext();
            GLXDrawable glxdraw = glXGetCurrentDrawable();
            fprintf(stderr,
                    "[FLTKContextManager::ensureWindow] after make_current():"
                    " glXGetCurrentContext()=%p"
                    " glXGetCurrentDrawable()=0x%lx"
                    " fl_xid(own_win_)=0x%lx\n",
                    (void*)glxctx, (unsigned long)glxdraw,
                    (unsigned long)fl_xid(own_win_));
            fflush(stderr);
        }
#endif
        /* Confirm the fallback hidden-window context is actually active. */
        reportGL("FLTKContextManager::ensureWindow (fallback hidden window)");
        return true;
    }
};

/* =========================================================================
 * Singleton and compatibility helpers
 *
 * These provide the same interface as the headless_utils.h system-GL path
 * so that obol_viewer.cpp requires no conditional logic beyond the header
 * selection guard.
 * ======================================================================= */
namespace {
    /* Meyer's singleton: thread-safe construction (C++11), destroyed at
     * program exit after all static objects in this translation unit. */
    inline FLTKContextManager& fltk_context_manager_singleton() {
        static FLTKContextManager instance;
        return instance;
    }
} // anonymous namespace

/**
 * Initialize Coin database with the FLTK GL context manager.
 * Drop-in replacement for headless_utils.h :: initCoinHeadless() for
 * system-GL builds compiled with -DOBOL_VIEWER_FLTK_GL.
 *
 * Call this once before the FLTK event loop starts.  The actual GL
 * context window is created lazily on the first render request, so
 * Fl::visual() and the main FLTK window do not need to be set up yet.
 */
inline void initCoinHeadless() {
    SoDB::init(&fltk_context_manager_singleton());
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the FLTK context manager registered with SoDB.
 * Drop-in replacement for headless_utils.h :: getCoinHeadlessContextManager().
 * Must be called after initCoinHeadless().
 */
inline SoDB::ContextManager* getCoinHeadlessContextManager() {
    return &fltk_context_manager_singleton();
}

/**
 * Return the single persistent offscreen renderer shared by all callers.
 * Drop-in replacement for headless_utils.h :: getSharedRenderer().
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer* s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(
            &fltk_context_manager_singleton(), vp);
    }
    return s_renderer;
}

#endif /* FLTK_CONTEXT_MANAGER_H */
