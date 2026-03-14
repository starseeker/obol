/*
 * SoDBPortableGL.cpp  –  PortableGL-backed SoDB::ContextManager implementation
 *
 * This translation unit provides:
 *   1. CoinPortableGLContextManagerImpl  – a SoDB::ContextManager that creates
 *      and manages PortableGL offscreen rendering contexts.
 *   2. obol_portablegl_getprocaddress()  – a static name→function-pointer table
 *      used by SoGLContext_getprocaddress() when OBOL_PORTABLEGL_BUILD is set.
 *   3. coin_create_portablegl_context_manager_impl()  – a C entry point called
 *      by SoDB::createPortableGLContextManager() in SoDB.cpp.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * Known obstacles / limitations (see also cmake/FindPortableGL.cmake)
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * GLSL (BLOCKER):  All glShaderSource / glCompileShader / glLinkProgram calls
 *   are no-ops in PortableGL.  Obol generates GLSL programs at runtime for
 *   lighting, materials, shadows, and textures.  These programs will silently
 *   compile to nothing, so rendered output will lack shading.  To obtain
 *   correct output, every GLSL shader in the Obol source would need to be
 *   reimplemented as a pair of PortableGL C vertex/fragment shader functions
 *   and registered via pglCreateProgram().
 *
 * Uniforms (BLOCKER):  glUniform*() and glUniformMatrix*() are all empty
 *   stubs.  Transformation matrices, material properties, and lighting
 *   parameters will not reach any PortableGL shader.
 *
 * FBOs (BLOCKER):  glGenFramebuffers / glBindFramebuffer / glCheckFramebuffer-
 *   Status are empty stubs.  Render-to-texture (SoSceneTexture2), shadow maps,
 *   and any pass that uses a custom FBO will silently fail.
 *
 * glReadPixels (missing):  PortableGL does not implement glReadPixels.
 *   CoinOffscreenGLCanvas calls glReadPixels to extract rendered pixels.  The
 *   portablegl_glReadPixels() wrapper below reads directly from the PortableGL
 *   back-buffer (pglGetBackBuffer()) as a workaround, but only supports
 *   GL_RGBA / GL_UNSIGNED_BYTE for the default framebuffer.
 *
 * Context switching:  PortableGL maintains a single global context pointer `c`.
 *   set_glContext() updates it.  To support multiple simultaneous offscreen
 *   renderers each PGLContextData saves the "previous" context pointer before
 *   making itself current, allowing restorePreviousContext() to reinstate it.
 *   This is single-thread safe; multi-threaded rendering requires a mutex.
 */

#include <Inventor/SoDB.h>
#include <cstring>
#include <memory>

/* portablegl.h is a single-header library.  portablegl_impl.cpp provides the
 * PORTABLEGL_IMPLEMENTATION; here we only need the declarations. */
#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#include <portablegl/portablegl.h>

/* ─────────────────────────────────────────────────────────────────────────── */
/* glReadPixels workaround                                                      */
/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * PortableGL stores the back buffer in PGL_ABGR32 format by default (32-bit
 * pixels, memory order R G B A on little-endian).  CoinOffscreenGLCanvas
 * calls glReadPixels(…, GL_RGBA, GL_UNSIGNED_BYTE, …) expecting bottom-to-top
 * row order (GL convention).  The PortableGL back buffer is stored top-to-bottom
 * (screen convention), so we must flip rows.
 *
 * Limitations of this workaround:
 *   • Only GL_RGBA / GL_UNSIGNED_BYTE is handled; other formats return silently.
 *   • Only the default framebuffer is read; if an FBO was bound (e.g. for
 *     render-to-texture) the result is wrong because FBO stubs are no-ops.
 */
static void portablegl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                    GLenum format, GLenum type, GLvoid* pixels)
{
    if (format != GL_RGBA || type != GL_UNSIGNED_BYTE) {
        /* Unsupported combination – zero the buffer so callers get a defined
         * (if incorrect) result rather than uninitialised memory. */
        if (pixels) memset(pixels, 0, (size_t)width * height * 4);
        return;
    }

    const pix_t* src = static_cast<const pix_t*>(pglGetBackBuffer());
    if (!src || !pixels) return;

    /* PGL_ABGR32 pixel layout (default, LSB architecture):
     *   bits  7-0  : R
     *   bits 15-8  : G
     *   bits 23-16 : B
     *   bits 31-24 : A
     * When read as uint32_t on little-endian the byte order in memory is
     * R G B A, which is exactly GL_RGBA GL_UNSIGNED_BYTE.
     *
     * The PortableGL buffer is stored from top row (y=0) to bottom, but
     * glReadPixels returns rows from bottom (y=0 in GL convention) to top.
     * We flip vertically when copying.
     */
    GLint ctx_width  = 0;
    glGetIntegerv(GL_VIEWPORT, &ctx_width); /* Not ideal – use the back buffer width */
    /* Fall back: just copy naively using the caller-supplied width/height. */
    (void)ctx_width;

    unsigned char* dst = static_cast<unsigned char*>(pixels);
    for (GLsizei row = 0; row < height; ++row) {
        /* Source row in PGL (top-to-bottom): (y_from_top + row)
         * Destination row in GL (bottom-to-top): (height - 1 - row) */
        GLint src_row = y + (height - 1 - row);  /* flip */
        const unsigned char* src_ptr =
            reinterpret_cast<const unsigned char*>(src + src_row * width) + x * 4;
        unsigned char* dst_ptr = dst + (size_t)row * width * 4;
        memcpy(dst_ptr, src_ptr, (size_t)width * 4);
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Static proc-address table                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * PortableGL has no dynamic function-pointer resolver equivalent to
 * OSMesaGetProcAddress() or glXGetProcAddress().  All of its GL functions are
 * directly-linked C symbols.  We expose them through a static name→pointer
 * table so that SoGLContext_getprocaddress() can populate the per-context
 * function-pointer struct that drives Obol's GL dispatch layer.
 *
 * Functions not listed here return NULL from obol_portablegl_getprocaddress(),
 * which the GL dispatch layer treats as "not supported".
 *
 * NOTE: stub functions (FBOs, uniforms, GLSL, etc.) ARE included in this table
 * so that the GL dispatch layer does not fall back to dlsym() and accidentally
 * pick up system-GL symbols.  Returning a no-op pointer is safer than returning
 * a real symbol from an incompatible implementation.
 */
struct PGLProcEntry { const char* name; void* fn; };

static const PGLProcEntry s_pgl_proctable[] = {
    /* Core drawing */
    { "glDrawArrays",             (void*)glDrawArrays             },
    { "glDrawElements",           (void*)glDrawElements           },
    { "glDrawArraysInstanced",    (void*)glDrawArraysInstanced    },
    { "glDrawElementsInstanced",  (void*)glDrawElementsInstanced  },
    { "glMultiDrawArrays",        (void*)glMultiDrawArrays        },
    { "glMultiDrawElements",      (void*)glMultiDrawElements      },

    /* State */
    { "glEnable",                 (void*)glEnable                 },
    { "glDisable",                (void*)glDisable                },
    { "glCullFace",               (void*)glCullFace               },
    { "glFrontFace",              (void*)glFrontFace              },
    { "glPolygonMode",            (void*)glPolygonMode            },
    { "glPolygonOffset",          (void*)glPolygonOffset          },
    { "glPointSize",              (void*)glPointSize              },
    { "glLineWidth",              (void*)glLineWidth              },
    { "glScissor",                (void*)glScissor                },
    { "glViewport",               (void*)glViewport               },
    { "glDepthFunc",              (void*)glDepthFunc              },
    { "glDepthMask",              (void*)glDepthMask              },
    { "glDepthRange",             (void*)glDepthRange             },
    { "glColorMask",              (void*)glColorMask              },
    { "glBlendFunc",              (void*)glBlendFunc              },
    { "glBlendEquation",          (void*)glBlendEquation          },
    { "glBlendFuncSeparate",      (void*)glBlendFuncSeparate      },
    { "glBlendEquationSeparate",  (void*)glBlendEquationSeparate  },
    { "glBlendColor",             (void*)glBlendColor             },
    { "glStencilFunc",            (void*)glStencilFunc            },
    { "glStencilFuncSeparate",    (void*)glStencilFuncSeparate    },
    { "glStencilOp",              (void*)glStencilOp              },
    { "glStencilOpSeparate",      (void*)glStencilOpSeparate      },
    { "glStencilMask",            (void*)glStencilMask            },
    { "glStencilMaskSeparate",    (void*)glStencilMaskSeparate    },
    { "glLogicOp",                (void*)glLogicOp                },
    { "glProvokingVertex",        (void*)glProvokingVertex        },

    /* Clear */
    { "glClear",                  (void*)glClear                  },
    { "glClearColor",             (void*)glClearColor             },
    { "glClearDepth",             (void*)glClearDepth             },
    { "glClearDepthf",            (void*)glClearDepthf            },
    { "glClearStencil",           (void*)glClearStencil           },

    /* Queries */
    { "glGetError",               (void*)glGetError               },
    { "glGetString",              (void*)glGetString              },
    { "glGetStringi",             (void*)glGetStringi             },
    { "glGetBooleanv",            (void*)glGetBooleanv            },
    { "glGetFloatv",              (void*)glGetFloatv              },
    { "glGetIntegerv",            (void*)glGetIntegerv            },
    { "glIsEnabled",              (void*)glIsEnabled              },

    /* Buffers (VBOs) */
    { "glGenBuffers",             (void*)glGenBuffers             },
    { "glDeleteBuffers",          (void*)glDeleteBuffers          },
    { "glBindBuffer",             (void*)glBindBuffer             },
    { "glBufferData",             (void*)glBufferData             },
    { "glBufferSubData",          (void*)glBufferSubData          },
    { "glMapBuffer",              (void*)glMapBuffer              },
    { "glUnmapBuffer",            (void*)glUnmapBuffer            },

    /* Vertex arrays (VAOs) */
    { "glGenVertexArrays",        (void*)glGenVertexArrays        },
    { "glDeleteVertexArrays",     (void*)glDeleteVertexArrays     },
    { "glBindVertexArray",        (void*)glBindVertexArray        },
    { "glVertexAttribPointer",    (void*)glVertexAttribPointer    },
    { "glVertexAttribDivisor",    (void*)glVertexAttribDivisor    },
    { "glEnableVertexAttribArray",(void*)glEnableVertexAttribArray},
    { "glDisableVertexAttribArray",(void*)glDisableVertexAttribArray},

    /* Textures */
    { "glGenTextures",            (void*)glGenTextures            },
    { "glDeleteTextures",         (void*)glDeleteTextures         },
    { "glBindTexture",            (void*)glBindTexture            },
    { "glActiveTexture",          (void*)glActiveTexture          },
    { "glTexParameteri",          (void*)glTexParameteri          },
    { "glTexParameterfv",         (void*)glTexParameterfv         },
    { "glTexParameteriv",         (void*)glTexParameteriv         },
    { "glGetTexParameterfv",      (void*)glGetTexParameterfv      },
    { "glGetTexParameteriv",      (void*)glGetTexParameteriv      },
    { "glTexImage2D",             (void*)glTexImage2D             },
    { "glTexImage3D",             (void*)glTexImage3D             },
    { "glTexSubImage2D",          (void*)glTexSubImage2D          },
    { "glTexSubImage3D",          (void*)glTexSubImage3D          },
    { "glPixelStorei",            (void*)glPixelStorei            },
    { "glGenerateMipmap",         (void*)glGenerateMipmap         },

    /* Pixel read – PortableGL has no glReadPixels, use our wrapper */
    { "glReadPixels",             (void*)portablegl_glReadPixels  },

    /* Shaders – all stubs in PortableGL; included so dispatch does not fall  */
    /* back to dlsym() and accidentally pick up system-GL symbols.            */
    { "glCreateProgram",          (void*)glCreateProgram          },
    { "glDeleteProgram",          (void*)glDeleteProgram          },
    { "glUseProgram",             (void*)glUseProgram             },
    { "glCreateShader",           (void*)glCreateShader           },
    { "glDeleteShader",           (void*)glDeleteShader           },
    { "glShaderSource",           (void*)glShaderSource           },
    { "glCompileShader",          (void*)glCompileShader          },
    { "glAttachShader",           (void*)glAttachShader           },
    { "glDetachShader",           (void*)glDetachShader           },
    { "glLinkProgram",            (void*)glLinkProgram            },
    { "glGetProgramiv",           (void*)glGetProgramiv           },
    { "glGetProgramInfoLog",      (void*)glGetProgramInfoLog      },
    { "glGetShaderiv",            (void*)glGetShaderiv            },
    { "glGetShaderInfoLog",       (void*)glGetShaderInfoLog       },
    { "glGetUniformLocation",     (void*)glGetUniformLocation     },
    { "glGetAttribLocation",      (void*)glGetAttribLocation      },

    /* Uniforms – stubs */
    { "glUniform1f",              (void*)glUniform1f              },
    { "glUniform2f",              (void*)glUniform2f              },
    { "glUniform3f",              (void*)glUniform3f              },
    { "glUniform4f",              (void*)glUniform4f              },
    { "glUniform1i",              (void*)glUniform1i              },
    { "glUniform2i",              (void*)glUniform2i              },
    { "glUniform3i",              (void*)glUniform3i              },
    { "glUniform4i",              (void*)glUniform4i              },
    { "glUniform1fv",             (void*)glUniform1fv             },
    { "glUniform2fv",             (void*)glUniform2fv             },
    { "glUniform3fv",             (void*)glUniform3fv             },
    { "glUniform4fv",             (void*)glUniform4fv             },
    { "glUniform1iv",             (void*)glUniform1iv             },
    { "glUniform2iv",             (void*)glUniform2iv             },
    { "glUniform3iv",             (void*)glUniform3iv             },
    { "glUniform4iv",             (void*)glUniform4iv             },
    { "glUniformMatrix2fv",       (void*)glUniformMatrix2fv       },
    { "glUniformMatrix3fv",       (void*)glUniformMatrix3fv       },
    { "glUniformMatrix4fv",       (void*)glUniformMatrix4fv       },
    { "glUniformMatrix3x4fv",     (void*)glUniformMatrix3x4fv     },
    { "glUniformMatrix4x3fv",     (void*)glUniformMatrix4x3fv     },

    /* Framebuffer objects – stubs */
    { "glGenFramebuffers",        (void*)glGenFramebuffers        },
    { "glBindFramebuffer",        (void*)glBindFramebuffer        },
    { "glDeleteFramebuffers",     (void*)glDeleteFramebuffers     },
    { "glFramebufferTexture2D",   (void*)glFramebufferTexture2D   },
    { "glFramebufferTexture3D",   (void*)glFramebufferTexture3D   },
    { "glFramebufferRenderbuffer",(void*)glFramebufferRenderbuffer},
    { "glCheckFramebufferStatus", (void*)glCheckFramebufferStatus },
    { "glIsFramebuffer",          (void*)glIsFramebuffer          },
    { "glBlitFramebuffer",        (void*)glBlitFramebuffer        },

    /* Renderbuffers – stubs */
    { "glGenRenderbuffers",       (void*)glGenRenderbuffers       },
    { "glBindRenderbuffer",       (void*)glBindRenderbuffer       },
    { "glDeleteRenderbuffers",    (void*)glDeleteRenderbuffers    },
    { "glRenderbufferStorage",    (void*)glRenderbufferStorage    },
    { "glIsRenderbuffer",         (void*)glIsRenderbuffer         },

    /* Misc stubs */
    { "glDrawBuffers",            (void*)glDrawBuffers            },
    { "glReadBuffer",             (void*)glReadBuffer             },
    { "glGenerateMipmap",         (void*)glGenerateMipmap         },

    /* End sentinel */
    { nullptr, nullptr }
};

extern "C"
void* obol_portablegl_getprocaddress(const char* name)
{
    for (int i = 0; s_pgl_proctable[i].name != nullptr; ++i) {
        if (strcmp(s_pgl_proctable[i].name, name) == 0)
            return s_pgl_proctable[i].fn;
    }
    return nullptr;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Per-context state                                                            */
/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * We track the "currently active" PortableGL context ourselves because
 * PortableGL stores its current-context pointer in a static variable inside
 * the implementation and provides no public getter.  Each context data record
 * saves the previously-active context pointer before making itself current so
 * that restorePreviousContext() can reinstate the prior state.
 */
static glContext* s_current_pgl_context = nullptr;

struct PGLContextData {
    glContext pgl_ctx;
    pix_t*    backbuf;
    int       width;
    int       height;
    /* The context that was active when makeContextCurrent() was last called
     * on THIS context, saved so restorePreviousContext() can put it back. */
    glContext* saved_prev;

    PGLContextData(int w, int h)
        : backbuf(nullptr), width(w), height(h), saved_prev(nullptr)
    {}

    ~PGLContextData() {
        /* If this context is currently active, deactivate it. */
        if (s_current_pgl_context == &pgl_ctx) {
            set_glContext(nullptr);
            s_current_pgl_context = nullptr;
        }
        free_glContext(&pgl_ctx);
        /* backbuf is owned and freed by free_glContext */
    }

    bool init() {
        return init_glContext(&pgl_ctx, &backbuf, width, height) != GL_FALSE;
    }

    void makeCurrent() {
        saved_prev = s_current_pgl_context;
        s_current_pgl_context = &pgl_ctx;
        set_glContext(&pgl_ctx);
    }

    void restorePrev() {
        s_current_pgl_context = saved_prev;
        set_glContext(saved_prev);
        saved_prev = nullptr;
    }
};

/* ─────────────────────────────────────────────────────────────────────────── */
/* SoDB::ContextManager implementation                                         */
/* ─────────────────────────────────────────────────────────────────────────── */

class CoinPortableGLContextManagerImpl : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* d = new PGLContextData((int)width, (int)height);
        if (!d->init()) {
            delete d;
            return nullptr;
        }
        return d;
    }

    SbBool isOSMesaContext(void * /*context*/) override {
        /* PortableGL contexts are not OSMesa contexts – return FALSE so
         * CoinOffscreenGLCanvas does not attempt to register them as OSMesa
         * contexts in the dual-GL dispatch table. */
        return FALSE;
    }

    void maxOffscreenDimensions(unsigned int & width, unsigned int & height) const override {
        /* PortableGL renders on the CPU into a heap-allocated buffer.  The
         * practical limit is available memory; we cap at 16384 like OSMesa. */
        width  = 16384;
        height = 16384;
    }

    SbBool makeContextCurrent(void * context) override {
        if (!context) return FALSE;
        static_cast<PGLContextData*>(context)->makeCurrent();
        return TRUE;
    }

    void restorePreviousContext(void * context) override {
        if (!context) return;
        static_cast<PGLContextData*>(context)->restorePrev();
    }

    void destroyContext(void * context) override {
        delete static_cast<PGLContextData*>(context);
    }

    void * getProcAddress(const char * funcName) override {
        return obol_portablegl_getprocaddress(funcName);
    }
};

/* ─────────────────────────────────────────────────────────────────────────── */
/* C entry point called by SoDB::createPortableGLContextManager()              */
/* ─────────────────────────────────────────────────────────────────────────── */

extern "C" {
SoDB::ContextManager * coin_create_portablegl_context_manager_impl();
SoDB::ContextManager * coin_create_portablegl_context_manager_impl()
{
    return new CoinPortableGLContextManagerImpl();
}
} /* extern "C" */
