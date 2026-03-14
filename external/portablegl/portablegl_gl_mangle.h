/*
 * portablegl_gl_mangle.h  –  Rename all PortableGL gl* symbols to pgl_gl*
 *
 * PURPOSE
 * ───────
 * PortableGL implements its OpenGL 3.x functions with the standard `gl*`
 * names (e.g. glDrawArrays, glClear, …).  When building Obol with BOTH a
 * system OpenGL backend and a PortableGL software backend in a single shared
 * library (OBOL_BUILD_DUAL_PORTABLEGL), those names collide with the real
 * system GL symbols.
 *
 * This header solves the collision using the same technique as OSMesa's
 * <OSMesa/gl_mangle.h>: a flat set of preprocessor macros that rename every
 * gl* token to pgl_gl*.  After this header is included:
 *
 *   • portablegl.h declares (and, with PORTABLEGL_IMPLEMENTATION, defines)
 *     the functions under the pgl_gl* names.
 *   • gl_portablegl.cpp recompiles gl.cpp with these macros active, so
 *     every `glDrawArrays(…)` call in the Obol source becomes a call to the
 *     PortableGL-private `pgl_glDrawArrays(…)`.
 *
 * UPSTREAM CONTRIBUTION NOTE
 * ──────────────────────────
 * This mechanism was designed for inclusion in portablegl.h itself under a
 * PGL_PREFIX_GL guard:
 *
 *   #ifdef PGL_PREFIX_GL
 *   #include "portablegl_gl_mangle.h"
 *   #endif
 *
 * When that patch is merged upstream, this file moves inside portablegl.h
 * and is no longer needed as a standalone header.
 *
 * USAGE
 * ─────
 * Include before portablegl.h in any TU that must call the renamed symbols:
 *
 *   #include <portablegl/portablegl_gl_mangle.h>
 *   #include <portablegl/portablegl.h>
 *
 * IMPORTANT: the glStringi macro at the bottom handles the glGetStringi
 * function which portablegl defines as a stub; glCreateBuffers is a macro
 * alias for glGenBuffers in portablegl.h and is treated accordingly.
 */

#ifndef PORTABLEGL_GL_MANGLE_H
#define PORTABLEGL_GL_MANGLE_H

/* ── A ──────────────────────────────────────────────────────────────────── */
#define glActiveTexture            pgl_glActiveTexture
#define glAttachShader             pgl_glAttachShader

/* ── B ──────────────────────────────────────────────────────────────────── */
#define glBindBuffer               pgl_glBindBuffer
#define glBindFramebuffer          pgl_glBindFramebuffer
#define glBindRenderbuffer         pgl_glBindRenderbuffer
#define glBindTexture              pgl_glBindTexture
#define glBindVertexArray          pgl_glBindVertexArray
#define glBlendColor               pgl_glBlendColor
#define glBlendEquation            pgl_glBlendEquation
#define glBlendEquationSeparate    pgl_glBlendEquationSeparate
#define glBlendFunc                pgl_glBlendFunc
#define glBlendFuncSeparate        pgl_glBlendFuncSeparate
#define glBlitFramebuffer          pgl_glBlitFramebuffer
#define glBlitNamedFramebuffer     pgl_glBlitNamedFramebuffer
#define glBufferData               pgl_glBufferData
#define glBufferSubData            pgl_glBufferSubData

/* ── C ──────────────────────────────────────────────────────────────────── */
#define glCheckFramebufferStatus   pgl_glCheckFramebufferStatus
#define glClear                    pgl_glClear
#define glClearBufferfi            pgl_glClearBufferfi
#define glClearBufferfv            pgl_glClearBufferfv
#define glClearBufferiv            pgl_glClearBufferiv
#define glClearBufferuiv           pgl_glClearBufferuiv
#define glClearColor               pgl_glClearColor
#define glClearDepth               pgl_glClearDepth
#define glClearDepthf              pgl_glClearDepthf
#define glClearNamedFramebufferfi  pgl_glClearNamedFramebufferfi
#define glClearNamedFramebufferfv  pgl_glClearNamedFramebufferfv
#define glClearNamedFramebufferiv  pgl_glClearNamedFramebufferiv
#define glClearNamedFramebufferuiv pgl_glClearNamedFramebufferuiv
#define glClearStencil             pgl_glClearStencil
#define glColorMask                pgl_glColorMask
#define glColorMaski               pgl_glColorMaski
#define glCompileShader            pgl_glCompileShader
#define glCompressedTexImage1D     pgl_glCompressedTexImage1D
#define glCompressedTexImage2D     pgl_glCompressedTexImage2D
#define glCompressedTexImage3D     pgl_glCompressedTexImage3D
/* glCreateBuffers is a macro alias for glGenBuffers in portablegl.h.
 * After mangling glGenBuffers, the alias is re-defined by portablegl.h
 * as pgl_glGenBuffers; we also provide an explicit mapping. */
#define glCreateBuffers            pgl_glCreateBuffers
#define glCreateProgram            pgl_glCreateProgram
#define glCreateShader             pgl_glCreateShader
#define glCreateTextures           pgl_glCreateTextures
#define glCullFace                 pgl_glCullFace

/* ── D ──────────────────────────────────────────────────────────────────── */
#define glDebugMessageCallback     pgl_glDebugMessageCallback
#define glDeleteBuffers            pgl_glDeleteBuffers
#define glDeleteFramebuffers       pgl_glDeleteFramebuffers
#define glDeleteProgram            pgl_glDeleteProgram
#define glDeleteRenderbuffers      pgl_glDeleteRenderbuffers
#define glDeleteShader             pgl_glDeleteShader
#define glDeleteTextures           pgl_glDeleteTextures
#define glDeleteVertexArrays       pgl_glDeleteVertexArrays
#define glDepthFunc                pgl_glDepthFunc
#define glDepthMask                pgl_glDepthMask
#define glDepthRange               pgl_glDepthRange
#define glDepthRangef              pgl_glDepthRangef
#define glDetachShader             pgl_glDetachShader
#define glDisable                  pgl_glDisable
#define glDisableVertexArrayAttrib pgl_glDisableVertexArrayAttrib
#define glDisableVertexAttribArray pgl_glDisableVertexAttribArray
#define glDrawArrays               pgl_glDrawArrays
#define glDrawArraysInstanced      pgl_glDrawArraysInstanced
#define glDrawArraysInstancedBaseInstance pgl_glDrawArraysInstancedBaseInstance
#define glDrawBuffers              pgl_glDrawBuffers
#define glDrawElements             pgl_glDrawElements
#define glDrawElementsInstanced    pgl_glDrawElementsInstanced
#define glDrawElementsInstancedBaseInstance pgl_glDrawElementsInstancedBaseInstance

/* ── E ──────────────────────────────────────────────────────────────────── */
#define glEnable                   pgl_glEnable
#define glEnableVertexArrayAttrib  pgl_glEnableVertexArrayAttrib
#define glEnableVertexAttribArray  pgl_glEnableVertexAttribArray

/* ── F ──────────────────────────────────────────────────────────────────── */
#define glFramebufferRenderbuffer  pgl_glFramebufferRenderbuffer
#define glFramebufferTexture       pgl_glFramebufferTexture
#define glFramebufferTexture1D     pgl_glFramebufferTexture1D
#define glFramebufferTexture2D     pgl_glFramebufferTexture2D
#define glFramebufferTexture3D     pgl_glFramebufferTexture3D
#define glFramebufferTextureLayer  pgl_glFramebufferTextureLayer
#define glFrontFace                pgl_glFrontFace

/* ── G ──────────────────────────────────────────────────────────────────── */
#define glGenBuffers               pgl_glGenBuffers
#define glGenFramebuffers          pgl_glGenFramebuffers
#define glGenRenderbuffers         pgl_glGenRenderbuffers
#define glGenTextures              pgl_glGenTextures
#define glGenVertexArrays          pgl_glGenVertexArrays
#define glGenerateMipmap           pgl_glGenerateMipmap
#define glGetAttribLocation        pgl_glGetAttribLocation
#define glGetBooleanv              pgl_glGetBooleanv
#define glGetDoublev               pgl_glGetDoublev
#define glGetError                 pgl_glGetError
#define glGetFloatv                pgl_glGetFloatv
#define glGetInteger64v            pgl_glGetInteger64v
#define glGetIntegerv              pgl_glGetIntegerv
#define glGetProgramInfoLog        pgl_glGetProgramInfoLog
#define glGetProgramiv             pgl_glGetProgramiv
#define glGetShaderInfoLog         pgl_glGetShaderInfoLog
#define glGetShaderiv              pgl_glGetShaderiv
#define glGetString                pgl_glGetString
#define glGetStringi               pgl_glGetStringi
#define glGetTexParameterIiv       pgl_glGetTexParameterIiv
#define glGetTexParameterIuiv      pgl_glGetTexParameterIuiv
#define glGetTexParameterfv        pgl_glGetTexParameterfv
#define glGetTexParameteriv        pgl_glGetTexParameteriv
#define glGetTextureParameterIiv   pgl_glGetTextureParameterIiv
#define glGetTextureParameterIuiv  pgl_glGetTextureParameterIuiv
#define glGetTextureParameterfv    pgl_glGetTextureParameterfv
#define glGetTextureParameteriv    pgl_glGetTextureParameteriv
#define glGetUniformLocation       pgl_glGetUniformLocation

/* ── I ──────────────────────────────────────────────────────────────────── */
#define glIsEnabled                pgl_glIsEnabled
#define glIsFramebuffer            pgl_glIsFramebuffer
#define glIsProgram                pgl_glIsProgram
#define glIsRenderbuffer           pgl_glIsRenderbuffer

/* ── L ──────────────────────────────────────────────────────────────────── */
#define glLineWidth                pgl_glLineWidth
#define glLinkProgram              pgl_glLinkProgram
#define glLogicOp                  pgl_glLogicOp

/* ── M ──────────────────────────────────────────────────────────────────── */
#define glMapBuffer                pgl_glMapBuffer
#define glMapNamedBuffer           pgl_glMapNamedBuffer
#define glMultiDrawArrays          pgl_glMultiDrawArrays
#define glMultiDrawElements        pgl_glMultiDrawElements

/* ── N ──────────────────────────────────────────────────────────────────── */
#define glNamedBufferData                 pgl_glNamedBufferData
#define glNamedBufferSubData              pgl_glNamedBufferSubData
#define glNamedFramebufferDrawBuffers     pgl_glNamedFramebufferDrawBuffers
#define glNamedFramebufferReadBuffer      pgl_glNamedFramebufferReadBuffer
#define glNamedFramebufferTextureLayer    pgl_glNamedFramebufferTextureLayer
#define glNamedRenderbufferStorageMultisample pgl_glNamedRenderbufferStorageMultisample

/* ── P ──────────────────────────────────────────────────────────────────── */
#define glPixelStorei              pgl_glPixelStorei
#define glPointParameteri          pgl_glPointParameteri
#define glPointSize                pgl_glPointSize
#define glPolygonMode              pgl_glPolygonMode
#define glPolygonOffset            pgl_glPolygonOffset
#define glProvokingVertex          pgl_glProvokingVertex

/* ── R ──────────────────────────────────────────────────────────────────── */
#define glReadBuffer               pgl_glReadBuffer
#define glRenderbufferStorage      pgl_glRenderbufferStorage
#define glRenderbufferStorageMultisample pgl_glRenderbufferStorageMultisample

/* ── S ──────────────────────────────────────────────────────────────────── */
#define glScissor                  pgl_glScissor
#define glShaderSource             pgl_glShaderSource
#define glStencilFunc              pgl_glStencilFunc
#define glStencilFuncSeparate      pgl_glStencilFuncSeparate
#define glStencilMask              pgl_glStencilMask
#define glStencilMaskSeparate      pgl_glStencilMaskSeparate
#define glStencilOp                pgl_glStencilOp
#define glStencilOpSeparate        pgl_glStencilOpSeparate

/* ── T ──────────────────────────────────────────────────────────────────── */
#define glTexBuffer                pgl_glTexBuffer
#define glTexImage1D               pgl_glTexImage1D
#define glTexImage2D               pgl_glTexImage2D
#define glTexImage3D               pgl_glTexImage3D
#define glTexParameterf            pgl_glTexParameterf
#define glTexParameterfv           pgl_glTexParameterfv
#define glTexParameteri            pgl_glTexParameteri
#define glTexParameteriv           pgl_glTexParameteriv
#define glTexParameterliv          pgl_glTexParameterliv
#define glTexParameterluiv         pgl_glTexParameterluiv
#define glTexSubImage1D            pgl_glTexSubImage1D
#define glTexSubImage2D            pgl_glTexSubImage2D
#define glTexSubImage3D            pgl_glTexSubImage3D
#define glTextureBuffer            pgl_glTextureBuffer
#define glTextureParameterf        pgl_glTextureParameterf
#define glTextureParameterfv       pgl_glTextureParameterfv
#define glTextureParameteri        pgl_glTextureParameteri
#define glTextureParameteriv       pgl_glTextureParameteriv
#define glTextureParameterliv      pgl_glTextureParameterliv
#define glTextureParameterluiv     pgl_glTextureParameterluiv

/* ── U ──────────────────────────────────────────────────────────────────── */
#define glUniform1f                pgl_glUniform1f
#define glUniform1fv               pgl_glUniform1fv
#define glUniform1i                pgl_glUniform1i
#define glUniform1iv               pgl_glUniform1iv
#define glUniform1ui               pgl_glUniform1ui
#define glUniform1uiv              pgl_glUniform1uiv
#define glUniform2f                pgl_glUniform2f
#define glUniform2fv               pgl_glUniform2fv
#define glUniform2i                pgl_glUniform2i
#define glUniform2iv               pgl_glUniform2iv
#define glUniform2ui               pgl_glUniform2ui
#define glUniform2uiv              pgl_glUniform2uiv
#define glUniform3f                pgl_glUniform3f
#define glUniform3fv               pgl_glUniform3fv
#define glUniform3i                pgl_glUniform3i
#define glUniform3iv               pgl_glUniform3iv
#define glUniform3ui               pgl_glUniform3ui
#define glUniform3uiv              pgl_glUniform3uiv
#define glUniform4f                pgl_glUniform4f
#define glUniform4fv               pgl_glUniform4fv
#define glUniform4i                pgl_glUniform4i
#define glUniform4iv               pgl_glUniform4iv
#define glUniform4ui               pgl_glUniform4ui
#define glUniform4uiv              pgl_glUniform4uiv
#define glUniformMatrix2fv         pgl_glUniformMatrix2fv
#define glUniformMatrix2x3fv       pgl_glUniformMatrix2x3fv
#define glUniformMatrix2x4fv       pgl_glUniformMatrix2x4fv
#define glUniformMatrix3fv         pgl_glUniformMatrix3fv
#define glUniformMatrix3x2fv       pgl_glUniformMatrix3x2fv
#define glUniformMatrix3x4fv       pgl_glUniformMatrix3x4fv
#define glUniformMatrix4fv         pgl_glUniformMatrix4fv
#define glUniformMatrix4x2fv       pgl_glUniformMatrix4x2fv
#define glUniformMatrix4x3fv       pgl_glUniformMatrix4x3fv
#define glUnmapBuffer              pgl_glUnmapBuffer
#define glUnmapNamedBuffer         pgl_glUnmapNamedBuffer
#define glUseProgram               pgl_glUseProgram

/* ── V ──────────────────────────────────────────────────────────────────── */
#define glVertexAttribDivisor      pgl_glVertexAttribDivisor
#define glVertexAttribPointer      pgl_glVertexAttribPointer
#define glViewport                 pgl_glViewport

#endif /* PORTABLEGL_GL_MANGLE_H */
