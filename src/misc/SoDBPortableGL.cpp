/*
 * SoDBPortableGL.cpp  –  PortableGL-backed SoDB::ContextManager
 *
 * This translation unit provides:
 *   1. CoinPortableGLContextManagerImpl  – a SoDB::ContextManager that creates
 *      and manages PortableGL offscreen rendering contexts, including wiring up
 *      the per-context ObolPGLCompatState used by the shader adapter layer.
 *   2. obol_portablegl_getprocaddress()  – a comprehensive name→function-pointer
 *      table that routes all Obol GL calls through interceptors defined in
 *      portablegl_compat_funcs.cpp and portablegl_shader_registry.cpp.
 *   3. coin_create_portablegl_context_manager_impl()  – C entry point called by
 *      SoDB::createPortableGLContextManager() in SoDB.cpp.
 *
 * Shader adaptation
 * ─────────────────
 * Obol's GLSL shaders use OpenGL compatibility-profile built-ins
 * (gl_ModelViewMatrix, gl_LightSource[], gl_FrontMaterial …).  PortableGL
 * cannot compile GLSL; instead this file wires a full compatibility-profile
 * state layer (ObolPGLCompatState) and pre-written C-function shaders
 * (portablegl_obol_shaders.h) that faithfully implement Obol's shader logic.
 *
 * See portablegl_compat_funcs.cpp and portablegl_shader_registry.cpp for the
 * interceptor implementations and shader registry respectively.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * Remaining limitations (see cmake/FindPortableGL.cmake for full list)
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * Shadow maps: FBO render-to-texture is implemented via pglSetTexBackBuffer()
 *   but SoShadowGroup's multi-pass shadow pipeline has not been validated.
 *
 * Context switching: PortableGL uses a single global context pointer.  The
 *   thread-local g_cur_compat pointer in portablegl_compat_funcs.cpp tracks
 *   the current ObolPGLCompatState; multi-threaded rendering requires care.
 *
 * Proposed upstream contributions to PortableGL
 * ──────────────────────────────────────────────
 * See portablegl_compat_funcs.cpp for the list of proposed upstream additions
 * (matrix stack, light/material state, immediate mode, glReadPixels, FBO).
 */

#include <Inventor/SoDB.h>
#include <cstring>

/* portablegl.h is a single-header library.  portablegl_impl.cpp provides the
 * PORTABLEGL_IMPLEMENTATION; here we only need the declarations. */
#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#include <portablegl/portablegl.h>

#include "portablegl_compat_state.h"

/* Forward declarations for functions defined in portablegl_compat_funcs.cpp
 * and portablegl_shader_registry.cpp. */
extern "C" {
    /* Compat funcs (matrix, light, material, imm mode, readpixels, FBO) */
    void pgl_igl_MatrixMode(GLenum);
    void pgl_igl_LoadIdentity();
    void pgl_igl_LoadMatrixf(const GLfloat*);
    void pgl_igl_LoadMatrixd(const GLdouble*);
    void pgl_igl_MultMatrixf(const GLfloat*);
    void pgl_igl_PushMatrix();
    void pgl_igl_PopMatrix();
    void pgl_igl_Translatef(GLfloat,GLfloat,GLfloat);
    void pgl_igl_Scalef(GLfloat,GLfloat,GLfloat);
    void pgl_igl_Rotatef(GLfloat,GLfloat,GLfloat,GLfloat);
    void pgl_igl_Ortho(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble);
    void pgl_igl_Frustum(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble);
    void pgl_igl_Lightfv(GLenum,GLenum,const GLfloat*);
    void pgl_igl_Lightf(GLenum,GLenum,GLfloat);
    void pgl_igl_Lighti(GLenum,GLenum,GLint);
    void pgl_igl_LightModelfv(GLenum,const GLfloat*);
    void pgl_igl_LightModeli(GLenum,GLint);
    void pgl_igl_Materialfv(GLenum,GLenum,const GLfloat*);
    void pgl_igl_Materialf(GLenum,GLenum,GLfloat);
    void pgl_igl_ColorMaterial(GLenum,GLenum);
    void pgl_igl_GetMaterialfv(GLenum,GLenum,GLfloat*);
    void pgl_igl_Enable(GLenum);
    void pgl_igl_Disable(GLenum);
    void pgl_igl_DrawArrays(GLenum, GLint, GLsizei);
    void pgl_igl_DrawElements(GLenum, GLsizei, GLenum, const GLvoid*);
    void pgl_igl_Begin(GLenum);
    void pgl_igl_End();
    void pgl_igl_Vertex2f(GLfloat,GLfloat);
    void pgl_igl_Vertex2s(GLshort,GLshort);
    void pgl_igl_Vertex3f(GLfloat,GLfloat,GLfloat);
    void pgl_igl_Vertex3fv(const GLfloat*);
    void pgl_igl_Vertex4fv(const GLfloat*);
    void pgl_igl_Normal3f(GLfloat,GLfloat,GLfloat);
    void pgl_igl_Normal3fv(const GLfloat*);
    void pgl_igl_Color3f(GLfloat,GLfloat,GLfloat);
    void pgl_igl_Color3fv(const GLfloat*);
    void pgl_igl_Color4f(GLfloat,GLfloat,GLfloat,GLfloat);
    void pgl_igl_Color4fv(const GLfloat*);
    void pgl_igl_Color3ub(GLubyte,GLubyte,GLubyte);
    void pgl_igl_Color3ubv(const GLubyte*);
    void pgl_igl_Color4ub(GLubyte,GLubyte,GLubyte,GLubyte);
    void pgl_igl_Color4ubv(const GLubyte*);
    void pgl_igl_TexCoord2f(GLfloat,GLfloat);
    void pgl_igl_TexCoord2fv(const GLfloat*);
    void pgl_igl_TexCoord3f(GLfloat,GLfloat,GLfloat);
    void pgl_igl_TexCoord3fv(const GLfloat*);
    void pgl_igl_TexCoord4fv(const GLfloat*);
    void pgl_igl_PixelStorei(GLenum,GLint);
    void pgl_igl_ReadPixels(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*);
    void pgl_igl_GenFramebuffers(GLsizei,GLuint*);
    void pgl_igl_BindFramebuffer(GLenum,GLuint);
    void pgl_igl_DeleteFramebuffers(GLsizei,const GLuint*);
    void pgl_igl_FramebufferTexture2D(GLenum,GLenum,GLenum,GLuint,GLint);
    void pgl_igl_FramebufferRenderbuffer(GLenum,GLenum,GLenum,GLuint);
    GLenum pgl_igl_CheckFramebufferStatus(GLenum);
    GLboolean pgl_igl_IsFramebuffer(GLuint);
    void pgl_igl_GenRenderbuffers(GLsizei,GLuint*);
    void pgl_igl_BindRenderbuffer(GLenum,GLuint);
    void pgl_igl_DeleteRenderbuffers(GLsizei,const GLuint*);
    void pgl_igl_RenderbufferStorage(GLenum,GLenum,GLsizei,GLsizei);
    GLboolean pgl_igl_IsRenderbuffer(GLuint);
    /* Fog */
    void pgl_igl_Fogf(GLenum,GLfloat);
    void pgl_igl_Fogi(GLenum,GLint);
    void pgl_igl_Fogfv(GLenum,const GLfloat*);
    void pgl_igl_Fogiv(GLenum,const GLint*);
    /* FBO back-buffer registration (called at makeCurrent) */
    void pgl_fbo_register_context(int,int);
    /* Shader registry */
    GLuint  pgl_igl_CreateShaderObjectARB(GLenum);
    void    pgl_igl_ShaderSourceARB(GLuint,GLsizei,const GLchar**,const GLint*);
    void    pgl_igl_CompileShaderARB(GLuint);
    void    pgl_igl_GetObjectParameterivARB(GLuint,GLenum,GLint*);
    void    pgl_igl_GetInfoLogARB(GLuint,GLsizei,GLsizei*,GLchar*);
    void    pgl_igl_DeleteObjectARB(GLuint);
    GLuint  pgl_igl_CreateProgramObjectARB();
    void    pgl_igl_AttachObjectARB(GLuint,GLuint);
    void    pgl_igl_DetachObjectARB(GLuint,GLuint);
    void    pgl_igl_LinkProgramARB(GLuint);
    GLboolean pgl_igl_IsProgram(GLuint);
    void    pgl_igl_UseProgramObjectARB(GLuint);
    GLint   pgl_igl_GetUniformLocationARB(GLuint,const GLchar*);
    void    pgl_igl_GetActiveUniformARB(GLuint,GLuint,GLsizei,GLsizei*,GLint*,GLenum*,GLchar*);
    void    pgl_igl_Uniform1iARB(GLint,GLint);
    void    pgl_igl_Uniform1fARB(GLint,GLfloat);
    void    pgl_igl_Uniform2fARB(GLint,GLfloat,GLfloat);
    void    pgl_igl_Uniform3fARB(GLint,GLfloat,GLfloat,GLfloat);
    void    pgl_igl_Uniform4fARB(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
    void    pgl_igl_Uniform2iARB(GLint,GLint,GLint);
    void    pgl_igl_Uniform3iARB(GLint,GLint,GLint,GLint);
    void    pgl_igl_Uniform4iARB(GLint,GLint,GLint,GLint,GLint);
    void    pgl_igl_Uniform1fvARB(GLint,GLsizei,const GLfloat*);
    void    pgl_igl_Uniform2fvARB(GLint,GLsizei,const GLfloat*);
    void    pgl_igl_Uniform3fvARB(GLint,GLsizei,const GLfloat*);
    void    pgl_igl_Uniform4fvARB(GLint,GLsizei,const GLfloat*);
    void    pgl_igl_Uniform1ivARB(GLint,GLsizei,const GLint*);
    void    pgl_igl_Uniform2ivARB(GLint,GLsizei,const GLint*);
    void    pgl_igl_Uniform3ivARB(GLint,GLsizei,const GLint*);
    void    pgl_igl_Uniform4ivARB(GLint,GLsizei,const GLint*);
    void    pgl_igl_UniformMatrix2fvARB(GLint,GLsizei,GLboolean,const GLfloat*);
    void    pgl_igl_UniformMatrix3fvARB(GLint,GLsizei,GLboolean,const GLfloat*);
    void    pgl_igl_UniformMatrix4fvARB(GLint,GLsizei,GLboolean,const GLfloat*);
} /* extern "C" */

/* g_cur_compat is defined in portablegl_compat_funcs.cpp; we set it here
 * when switching contexts. */
extern thread_local ObolPGLCompatState* g_cur_compat;

/* Reset per-context caches (VAO/VBO/shader program IDs) when switching to a
 * new portablegl context.  Defined in portablegl_compat_funcs.cpp.           */
extern "C" void pgl_igl_reset_context_caches();

/* ─────────────────────────────────────────────────────────────────────────── */
/* No-op stubs for GL 1.x compatibility-profile functions that PortableGL      */
/* does not implement.  These are needed so that SoGLContext's function pointer */
/* table has valid non-null entries and Obol code can call them safely.         */
/* ─────────────────────────────────────────────────────────────────────────── */
static void pgl_noop_glFlush(void) {}
static void pgl_noop_glFinish(void) {}
static void pgl_noop_glIndexi(GLint) {}
static void pgl_noop_glPixelTransferi(GLenum, GLint) {}
static void pgl_noop_glPixelTransferf(GLenum, GLfloat) {}
static void pgl_noop_glPixelMapfv(GLenum, GLsizei, const GLfloat*) {}
static void pgl_noop_glPixelMapuiv(GLenum, GLsizei, const GLuint*) {}
static void pgl_noop_glPixelMapusv(GLenum, GLsizei, const GLushort*) {}
static void pgl_noop_glPushAttrib(GLbitfield) {}
static void pgl_noop_glPopAttrib(void) {}
static void pgl_noop_glPushClientAttrib(GLbitfield) {}
static void pgl_noop_glPopClientAttrib(void) {}

/* glClear interceptor: translate standard OpenGL buffer-bit values to the
 * PortableGL-internal enum values.  portablegl_compat_consts.h re-defines
 * the GL_*_BUFFER_BIT macros to standard OpenGL values (COLOR=0x4000,
 * DEPTH=0x0100, STENCIL=0x0400), but PortableGL's pgl_glClear implementation
 * uses its own enum: COLOR=1<<10(0x400), DEPTH=1<<11(0x800), STENCIL=1<<12(0x1000).
 * Bits that PortableGL does not support (e.g. GL_ACCUM_BUFFER_BIT) are dropped. */
static void pgl_igl_Clear(GLbitfield mask) {
    /* portablegl.h enum (not overridden by compat_consts.h, use literals): */
    GLbitfield pgl_mask = 0;
    if (mask & 0x4000u) pgl_mask |= (1u << 10); /* GL_COLOR_BUFFER_BIT   */
    if (mask & 0x0100u) pgl_mask |= (1u << 11); /* GL_DEPTH_BUFFER_BIT   */
    if (mask & 0x0400u) pgl_mask |= (1u << 12); /* GL_STENCIL_BUFFER_BIT */
    if (pgl_mask) glClear(pgl_mask);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* NOTE: glReadPixels and all other interceptors are now in                     */
/* portablegl_compat_funcs.cpp and portablegl_shader_registry.cpp.             */
/* This file only contains the context manager and proc-address table.         */
/* ─────────────────────────────────────────────────────────────────────────── */



/* ─────────────────────────────────────────────────────────────────────────── */
/* Static proc-address table                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

struct PGLProcEntry { const char* name; void* fn; };

static const PGLProcEntry s_pgl_proctable[] = {
    /* Core drawing */
    { "glDrawArrays",              (void*)pgl_igl_DrawArrays        },
    { "glDrawElements",            (void*)pgl_igl_DrawElements      },
    { "glDrawArraysInstanced",     (void*)glDrawArraysInstanced     },
    { "glDrawElementsInstanced",   (void*)glDrawElementsInstanced   },
    { "glMultiDrawArrays",         (void*)glMultiDrawArrays         },
    { "glMultiDrawElements",       (void*)glMultiDrawElements       },

    /* State – Enable/Disable intercepted for light management */
    { "glEnable",                  (void*)pgl_igl_Enable            },
    { "glDisable",                 (void*)pgl_igl_Disable           },
    { "glCullFace",                (void*)glCullFace                },
    { "glFrontFace",               (void*)glFrontFace               },
    { "glPolygonMode",             (void*)glPolygonMode             },
    { "glPolygonOffset",           (void*)glPolygonOffset           },
    { "glPointSize",               (void*)glPointSize               },
    { "glLineWidth",               (void*)glLineWidth               },
    { "glScissor",                 (void*)glScissor                 },
    { "glViewport",                (void*)glViewport                },
    { "glDepthFunc",               (void*)glDepthFunc               },
    { "glDepthMask",               (void*)glDepthMask               },
    { "glDepthRange",              (void*)glDepthRange              },
    { "glColorMask",               (void*)glColorMask               },
    { "glBlendFunc",               (void*)glBlendFunc               },
    { "glBlendEquation",           (void*)glBlendEquation           },
    { "glBlendFuncSeparate",       (void*)glBlendFuncSeparate       },
    { "glBlendEquationSeparate",   (void*)glBlendEquationSeparate   },
    { "glBlendColor",              (void*)glBlendColor              },
    { "glStencilFunc",             (void*)glStencilFunc             },
    { "glStencilFuncSeparate",     (void*)glStencilFuncSeparate     },
    { "glStencilOp",               (void*)glStencilOp               },
    { "glStencilOpSeparate",       (void*)glStencilOpSeparate       },
    { "glStencilMask",             (void*)glStencilMask             },
    { "glStencilMaskSeparate",     (void*)glStencilMaskSeparate     },
    { "glLogicOp",                 (void*)glLogicOp                 },
    { "glProvokingVertex",         (void*)glProvokingVertex         },

    /* Clear */
    { "glClear",                   (void*)pgl_igl_Clear             },
    { "glClearColor",              (void*)glClearColor              },
    { "glClearDepth",              (void*)glClearDepth              },
    { "glClearDepthf",             (void*)glClearDepthf             },
    { "glClearStencil",            (void*)glClearStencil            },

    /* Queries */
    { "glGetError",                (void*)glGetError                },
    { "glGetString",               (void*)glGetString               },
    { "glGetStringi",              (void*)glGetStringi              },
    { "glGetBooleanv",             (void*)glGetBooleanv             },
    { "glGetFloatv",               (void*)glGetFloatv               },
    { "glGetIntegerv",             (void*)glGetIntegerv             },
    { "glIsEnabled",               (void*)glIsEnabled               },

    /* Buffers (VBOs) */
    { "glGenBuffers",              (void*)glGenBuffers              },
    { "glDeleteBuffers",           (void*)glDeleteBuffers           },
    { "glBindBuffer",              (void*)glBindBuffer              },
    { "glBufferData",              (void*)glBufferData              },
    { "glBufferSubData",           (void*)glBufferSubData           },
    { "glMapBuffer",               (void*)glMapBuffer               },
    { "glUnmapBuffer",             (void*)glUnmapBuffer             },

    /* Vertex arrays (VAOs) */
    { "glGenVertexArrays",         (void*)glGenVertexArrays         },
    { "glDeleteVertexArrays",      (void*)glDeleteVertexArrays      },
    { "glBindVertexArray",         (void*)glBindVertexArray         },
    { "glVertexAttribPointer",     (void*)glVertexAttribPointer     },
    { "glVertexAttribDivisor",     (void*)glVertexAttribDivisor     },
    { "glEnableVertexAttribArray", (void*)glEnableVertexAttribArray },
    { "glDisableVertexAttribArray",(void*)glDisableVertexAttribArray},

    /* Textures */
    { "glGenTextures",             (void*)glGenTextures             },
    { "glDeleteTextures",          (void*)glDeleteTextures          },
    { "glBindTexture",             (void*)glBindTexture             },
    { "glActiveTexture",           (void*)glActiveTexture           },
    { "glTexParameteri",           (void*)glTexParameteri           },
    { "glTexParameterfv",          (void*)glTexParameterfv          },
    { "glTexParameteriv",          (void*)glTexParameteriv          },
    { "glGetTexParameterfv",       (void*)glGetTexParameterfv       },
    { "glGetTexParameteriv",       (void*)glGetTexParameteriv       },
    { "glTexImage2D",              (void*)glTexImage2D              },
    { "glTexImage3D",              (void*)glTexImage3D              },
    { "glTexSubImage2D",           (void*)glTexSubImage2D           },
    { "glTexSubImage3D",           (void*)glTexSubImage3D           },
    { "glPixelStorei",             (void*)pgl_igl_PixelStorei       },
    { "glGenerateMipmap",          (void*)glGenerateMipmap          },

    /* Pixel read – intercepted: PortableGL has no glReadPixels */
    { "glReadPixels",              (void*)pgl_igl_ReadPixels        },

    /* Matrix stack interceptors (update ObolPGLCompatState) */
    { "glMatrixMode",              (void*)pgl_igl_MatrixMode        },
    { "glLoadIdentity",            (void*)pgl_igl_LoadIdentity      },
    { "glLoadMatrixf",             (void*)pgl_igl_LoadMatrixf       },
    { "glLoadMatrixd",             (void*)pgl_igl_LoadMatrixd       },
    { "glMultMatrixf",             (void*)pgl_igl_MultMatrixf       },
    { "glPushMatrix",              (void*)pgl_igl_PushMatrix        },
    { "glPopMatrix",               (void*)pgl_igl_PopMatrix         },
    { "glTranslatef",              (void*)pgl_igl_Translatef        },
    { "glScalef",                  (void*)pgl_igl_Scalef            },
    { "glRotatef",                 (void*)pgl_igl_Rotatef           },
    { "glOrtho",                   (void*)pgl_igl_Ortho             },
    { "glFrustum",                 (void*)pgl_igl_Frustum           },

    /* Light state interceptors */
    { "glLightfv",                 (void*)pgl_igl_Lightfv           },
    { "glLightf",                  (void*)pgl_igl_Lightf            },
    { "glLighti",                  (void*)pgl_igl_Lighti            },
    { "glLightModelfv",            (void*)pgl_igl_LightModelfv      },
    { "glLightModeli",             (void*)pgl_igl_LightModeli       },
    { "glMaterialfv",              (void*)pgl_igl_Materialfv        },
    { "glMaterialf",               (void*)pgl_igl_Materialf         },
    { "glColorMaterial",           (void*)pgl_igl_ColorMaterial     },
    { "glGetMaterialfv",           (void*)pgl_igl_GetMaterialfv     },

    /* Immediate mode interceptors */
    { "glBegin",                   (void*)pgl_igl_Begin             },
    { "glEnd",                     (void*)pgl_igl_End               },
    { "glVertex2f",                (void*)pgl_igl_Vertex2f          },
    { "glVertex2s",                (void*)pgl_igl_Vertex2s          },
    { "glVertex3f",                (void*)pgl_igl_Vertex3f          },
    { "glVertex3fv",               (void*)pgl_igl_Vertex3fv         },
    { "glVertex4fv",               (void*)pgl_igl_Vertex4fv         },
    { "glNormal3f",                (void*)pgl_igl_Normal3f          },
    { "glNormal3fv",               (void*)pgl_igl_Normal3fv         },
    { "glColor3f",                 (void*)pgl_igl_Color3f           },
    { "glColor3fv",                (void*)pgl_igl_Color3fv          },
    { "glColor4f",                 (void*)pgl_igl_Color4f           },
    { "glColor4fv",                (void*)pgl_igl_Color4fv          },
    { "glColor3ub",                (void*)pgl_igl_Color3ub          },
    { "glColor3ubv",               (void*)pgl_igl_Color3ubv         },
    { "glColor4ub",                (void*)pgl_igl_Color4ub          },
    { "glColor4ubv",               (void*)pgl_igl_Color4ubv         },
    { "glTexCoord2f",              (void*)pgl_igl_TexCoord2f        },
    { "glTexCoord2fv",             (void*)pgl_igl_TexCoord2fv       },
    { "glTexCoord3f",              (void*)pgl_igl_TexCoord3f        },
    { "glTexCoord3fv",             (void*)pgl_igl_TexCoord3fv       },
    { "glTexCoord4fv",             (void*)pgl_igl_TexCoord4fv       },

    /* ARB shader object interceptors (GLSL classification + C-shader dispatch) */
    { "glCreateShaderObjectARB",   (void*)pgl_igl_CreateShaderObjectARB   },
    { "glShaderSourceARB",         (void*)pgl_igl_ShaderSourceARB         },
    { "glCompileShaderARB",        (void*)pgl_igl_CompileShaderARB        },
    { "glGetObjectParameterivARB", (void*)pgl_igl_GetObjectParameterivARB },
    { "glGetInfoLogARB",           (void*)pgl_igl_GetInfoLogARB           },
    { "glDeleteObjectARB",         (void*)pgl_igl_DeleteObjectARB         },
    { "glCreateProgramObjectARB",  (void*)pgl_igl_CreateProgramObjectARB  },
    { "glAttachObjectARB",         (void*)pgl_igl_AttachObjectARB         },
    { "glDetachObjectARB",         (void*)pgl_igl_DetachObjectARB         },
    { "glLinkProgramARB",          (void*)pgl_igl_LinkProgramARB          },
    { "glIsProgram",               (void*)pgl_igl_IsProgram               },
    { "glUseProgramObjectARB",     (void*)pgl_igl_UseProgramObjectARB     },
    { "glGetUniformLocationARB",   (void*)pgl_igl_GetUniformLocationARB   },
    { "glGetActiveUniformARB",     (void*)pgl_igl_GetActiveUniformARB     },

    /* Uniform interceptors */
    { "glUniform1iARB",            (void*)pgl_igl_Uniform1iARB      },
    { "glUniform1fARB",            (void*)pgl_igl_Uniform1fARB      },
    { "glUniform2fARB",            (void*)pgl_igl_Uniform2fARB      },
    { "glUniform3fARB",            (void*)pgl_igl_Uniform3fARB      },
    { "glUniform4fARB",            (void*)pgl_igl_Uniform4fARB      },
    { "glUniform2iARB",            (void*)pgl_igl_Uniform2iARB      },
    { "glUniform3iARB",            (void*)pgl_igl_Uniform3iARB      },
    { "glUniform4iARB",            (void*)pgl_igl_Uniform4iARB      },
    { "glUniform1fvARB",           (void*)pgl_igl_Uniform1fvARB     },
    { "glUniform2fvARB",           (void*)pgl_igl_Uniform2fvARB     },
    { "glUniform3fvARB",           (void*)pgl_igl_Uniform3fvARB     },
    { "glUniform4fvARB",           (void*)pgl_igl_Uniform4fvARB     },
    { "glUniform1ivARB",           (void*)pgl_igl_Uniform1ivARB     },
    { "glUniform2ivARB",           (void*)pgl_igl_Uniform2ivARB     },
    { "glUniform3ivARB",           (void*)pgl_igl_Uniform3ivARB     },
    { "glUniform4ivARB",           (void*)pgl_igl_Uniform4ivARB     },
    { "glUniformMatrix2fvARB",     (void*)pgl_igl_UniformMatrix2fvARB },
    { "glUniformMatrix3fvARB",     (void*)pgl_igl_UniformMatrix3fvARB },
    { "glUniformMatrix4fvARB",     (void*)pgl_igl_UniformMatrix4fvARB },

    /* FBO interceptors (render-to-texture via pglSetTexBackBuffer) */
    { "glGenFramebuffers",         (void*)pgl_igl_GenFramebuffers         },
    { "glBindFramebuffer",         (void*)pgl_igl_BindFramebuffer         },
    { "glDeleteFramebuffers",      (void*)pgl_igl_DeleteFramebuffers      },
    { "glFramebufferTexture2D",    (void*)pgl_igl_FramebufferTexture2D    },
    { "glFramebufferRenderbuffer", (void*)pgl_igl_FramebufferRenderbuffer },
    { "glCheckFramebufferStatus",  (void*)pgl_igl_CheckFramebufferStatus  },
    { "glIsFramebuffer",           (void*)pgl_igl_IsFramebuffer           },
    /* Renderbuffers – tracked for FBO-completeness; depth not sampled */
    { "glGenRenderbuffers",        (void*)pgl_igl_GenRenderbuffers        },
    { "glBindRenderbuffer",        (void*)pgl_igl_BindRenderbuffer        },
    { "glDeleteRenderbuffers",     (void*)pgl_igl_DeleteRenderbuffers     },
    { "glRenderbufferStorage",     (void*)pgl_igl_RenderbufferStorage     },
    { "glIsRenderbuffer",          (void*)pgl_igl_IsRenderbuffer          },
    /* Fog interceptors */
    { "glFogf",                    (void*)pgl_igl_Fogf                    },
    { "glFogi",                    (void*)pgl_igl_Fogi                    },
    { "glFogfv",                   (void*)pgl_igl_Fogfv                   },
    { "glFogiv",                   (void*)pgl_igl_Fogiv                   },
    /* Misc */
    { "glFlush",                   (void*)pgl_noop_glFlush          },
    { "glFinish",                  (void*)pgl_noop_glFinish         },
    { "glIndexi",                  (void*)pgl_noop_glIndexi         },
    { "glPixelTransferi",          (void*)pgl_noop_glPixelTransferi },
    { "glPixelTransferf",          (void*)pgl_noop_glPixelTransferf },
    { "glPixelMapfv",              (void*)pgl_noop_glPixelMapfv     },
    { "glPixelMapuiv",             (void*)pgl_noop_glPixelMapuiv    },
    { "glPixelMapusv",             (void*)pgl_noop_glPixelMapusv    },
    { "glPushAttrib",              (void*)pgl_noop_glPushAttrib     },
    { "glPopAttrib",               (void*)pgl_noop_glPopAttrib      },
    { "glPushClientAttrib",        (void*)pgl_noop_glPushClientAttrib },
    { "glPopClientAttrib",         (void*)pgl_noop_glPopClientAttrib},
    { "glDrawBuffers",             (void*)glDrawBuffers             },
    { "glReadBuffer",              (void*)glReadBuffer              },

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
/* Per-context data                                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

static glContext* s_current_pgl_context = nullptr;

struct PGLContextData {
    glContext          pgl_ctx;
    pix_t*             backbuf;
    int                width;
    int                height;
    glContext*         saved_prev;
    ObolPGLCompatState compat;

    PGLContextData(int w, int h)
        : backbuf(nullptr), width(w), height(h), saved_prev(nullptr)
    { compat.init(); }

    ~PGLContextData() {
        if (s_current_pgl_context == &pgl_ctx) {
            set_glContext(nullptr);
            s_current_pgl_context = nullptr;
            g_cur_compat = nullptr;
        }
        free_glContext(&pgl_ctx);
    }

    bool init() {
        return init_glContext(&pgl_ctx, &backbuf, width, height) != GL_FALSE;
    }

    void makeCurrent() {
        saved_prev       = s_current_pgl_context;
        s_current_pgl_context = &pgl_ctx;
        set_glContext(&pgl_ctx);
        g_cur_compat = &compat;
        pglSetUniform(&compat);
        /* Reset per-context caches (program IDs, VAO/VBO handles) whenever
         * the active portablegl context changes.  The new context has its own
         * object-name namespace so cached handles from the previous context
         * would be invalid.                                                   */
        pgl_igl_reset_context_caches();
        /* Register default back-buffer pointer/dimensions so BindFramebuffer(0)
         * can restore the PGL-internal back buffer after FBO use.           */
        pgl_fbo_register_context(width, height);
    }

    void restorePrev() {
        s_current_pgl_context = saved_prev;
        set_glContext(saved_prev);
        g_cur_compat = nullptr;
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
