#ifndef OBOL_SOGL_DISPATCH_H
#define OBOL_SOGL_DISPATCH_H

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
 * SoGLDispatch.h  --  Public API for Obol's backend-dispatched GL wrappers.
 *
 * PURPOSE
 * -------
 * In single-backend Obol builds (system OpenGL only, or pure OSMesa) raw
 * gl*() calls work fine because there is only one backend.  In dual-backend
 * builds (OBOL_BUILD_DUAL_GL) both system OpenGL and OSMesa contexts coexist
 * in the same process; a raw gl*() call is always routed to the system GL
 * library regardless of which context is currently active, so OSMesa renders
 * receive the wrong calls.
 *
 * This header exposes the dispatch layer so user code (SoCallback handlers,
 * custom nodes, etc.) can make GL calls that are correctly routed to whatever
 * backend owns the active rendering context.
 *
 * USAGE – EXPLICIT FORM
 * ---------------------
 * Obtain a dispatch context and call SoGLContext_glXxx():
 *
 *   #include <Inventor/system/SoGLDispatch.h>
 *   #include <Inventor/actions/SoGLRenderAction.h>
 *
 *   void myCallback(void * data, SoAction * action)
 *   {
 *       if (!action->isOfType(SoGLRenderAction::getClassTypeId())) return;
 *       const SoGLContext * ctx = sogl_glue_from_state(action->getState());
 *       SoGLContext_glMatrixMode(ctx, GL_PROJECTION);
 *       SoGLContext_glPushMatrix(ctx);
 *       SoGLContext_glLoadIdentity(ctx);
 *       SoGLContext_glOrtho(ctx, 0, w, 0, h, -1, 1);
 *       SoGLContext_glBegin(ctx, GL_QUADS);
 *         SoGLContext_glColor3f(ctx, 0.2f, 0.4f, 0.8f);
 *         SoGLContext_glVertex2f(ctx, 0.0f, 0.0f);
 *         // ...
 *       SoGLContext_glEnd(ctx);
 *       SoGLContext_glMatrixMode(ctx, GL_PROJECTION);
 *       SoGLContext_glPopMatrix(ctx);
 *   }
 *
 * USAGE – IMPLICIT FORM (Inventor/gl.h trick header)
 * ---------------------------------------------------
 * For code that already uses bare gl*() calls, include <Inventor/gl.h>
 * instead of <GL/gl.h>.  That header includes SoGLDispatch.h and defines
 * macros that redirect every gl*() call through sogl_current_render_glue(),
 * which holds the active context for the duration of a rendering pass.
 * No other source changes are required.
 */

#include <Inventor/basic.h>
#include <Inventor/system/gl.h>   /* GL types: GLenum, GLfloat, GLsizei … */

/* Forward declarations – SoGLContext is intentionally opaque to callers. */
struct SoGLContext;
class  SoState;

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Context retrieval
 * --------------------------------------------------------------------- */

/**
 * Return the GL dispatch context for the render pass currently active on
 * this thread.  Set by SoGLRenderAction before each traversal pass and
 * cleared afterwards.  Valid inside any SoCallback that is invoked during
 * a GL render action; returns NULL outside of a render pass.
 */
OBOL_DLL_API const SoGLContext * sogl_current_render_glue(void);

/**
 * Derive the GL dispatch context from an SoState pointer.
 * Equivalent to sogl_current_render_glue() but uses an explicit state
 * pointer, which is slightly safer (no dependency on thread-local storage).
 * Typical use: pass action->getState() from your SoCallback.
 */
OBOL_DLL_API const SoGLContext * sogl_glue_from_state(const SoState * state);

/* -----------------------------------------------------------------------
 * Core GL 1.0/1.1 dispatch wrappers
 *
 * Every function routes through the correct backend at runtime.
 * Signature: (const SoGLContext * ctx, <original GL parameters>)
 * --------------------------------------------------------------------- */

/* --- Immediate mode geometry --- */
OBOL_DLL_API void SoGLContext_glBegin(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glEnd(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glVertex2f(const SoGLContext * glue, GLfloat x, GLfloat y);
OBOL_DLL_API void SoGLContext_glVertex2s(const SoGLContext * glue, GLshort x, GLshort y);
OBOL_DLL_API void SoGLContext_glVertex3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glVertex3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glVertex4fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glNormal3f(const SoGLContext * glue, GLfloat nx, GLfloat ny, GLfloat nz);
OBOL_DLL_API void SoGLContext_glNormal3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glColor3f(const SoGLContext * glue, GLfloat r, GLfloat g, GLfloat b);
OBOL_DLL_API void SoGLContext_glColor3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glColor3ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b);
OBOL_DLL_API void SoGLContext_glColor3ubv(const SoGLContext * glue, const GLubyte * v);
OBOL_DLL_API void SoGLContext_glColor4ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b, GLubyte a);
OBOL_DLL_API void SoGLContext_glTexCoord2f(const SoGLContext * glue, GLfloat s, GLfloat t);
OBOL_DLL_API void SoGLContext_glTexCoord2fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glTexCoord3f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r);
OBOL_DLL_API void SoGLContext_glTexCoord3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glTexCoord4fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glIndexi(const SoGLContext * glue, GLint c);

/* --- Matrix stack --- */
OBOL_DLL_API void SoGLContext_glMatrixMode(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glLoadIdentity(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glLoadMatrixf(const SoGLContext * glue, const GLfloat * m);
OBOL_DLL_API void SoGLContext_glLoadMatrixd(const SoGLContext * glue, const GLdouble * m);
OBOL_DLL_API void SoGLContext_glMultMatrixf(const SoGLContext * glue, const GLfloat * m);
OBOL_DLL_API void SoGLContext_glPushMatrix(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glPopMatrix(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glOrtho(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
OBOL_DLL_API void SoGLContext_glFrustum(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
OBOL_DLL_API void SoGLContext_glTranslatef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glRotatef(const SoGLContext * glue, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glScalef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);

/* --- Render state --- */
OBOL_DLL_API void SoGLContext_glEnable(const SoGLContext * glue, GLenum cap);
OBOL_DLL_API void SoGLContext_glDisable(const SoGLContext * glue, GLenum cap);
OBOL_DLL_API GLboolean SoGLContext_glIsEnabled(const SoGLContext * glue, GLenum cap);
OBOL_DLL_API void SoGLContext_glPushAttrib(const SoGLContext * glue, GLbitfield mask);
OBOL_DLL_API void SoGLContext_glPopAttrib(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glDepthMask(const SoGLContext * glue, GLboolean flag);
OBOL_DLL_API void SoGLContext_glDepthFunc(const SoGLContext * glue, GLenum func);
OBOL_DLL_API void SoGLContext_glDepthRange(const SoGLContext * glue, GLclampd near_val, GLclampd far_val);
OBOL_DLL_API void SoGLContext_glBlendFunc(const SoGLContext * glue, GLenum sfactor, GLenum dfactor);
OBOL_DLL_API void SoGLContext_glAlphaFunc(const SoGLContext * glue, GLenum func, GLclampf ref);
OBOL_DLL_API void SoGLContext_glViewport(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height);
OBOL_DLL_API void SoGLContext_glScissor(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height);
OBOL_DLL_API void SoGLContext_glClearColor(const SoGLContext * glue, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
OBOL_DLL_API void SoGLContext_glClear(const SoGLContext * glue, GLbitfield mask);
OBOL_DLL_API void SoGLContext_glFlush(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glFinish(const SoGLContext * glue);
OBOL_DLL_API GLenum SoGLContext_glGetError(const SoGLContext * glue);
OBOL_DLL_API const GLubyte * SoGLContext_glGetString(const SoGLContext * glue, GLenum name);
OBOL_DLL_API void SoGLContext_glGetIntegerv(const SoGLContext * glue, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetFloatv(const SoGLContext * glue, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetBooleanv(const SoGLContext * glue, GLenum pname, GLboolean * params);
OBOL_DLL_API void SoGLContext_glColorMask(const SoGLContext * glue, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
OBOL_DLL_API void SoGLContext_glStencilFunc(const SoGLContext * glue, GLenum func, GLint ref, GLuint mask);
OBOL_DLL_API void SoGLContext_glStencilOp(const SoGLContext * glue, GLenum fail, GLenum zfail, GLenum zpass);
OBOL_DLL_API void SoGLContext_glFrontFace(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glCullFace(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glPolygonMode(const SoGLContext * glue, GLenum face, GLenum mode);
OBOL_DLL_API void SoGLContext_glPolygonStipple(const SoGLContext * glue, const GLubyte * mask);
OBOL_DLL_API void SoGLContext_glLineWidth(const SoGLContext * glue, GLfloat width);
OBOL_DLL_API void SoGLContext_glLineStipple(const SoGLContext * glue, GLint factor, GLushort pattern);
OBOL_DLL_API void SoGLContext_glPointSize(const SoGLContext * glue, GLfloat size);
OBOL_DLL_API void SoGLContext_glClipPlane(const SoGLContext * glue, GLenum plane, const GLdouble * equation);
OBOL_DLL_API void SoGLContext_glDrawBuffer(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glClearIndex(const SoGLContext * glue, GLfloat c);
OBOL_DLL_API void SoGLContext_glClearStencil(const SoGLContext * glue, GLint s);
OBOL_DLL_API void SoGLContext_glAccum(const SoGLContext * glue, GLenum op, GLfloat value);

/* --- Pixel / image operations --- */
OBOL_DLL_API void SoGLContext_glPixelStorei(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glReadPixels(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glRasterPos2f(const SoGLContext * glue, GLfloat x, GLfloat y);
OBOL_DLL_API void SoGLContext_glRasterPos3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glBitmap(const SoGLContext * glue, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap);
OBOL_DLL_API void SoGLContext_glDrawPixels(const SoGLContext * glue, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glPixelTransferf(const SoGLContext * glue, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glPixelTransferi(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glPixelMapfv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLfloat * values);
OBOL_DLL_API void SoGLContext_glPixelMapuiv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLuint * values);
OBOL_DLL_API void SoGLContext_glPixelZoom(const SoGLContext * glue, GLfloat xfactor, GLfloat yfactor);

/* --- Lighting / material --- */
OBOL_DLL_API void SoGLContext_glLightf(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glLightfv(const SoGLContext * glue, GLenum light, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glLightModeli(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glLightModelfv(const SoGLContext * glue, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glMaterialf(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glColorMaterial(const SoGLContext * glue, GLenum face, GLenum mode);

/* --- Fog --- */
OBOL_DLL_API void SoGLContext_glFogi(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glFogf(const SoGLContext * glue, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glFogfv(const SoGLContext * glue, GLenum pname, const GLfloat * params);

/* --- Textures --- */
OBOL_DLL_API void SoGLContext_glTexEnvi(const SoGLContext * glue, GLenum target, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glTexEnvf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glTexGeni(const SoGLContext * glue, GLenum coord, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glTexGenf(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glTexParameteri(const SoGLContext * glue, GLenum target, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glTexParameterf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glCopyTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
OBOL_DLL_API void SoGLContext_glCopyTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

/* --- Display lists --- */
OBOL_DLL_API void  SoGLContext_glNewList(const SoGLContext * glue, GLuint list, GLenum mode);
OBOL_DLL_API void  SoGLContext_glEndList(const SoGLContext * glue);
OBOL_DLL_API void  SoGLContext_glCallList(const SoGLContext * glue, GLuint list);
OBOL_DLL_API void  SoGLContext_glDeleteLists(const SoGLContext * glue, GLuint list, GLsizei range);
OBOL_DLL_API GLuint SoGLContext_glGenLists(const SoGLContext * glue, GLsizei range);

/* --- Vertex arrays --- */
OBOL_DLL_API void SoGLContext_glDrawArrays(const SoGLContext * glue, GLenum mode, GLint first, GLsizei count);
OBOL_DLL_API void SoGLContext_glDrawElements(const SoGLContext * glue, GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);

#ifdef __cplusplus
}
#endif

#endif /* OBOL_SOGL_DISPATCH_H */
