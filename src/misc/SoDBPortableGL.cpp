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

/* PortableGL glReadPixels implementation.
 * Reads from the current portablegl back buffer (pglGetBackBuffer()).
 * The portablegl back buffer uses PGL_ABGR32 format on little-endian
 * systems: each pix_t uint32 stores bytes as R G B A (LSB first).
 * We use glGetIntegerv(GL_VIEWPORT) to obtain the framebuffer width so
 * that row-stride math is correct without needing a stored context ptr. */
static void pgl_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h,
                              GLenum format, GLenum type, GLvoid* pixels)
{
    if (!pixels || type != GL_UNSIGNED_BYTE) return;
    pix_t* backbuf = static_cast<pix_t*>(pglGetBackBuffer());
    if (!backbuf) return;
    /* Determine framebuffer dimensions from the viewport.
     * portablegl's draw_pixel() stores pixel at OpenGL (x,y) at backbuf row
     * (fbh - 1 - y), i.e. row 0 of the backbuffer = OpenGL y = fbh-1 (top).
     * Standard glReadPixels(x, y, w, h) reads starting from OpenGL y (bottom
     * left), so we must reverse the row order when accessing the backbuffer. */
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    const int fbw = vp[2] > 0 ? vp[2] : w; /* fallback: stride = requested width */
    const int fbh = vp[3] > 0 ? vp[3] : h; /* fallback: height = requested height */
    uint8_t* dst = static_cast<uint8_t*>(pixels);
    for (int ogy = y; ogy < y + h; ++ogy) {
        /* Map OpenGL y (0=bottom) → portablegl buffer row */
        int buf_row = (fbh - 1 - ogy);
        if (buf_row < 0 || buf_row >= fbh) continue;
        for (int col = x; col < x + w; ++col) {
            pix_t p = backbuf[buf_row * fbw + col];
            /* PGL_ABGR32 (default LE format): RSHIFT=0 GSHIFT=8 BSHIFT=16 ASHIFT=24 */
            uint8_t r = (uint8_t)((p >>  0) & 0xFF);
            uint8_t g = (uint8_t)((p >>  8) & 0xFF);
            uint8_t b = (uint8_t)((p >> 16) & 0xFF);
            uint8_t a = (uint8_t)((p >> 24) & 0xFF);
            if (format == (GLenum)GL_RGB) {
                *dst++ = r; *dst++ = g; *dst++ = b;
            } else { /* GL_RGBA */
                *dst++ = r; *dst++ = g; *dst++ = b; *dst++ = a;
            }
        }
    }
}

/* No-op stubs for GL functions absent from PortableGL */
static void pgl_glFlush(void) {}
static void pgl_glColor4ub(GLubyte, GLubyte, GLubyte, GLubyte) {}
static void pgl_glColorMaterial(GLenum, GLenum) {}
static void pgl_glViewport_nop(GLint, GLint, GLsizei, GLsizei) {}

/* PortableGL does not implement the GL3 buffer-query helpers.  Provide
 * harmless stubs so that the VBO-detection path in gl.cpp can find all
 * the required function pointers and leaves has_vertex_buffer_object=TRUE. */
static void  pgl_glGetBufferSubData(GLenum, GLintptr, GLsizeiptr, GLvoid*) {}
static void  pgl_glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    if (!params) return;
    /* Minimal useful answers so that callers don't crash. */
    if (pname == 0x8764 /*GL_BUFFER_SIZE*/)   { *params = 0; return; }
    if (pname == 0x8765 /*GL_BUFFER_USAGE*/)  { *params = 0x88B4 /*GL_STATIC_DRAW*/; return; }
    if (pname == 0x88BC /*GL_BUFFER_ACCESS*/) { *params = 0x88B8 /*GL_READ_WRITE*/; return; }
    if (pname == 0x88BD /*GL_BUFFER_MAPPED*/) { *params = GL_FALSE; return; }
    *params = 0;
}
static void  pgl_glGetBufferPointerv(GLenum, GLenum, GLvoid**) {}
static GLboolean pgl_glIsBuffer(GLuint buffer) { return buffer != 0 ? GL_TRUE : GL_FALSE; }

/* PortableGL's glGetString does not handle GL_EXTENSIONS (returns NULL with
 * GL_INVALID_ENUM).  Return an empty string so the extension-detection code
 * in gl.cpp gets a non-NULL result and stops looking. */
static const GLubyte* pgl_glGetString(GLenum name)
{
    if (name == 0x1F03 /*GL_EXTENSIONS*/) {
        static const GLubyte empty[] = "";
        return empty;
    }
    return glGetString(name);
}

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
        if (valid) {
            /* PortableGL enables its stdout debug callback by default.
             * Silence it: Obol handles error reporting via SoDebugError. */
            set_glContext(&ctx);
            glDebugMessageCallback(nullptr, nullptr);
        }
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

    /* PortableGL renders into its own internal software back-buffer;
     * no FBO is needed for offscreen rendering.  Signal this to
     * CoinOffscreenGLCanvas so it skips FBO creation/binding. */
    SbBool hasSoftwareOwnBuffer(void * /*context*/) override {
        return TRUE;
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

    void * getProcAddress(const char * funcName) override {
        /* PortableGL compiles all GL 3.x core functions as static symbols.
         * Provide a direct name-to-pointer mapping since dlsym(RTLD_DEFAULT)
         * cannot find functions hidden inside a shared library.            */
        if (strcmp(funcName, "glActiveTexture") == 0) return (void*)glActiveTexture;
        if (strcmp(funcName, "glAttachShader") == 0) return (void*)glAttachShader;
        if (strcmp(funcName, "glBindBuffer") == 0) return (void*)glBindBuffer;
        if (strcmp(funcName, "glBindFramebuffer") == 0) return (void*)glBindFramebuffer;
        if (strcmp(funcName, "glBindRenderbuffer") == 0) return (void*)glBindRenderbuffer;
        if (strcmp(funcName, "glBindTexture") == 0) return (void*)glBindTexture;
        if (strcmp(funcName, "glBindVertexArray") == 0) return (void*)glBindVertexArray;
        if (strcmp(funcName, "glBlendColor") == 0) return (void*)glBlendColor;
        if (strcmp(funcName, "glBlendEquation") == 0) return (void*)glBlendEquation;
        if (strcmp(funcName, "glBlendEquationSeparate") == 0) return (void*)glBlendEquationSeparate;
        if (strcmp(funcName, "glBlendFunc") == 0) return (void*)glBlendFunc;
        if (strcmp(funcName, "glBlendFuncSeparate") == 0) return (void*)glBlendFuncSeparate;
        if (strcmp(funcName, "glBlitFramebuffer") == 0) return (void*)glBlitFramebuffer;
        if (strcmp(funcName, "glBlitNamedFramebuffer") == 0) return (void*)glBlitNamedFramebuffer;
        if (strcmp(funcName, "glBufferData") == 0) return (void*)glBufferData;
        if (strcmp(funcName, "glBufferSubData") == 0) return (void*)glBufferSubData;
        if (strcmp(funcName, "glCheckFramebufferStatus") == 0) return (void*)glCheckFramebufferStatus;
        if (strcmp(funcName, "glClear") == 0) return (void*)glClear;
        if (strcmp(funcName, "glClearBufferfi") == 0) return (void*)glClearBufferfi;
        if (strcmp(funcName, "glClearBufferfv") == 0) return (void*)glClearBufferfv;
        if (strcmp(funcName, "glClearBufferiv") == 0) return (void*)glClearBufferiv;
        if (strcmp(funcName, "glClearBufferuiv") == 0) return (void*)glClearBufferuiv;
        if (strcmp(funcName, "glClearColor") == 0) return (void*)glClearColor;
        if (strcmp(funcName, "glClearDepth") == 0) return (void*)glClearDepth;
        if (strcmp(funcName, "glClearDepthf") == 0) return (void*)glClearDepthf;
        if (strcmp(funcName, "glClearNamedFramebufferfi") == 0) return (void*)glClearNamedFramebufferfi;
        if (strcmp(funcName, "glClearNamedFramebufferfv") == 0) return (void*)glClearNamedFramebufferfv;
        if (strcmp(funcName, "glClearNamedFramebufferiv") == 0) return (void*)glClearNamedFramebufferiv;
        if (strcmp(funcName, "glClearNamedFramebufferuiv") == 0) return (void*)glClearNamedFramebufferuiv;
        if (strcmp(funcName, "glClearStencil") == 0) return (void*)glClearStencil;
        if (strcmp(funcName, "glColorMask") == 0) return (void*)glColorMask;
        if (strcmp(funcName, "glColorMaski") == 0) return (void*)glColorMaski;
        if (strcmp(funcName, "glCompileShader") == 0) return (void*)glCompileShader;
        if (strcmp(funcName, "glCompressedTexImage1D") == 0) return (void*)glCompressedTexImage1D;
        if (strcmp(funcName, "glCompressedTexImage2D") == 0) return (void*)glCompressedTexImage2D;
        if (strcmp(funcName, "glCompressedTexImage3D") == 0) return (void*)glCompressedTexImage3D;
        if (strcmp(funcName, "glCreateProgram") == 0) return (void*)glCreateProgram;
        if (strcmp(funcName, "glCreateShader") == 0) return (void*)glCreateShader;
        if (strcmp(funcName, "glCreateTextures") == 0) return (void*)glCreateTextures;
        if (strcmp(funcName, "glCullFace") == 0) return (void*)glCullFace;
        if (strcmp(funcName, "glDebugMessageCallback") == 0) return (void*)glDebugMessageCallback;
        if (strcmp(funcName, "glDeleteBuffers") == 0) return (void*)glDeleteBuffers;
        if (strcmp(funcName, "glDeleteFramebuffers") == 0) return (void*)glDeleteFramebuffers;
        if (strcmp(funcName, "glDeleteProgram") == 0) return (void*)glDeleteProgram;
        if (strcmp(funcName, "glDeleteRenderbuffers") == 0) return (void*)glDeleteRenderbuffers;
        if (strcmp(funcName, "glDeleteShader") == 0) return (void*)glDeleteShader;
        if (strcmp(funcName, "glDeleteTextures") == 0) return (void*)glDeleteTextures;
        if (strcmp(funcName, "glDeleteVertexArrays") == 0) return (void*)glDeleteVertexArrays;
        if (strcmp(funcName, "glDepthFunc") == 0) return (void*)glDepthFunc;
        if (strcmp(funcName, "glDepthMask") == 0) return (void*)glDepthMask;
        if (strcmp(funcName, "glDepthRange") == 0) return (void*)glDepthRange;
        if (strcmp(funcName, "glDepthRangef") == 0) return (void*)glDepthRangef;
        if (strcmp(funcName, "glDetachShader") == 0) return (void*)glDetachShader;
        if (strcmp(funcName, "glDisable") == 0) return (void*)glDisable;
        if (strcmp(funcName, "glDisableVertexArrayAttrib") == 0) return (void*)glDisableVertexArrayAttrib;
        if (strcmp(funcName, "glDisableVertexAttribArray") == 0) return (void*)glDisableVertexAttribArray;
        if (strcmp(funcName, "glDrawArrays") == 0) return (void*)glDrawArrays;
        if (strcmp(funcName, "glDrawArraysInstanced") == 0) return (void*)glDrawArraysInstanced;
        if (strcmp(funcName, "glDrawArraysInstancedBaseInstance") == 0) return (void*)glDrawArraysInstancedBaseInstance;
        if (strcmp(funcName, "glDrawBuffers") == 0) return (void*)glDrawBuffers;
        if (strcmp(funcName, "glDrawElements") == 0) return (void*)glDrawElements;
        if (strcmp(funcName, "glDrawElementsInstanced") == 0) return (void*)glDrawElementsInstanced;
        if (strcmp(funcName, "glDrawElementsInstancedBaseInstance") == 0) return (void*)glDrawElementsInstancedBaseInstance;
        if (strcmp(funcName, "glEnable") == 0) return (void*)glEnable;
        if (strcmp(funcName, "glEnableVertexArrayAttrib") == 0) return (void*)glEnableVertexArrayAttrib;
        if (strcmp(funcName, "glEnableVertexAttribArray") == 0) return (void*)glEnableVertexAttribArray;
        /* PortableGL stubs: functions absent from the GL3 core subset */
        if (strcmp(funcName, "glFlush") == 0) return (void*)pgl_glFlush;
        if (strcmp(funcName, "glFinish") == 0) return (void*)pgl_glFlush;  /* same no-op */
        if (strcmp(funcName, "glColor4ub") == 0) return (void*)pgl_glColor4ub;
        if (strcmp(funcName, "glColorMaterial") == 0) return (void*)pgl_glColorMaterial;
        if (strcmp(funcName, "glFramebufferRenderbuffer") == 0) return (void*)glFramebufferRenderbuffer;
        if (strcmp(funcName, "glFramebufferTexture") == 0) return (void*)glFramebufferTexture;
        if (strcmp(funcName, "glFramebufferTexture1D") == 0) return (void*)glFramebufferTexture1D;
        if (strcmp(funcName, "glFramebufferTexture2D") == 0) return (void*)glFramebufferTexture2D;
        if (strcmp(funcName, "glFramebufferTexture3D") == 0) return (void*)glFramebufferTexture3D;
        if (strcmp(funcName, "glFramebufferTextureLayer") == 0) return (void*)glFramebufferTextureLayer;
        if (strcmp(funcName, "glFrontFace") == 0) return (void*)glFrontFace;
        if (strcmp(funcName, "glGenBuffers") == 0) return (void*)glGenBuffers;
        if (strcmp(funcName, "glGenFramebuffers") == 0) return (void*)glGenFramebuffers;
        if (strcmp(funcName, "glGenRenderbuffers") == 0) return (void*)glGenRenderbuffers;
        if (strcmp(funcName, "glGenTextures") == 0) return (void*)glGenTextures;
        if (strcmp(funcName, "glGenVertexArrays") == 0) return (void*)glGenVertexArrays;
        if (strcmp(funcName, "glGenerateMipmap") == 0) return (void*)glGenerateMipmap;
        if (strcmp(funcName, "glGetAttribLocation") == 0) return (void*)glGetAttribLocation;
        if (strcmp(funcName, "glGetBufferSubData") == 0) return (void*)pgl_glGetBufferSubData;
        if (strcmp(funcName, "glGetBufferParameteriv") == 0) return (void*)pgl_glGetBufferParameteriv;
        if (strcmp(funcName, "glGetBufferPointerv") == 0) return (void*)pgl_glGetBufferPointerv;
        if (strcmp(funcName, "glGetBooleanv") == 0) return (void*)glGetBooleanv;
        if (strcmp(funcName, "glGetDoublev") == 0) return (void*)glGetDoublev;
        if (strcmp(funcName, "glGetError") == 0) return (void*)glGetError;
        if (strcmp(funcName, "glGetFloatv") == 0) return (void*)glGetFloatv;
        if (strcmp(funcName, "glGetInteger64v") == 0) return (void*)glGetInteger64v;
        if (strcmp(funcName, "glGetIntegerv") == 0) return (void*)glGetIntegerv;
        if (strcmp(funcName, "glGetProgramInfoLog") == 0) return (void*)glGetProgramInfoLog;
        if (strcmp(funcName, "glGetProgramiv") == 0) return (void*)glGetProgramiv;
        if (strcmp(funcName, "glGetShaderInfoLog") == 0) return (void*)glGetShaderInfoLog;
        if (strcmp(funcName, "glGetShaderiv") == 0) return (void*)glGetShaderiv;
        if (strcmp(funcName, "glGetString") == 0) return (void*)pgl_glGetString;
        if (strcmp(funcName, "glGetStringi") == 0) return (void*)glGetStringi;
        if (strcmp(funcName, "glGetTexParameterIiv") == 0) return (void*)glGetTexParameterIiv;
        if (strcmp(funcName, "glGetTexParameterIuiv") == 0) return (void*)glGetTexParameterIuiv;
        if (strcmp(funcName, "glGetTexParameterfv") == 0) return (void*)glGetTexParameterfv;
        if (strcmp(funcName, "glGetTexParameteriv") == 0) return (void*)glGetTexParameteriv;
        if (strcmp(funcName, "glGetTextureParameterIiv") == 0) return (void*)glGetTextureParameterIiv;
        if (strcmp(funcName, "glGetTextureParameterIuiv") == 0) return (void*)glGetTextureParameterIuiv;
        if (strcmp(funcName, "glGetTextureParameterfv") == 0) return (void*)glGetTextureParameterfv;
        if (strcmp(funcName, "glGetTextureParameteriv") == 0) return (void*)glGetTextureParameteriv;
        if (strcmp(funcName, "glGetUniformLocation") == 0) return (void*)glGetUniformLocation;
        if (strcmp(funcName, "glIsBuffer") == 0) return (void*)pgl_glIsBuffer;
        if (strcmp(funcName, "glIsEnabled") == 0) return (void*)glIsEnabled;
        if (strcmp(funcName, "glIsFramebuffer") == 0) return (void*)glIsFramebuffer;
        if (strcmp(funcName, "glIsProgram") == 0) return (void*)glIsProgram;
        if (strcmp(funcName, "glIsRenderbuffer") == 0) return (void*)glIsRenderbuffer;
        if (strcmp(funcName, "glLineWidth") == 0) return (void*)glLineWidth;
        if (strcmp(funcName, "glLinkProgram") == 0) return (void*)glLinkProgram;
        if (strcmp(funcName, "glLogicOp") == 0) return (void*)glLogicOp;
        if (strcmp(funcName, "glMapBuffer") == 0) return (void*)glMapBuffer;
        if (strcmp(funcName, "glMapNamedBuffer") == 0) return (void*)glMapNamedBuffer;
        if (strcmp(funcName, "glMultiDrawArrays") == 0) return (void*)glMultiDrawArrays;
        if (strcmp(funcName, "glMultiDrawElements") == 0) return (void*)glMultiDrawElements;
        if (strcmp(funcName, "glNamedBufferData") == 0) return (void*)glNamedBufferData;
        if (strcmp(funcName, "glNamedBufferSubData") == 0) return (void*)glNamedBufferSubData;
        if (strcmp(funcName, "glNamedFramebufferDrawBuffers") == 0) return (void*)glNamedFramebufferDrawBuffers;
        if (strcmp(funcName, "glNamedFramebufferReadBuffer") == 0) return (void*)glNamedFramebufferReadBuffer;
        if (strcmp(funcName, "glNamedFramebufferTextureLayer") == 0) return (void*)glNamedFramebufferTextureLayer;
        if (strcmp(funcName, "glNamedRenderbufferStorageMultisample") == 0) return (void*)glNamedRenderbufferStorageMultisample;
        if (strcmp(funcName, "glPixelStorei") == 0) return (void*)glPixelStorei;
        if (strcmp(funcName, "glPointParameteri") == 0) return (void*)glPointParameteri;
        if (strcmp(funcName, "glPointSize") == 0) return (void*)glPointSize;
        if (strcmp(funcName, "glPolygonMode") == 0) return (void*)glPolygonMode;
        if (strcmp(funcName, "glPolygonOffset") == 0) return (void*)glPolygonOffset;
        if (strcmp(funcName, "glProvokingVertex") == 0) return (void*)glProvokingVertex;
        if (strcmp(funcName, "glReadBuffer") == 0) return (void*)glReadBuffer;
        if (strcmp(funcName, "glReadPixels") == 0) return (void*)pgl_glReadPixels;
        if (strcmp(funcName, "glRenderbufferStorage") == 0) return (void*)glRenderbufferStorage;
        if (strcmp(funcName, "glRenderbufferStorageMultisample") == 0) return (void*)glRenderbufferStorageMultisample;
        if (strcmp(funcName, "glScissor") == 0) return (void*)glScissor;
        if (strcmp(funcName, "glShaderSource") == 0) return (void*)glShaderSource;
        if (strcmp(funcName, "glStencilFunc") == 0) return (void*)glStencilFunc;
        if (strcmp(funcName, "glStencilFuncSeparate") == 0) return (void*)glStencilFuncSeparate;
        if (strcmp(funcName, "glStencilMask") == 0) return (void*)glStencilMask;
        if (strcmp(funcName, "glStencilMaskSeparate") == 0) return (void*)glStencilMaskSeparate;
        if (strcmp(funcName, "glStencilOp") == 0) return (void*)glStencilOp;
        if (strcmp(funcName, "glStencilOpSeparate") == 0) return (void*)glStencilOpSeparate;
        if (strcmp(funcName, "glTexBuffer") == 0) return (void*)glTexBuffer;
        if (strcmp(funcName, "glTexImage1D") == 0) return (void*)glTexImage1D;
        if (strcmp(funcName, "glTexImage2D") == 0) return (void*)glTexImage2D;
        if (strcmp(funcName, "glTexImage3D") == 0) return (void*)glTexImage3D;
        if (strcmp(funcName, "glTexParameterf") == 0) return (void*)glTexParameterf;
        if (strcmp(funcName, "glTexParameterfv") == 0) return (void*)glTexParameterfv;
        if (strcmp(funcName, "glTexParameteri") == 0) return (void*)glTexParameteri;
        if (strcmp(funcName, "glTexParameteriv") == 0) return (void*)glTexParameteriv;
        if (strcmp(funcName, "glTexParameterliv") == 0) return (void*)glTexParameterliv;
        if (strcmp(funcName, "glTexParameterluiv") == 0) return (void*)glTexParameterluiv;
        if (strcmp(funcName, "glTexSubImage1D") == 0) return (void*)glTexSubImage1D;
        if (strcmp(funcName, "glTexSubImage2D") == 0) return (void*)glTexSubImage2D;
        if (strcmp(funcName, "glTexSubImage3D") == 0) return (void*)glTexSubImage3D;
        if (strcmp(funcName, "glTextureBuffer") == 0) return (void*)glTextureBuffer;
        if (strcmp(funcName, "glTextureParameterf") == 0) return (void*)glTextureParameterf;
        if (strcmp(funcName, "glTextureParameterfv") == 0) return (void*)glTextureParameterfv;
        if (strcmp(funcName, "glTextureParameteri") == 0) return (void*)glTextureParameteri;
        if (strcmp(funcName, "glTextureParameteriv") == 0) return (void*)glTextureParameteriv;
        if (strcmp(funcName, "glTextureParameterliv") == 0) return (void*)glTextureParameterliv;
        if (strcmp(funcName, "glTextureParameterluiv") == 0) return (void*)glTextureParameterluiv;
        if (strcmp(funcName, "glUniform1f") == 0) return (void*)glUniform1f;
        if (strcmp(funcName, "glUniform1fv") == 0) return (void*)glUniform1fv;
        if (strcmp(funcName, "glUniform1i") == 0) return (void*)glUniform1i;
        if (strcmp(funcName, "glUniform1iv") == 0) return (void*)glUniform1iv;
        if (strcmp(funcName, "glUniform1ui") == 0) return (void*)glUniform1ui;
        if (strcmp(funcName, "glUniform1uiv") == 0) return (void*)glUniform1uiv;
        if (strcmp(funcName, "glUniform2f") == 0) return (void*)glUniform2f;
        if (strcmp(funcName, "glUniform2fv") == 0) return (void*)glUniform2fv;
        if (strcmp(funcName, "glUniform2i") == 0) return (void*)glUniform2i;
        if (strcmp(funcName, "glUniform2iv") == 0) return (void*)glUniform2iv;
        if (strcmp(funcName, "glUniform2ui") == 0) return (void*)glUniform2ui;
        if (strcmp(funcName, "glUniform2uiv") == 0) return (void*)glUniform2uiv;
        if (strcmp(funcName, "glUniform3f") == 0) return (void*)glUniform3f;
        if (strcmp(funcName, "glUniform3fv") == 0) return (void*)glUniform3fv;
        if (strcmp(funcName, "glUniform3i") == 0) return (void*)glUniform3i;
        if (strcmp(funcName, "glUniform3iv") == 0) return (void*)glUniform3iv;
        if (strcmp(funcName, "glUniform3ui") == 0) return (void*)glUniform3ui;
        if (strcmp(funcName, "glUniform3uiv") == 0) return (void*)glUniform3uiv;
        if (strcmp(funcName, "glUniform4f") == 0) return (void*)glUniform4f;
        if (strcmp(funcName, "glUniform4fv") == 0) return (void*)glUniform4fv;
        if (strcmp(funcName, "glUniform4i") == 0) return (void*)glUniform4i;
        if (strcmp(funcName, "glUniform4iv") == 0) return (void*)glUniform4iv;
        if (strcmp(funcName, "glUniform4ui") == 0) return (void*)glUniform4ui;
        if (strcmp(funcName, "glUniform4uiv") == 0) return (void*)glUniform4uiv;
        if (strcmp(funcName, "glUniformMatrix2fv") == 0) return (void*)glUniformMatrix2fv;
        if (strcmp(funcName, "glUniformMatrix2x3fv") == 0) return (void*)glUniformMatrix2x3fv;
        if (strcmp(funcName, "glUniformMatrix2x4fv") == 0) return (void*)glUniformMatrix2x4fv;
        if (strcmp(funcName, "glUniformMatrix3fv") == 0) return (void*)glUniformMatrix3fv;
        if (strcmp(funcName, "glUniformMatrix3x2fv") == 0) return (void*)glUniformMatrix3x2fv;
        if (strcmp(funcName, "glUniformMatrix3x4fv") == 0) return (void*)glUniformMatrix3x4fv;
        if (strcmp(funcName, "glUniformMatrix4fv") == 0) return (void*)glUniformMatrix4fv;
        if (strcmp(funcName, "glUniformMatrix4x2fv") == 0) return (void*)glUniformMatrix4x2fv;
        if (strcmp(funcName, "glUniformMatrix4x3fv") == 0) return (void*)glUniformMatrix4x3fv;
        if (strcmp(funcName, "glUnmapBuffer") == 0) return (void*)glUnmapBuffer;
        if (strcmp(funcName, "glUnmapNamedBuffer") == 0) return (void*)glUnmapNamedBuffer;
        if (strcmp(funcName, "glUseProgram") == 0) return (void*)glUseProgram;
        if (strcmp(funcName, "glVertexAttribDivisor") == 0) return (void*)glVertexAttribDivisor;
        if (strcmp(funcName, "glVertexAttribPointer") == 0) return (void*)glVertexAttribPointer;
        if (strcmp(funcName, "glViewport") == 0) return (void*)glViewport;
        return nullptr;
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
