#ifndef OBOL_INVENTOR_GL_H
#define OBOL_INVENTOR_GL_H

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
 * Inventor/gl.h  --  Drop-in replacement for <GL/gl.h> that routes all
 *                    gl*() calls through Obol's GL dispatch layer.
 *
 * PURPOSE
 * -------
 * In single-backend Obol builds (system OpenGL only, or pure OSMesa) raw
 * gl*() calls work fine.  In dual-backend builds (OBOL_BUILD_DUAL_GL),
 * raw gl*() calls are always bound to the system GL library, so they
 * silently go to the wrong backend during OSMesa renders.
 *
 * Including this header instead of the platform <GL/gl.h> causes every
 * gl*() call in that translation unit to be redirected through Obol's
 * dispatch layer via sogl_current_render_glue(), which always returns the
 * SoGLContext for the render pass currently active on this thread.
 *
 * USAGE
 * -----
 * In SoCallback code and custom scene-graph nodes, replace:
 *
 *   #include <GL/gl.h>          // or <OSMesa/gl.h> / <OpenGL/gl.h>
 *
 * with:
 *
 *   #include <Inventor/gl.h>
 *
 * Existing gl*() calls need no further changes; they are redirected at
 * compile time by the macros below.
 *
 * LIMITATIONS
 * -----------
 * - The macros are valid only while sogl_current_render_glue() is non-NULL,
 *   i.e. during an active SoGLRenderAction traversal pass.  Calling a
 *   redirected gl*() outside of rendering yields undefined behaviour.
 * - Only the core OpenGL 1.x subset listed below is redirected.  Calls to
 *   extension functions (glGenBuffers, glBindBuffer, etc.) are not macros
 *   here; for those, use SoGLContext_glXxx() directly from SoGLDispatch.h.
 * - This header must not be included inside any Obol internal translation
 *   unit that compiles with SOGL_PREFIX_SET (i.e. gl_osmesa.cpp), as the
 *   macro names would conflict with the internal dispatch wrappers.
 */

/* Pull in GL enum values and types (GLenum, GLfloat, etc.).
   Do NOT define our macros before this include; the system GL header may
   use some of these names internally (e.g. as function parameters). */
#include <Inventor/system/gl.h>

/* Dispatch API: sogl_current_render_glue() and SoGLContext_glXxx(). */
#include <Inventor/system/SoGLDispatch.h>

/*
 * Macro redirects.
 *
 * Each bare gl*() call becomes:
 *   SoGLContext_glXxx(sogl_current_render_glue(), ...)
 *
 * The macros shadow the GLAPI-declared gl* functions from the system GL
 * header.  This shadowing is intentional: at the call site, the macro
 * expansion takes precedence over any function declaration.
 *
 * We undef each name before defining it to silence compiler warnings
 * when an existing #define is already present (e.g. from gl_mangle.h
 * in OBOL_OSMESA_BUILD builds that pull in <OSMesa/gl_mangle.h>).
 */

/* --- Immediate mode geometry --- */
#undef glBegin
#define glBegin(mode)                   SoGLContext_glBegin(sogl_current_render_glue(), (mode))
#undef glEnd
#define glEnd()                         SoGLContext_glEnd(sogl_current_render_glue())
#undef glVertex2f
#define glVertex2f(x,y)                 SoGLContext_glVertex2f(sogl_current_render_glue(), (x),(y))
#undef glVertex2s
#define glVertex2s(x,y)                 SoGLContext_glVertex2s(sogl_current_render_glue(), (x),(y))
#undef glVertex3f
#define glVertex3f(x,y,z)               SoGLContext_glVertex3f(sogl_current_render_glue(), (x),(y),(z))
#undef glVertex3fv
#define glVertex3fv(v)                  SoGLContext_glVertex3fv(sogl_current_render_glue(), (v))
#undef glVertex4fv
#define glVertex4fv(v)                  SoGLContext_glVertex4fv(sogl_current_render_glue(), (v))
#undef glNormal3f
#define glNormal3f(nx,ny,nz)            SoGLContext_glNormal3f(sogl_current_render_glue(), (nx),(ny),(nz))
#undef glNormal3fv
#define glNormal3fv(v)                  SoGLContext_glNormal3fv(sogl_current_render_glue(), (v))
#undef glColor3f
#define glColor3f(r,g,b)                SoGLContext_glColor3f(sogl_current_render_glue(), (r),(g),(b))
#undef glColor3fv
#define glColor3fv(v)                   SoGLContext_glColor3fv(sogl_current_render_glue(), (v))
#undef glColor3ub
#define glColor3ub(r,g,b)               SoGLContext_glColor3ub(sogl_current_render_glue(), (r),(g),(b))
#undef glColor3ubv
#define glColor3ubv(v)                  SoGLContext_glColor3ubv(sogl_current_render_glue(), (v))
#undef glColor4ub
#define glColor4ub(r,g,b,a)             SoGLContext_glColor4ub(sogl_current_render_glue(), (r),(g),(b),(a))
#undef glTexCoord2f
#define glTexCoord2f(s,t)               SoGLContext_glTexCoord2f(sogl_current_render_glue(), (s),(t))
#undef glTexCoord2fv
#define glTexCoord2fv(v)                SoGLContext_glTexCoord2fv(sogl_current_render_glue(), (v))
#undef glTexCoord3f
#define glTexCoord3f(s,t,r)             SoGLContext_glTexCoord3f(sogl_current_render_glue(), (s),(t),(r))
#undef glTexCoord3fv
#define glTexCoord3fv(v)                SoGLContext_glTexCoord3fv(sogl_current_render_glue(), (v))
#undef glTexCoord4fv
#define glTexCoord4fv(v)                SoGLContext_glTexCoord4fv(sogl_current_render_glue(), (v))
#undef glIndexi
#define glIndexi(c)                     SoGLContext_glIndexi(sogl_current_render_glue(), (c))

/* --- Matrix stack --- */
#undef glMatrixMode
#define glMatrixMode(mode)              SoGLContext_glMatrixMode(sogl_current_render_glue(), (mode))
#undef glLoadIdentity
#define glLoadIdentity()                SoGLContext_glLoadIdentity(sogl_current_render_glue())
#undef glLoadMatrixf
#define glLoadMatrixf(m)                SoGLContext_glLoadMatrixf(sogl_current_render_glue(), (m))
#undef glLoadMatrixd
#define glLoadMatrixd(m)                SoGLContext_glLoadMatrixd(sogl_current_render_glue(), (m))
#undef glMultMatrixf
#define glMultMatrixf(m)                SoGLContext_glMultMatrixf(sogl_current_render_glue(), (m))
#undef glPushMatrix
#define glPushMatrix()                  SoGLContext_glPushMatrix(sogl_current_render_glue())
#undef glPopMatrix
#define glPopMatrix()                   SoGLContext_glPopMatrix(sogl_current_render_glue())
#undef glOrtho
#define glOrtho(l,r,b,t,n,f)            SoGLContext_glOrtho(sogl_current_render_glue(), (l),(r),(b),(t),(n),(f))
#undef glFrustum
#define glFrustum(l,r,b,t,n,f)          SoGLContext_glFrustum(sogl_current_render_glue(), (l),(r),(b),(t),(n),(f))
#undef glTranslatef
#define glTranslatef(x,y,z)             SoGLContext_glTranslatef(sogl_current_render_glue(), (x),(y),(z))
#undef glRotatef
#define glRotatef(a,x,y,z)              SoGLContext_glRotatef(sogl_current_render_glue(), (a),(x),(y),(z))
#undef glScalef
#define glScalef(x,y,z)                 SoGLContext_glScalef(sogl_current_render_glue(), (x),(y),(z))

/* --- Render state --- */
#undef glEnable
#define glEnable(cap)                   SoGLContext_glEnable(sogl_current_render_glue(), (cap))
#undef glDisable
#define glDisable(cap)                  SoGLContext_glDisable(sogl_current_render_glue(), (cap))
#undef glIsEnabled
#define glIsEnabled(cap)                SoGLContext_glIsEnabled(sogl_current_render_glue(), (cap))
#undef glPushAttrib
#define glPushAttrib(mask)              SoGLContext_glPushAttrib(sogl_current_render_glue(), (mask))
#undef glPopAttrib
#define glPopAttrib()                   SoGLContext_glPopAttrib(sogl_current_render_glue())
#undef glDepthMask
#define glDepthMask(flag)               SoGLContext_glDepthMask(sogl_current_render_glue(), (flag))
#undef glDepthFunc
#define glDepthFunc(func)               SoGLContext_glDepthFunc(sogl_current_render_glue(), (func))
#undef glDepthRange
#define glDepthRange(n,f)               SoGLContext_glDepthRange(sogl_current_render_glue(), (n),(f))
#undef glBlendFunc
#define glBlendFunc(sf,df)              SoGLContext_glBlendFunc(sogl_current_render_glue(), (sf),(df))
#undef glAlphaFunc
#define glAlphaFunc(func,ref)           SoGLContext_glAlphaFunc(sogl_current_render_glue(), (func),(ref))
#undef glViewport
#define glViewport(x,y,w,h)             SoGLContext_glViewport(sogl_current_render_glue(), (x),(y),(w),(h))
#undef glScissor
#define glScissor(x,y,w,h)              SoGLContext_glScissor(sogl_current_render_glue(), (x),(y),(w),(h))
#undef glClearColor
#define glClearColor(r,g,b,a)           SoGLContext_glClearColor(sogl_current_render_glue(), (r),(g),(b),(a))
#undef glClear
#define glClear(mask)                   SoGLContext_glClear(sogl_current_render_glue(), (mask))
#undef glFlush
#define glFlush()                       SoGLContext_glFlush(sogl_current_render_glue())
#undef glFinish
#define glFinish()                      SoGLContext_glFinish(sogl_current_render_glue())
#undef glGetError
#define glGetError()                    SoGLContext_glGetError(sogl_current_render_glue())
#undef glGetString
#define glGetString(name)               SoGLContext_glGetString(sogl_current_render_glue(), (name))
#undef glGetIntegerv
#define glGetIntegerv(pname,params)     SoGLContext_glGetIntegerv(sogl_current_render_glue(), (pname),(params))
#undef glGetFloatv
#define glGetFloatv(pname,params)       SoGLContext_glGetFloatv(sogl_current_render_glue(), (pname),(params))
#undef glGetBooleanv
#define glGetBooleanv(pname,params)     SoGLContext_glGetBooleanv(sogl_current_render_glue(), (pname),(params))
#undef glColorMask
#define glColorMask(r,g,b,a)            SoGLContext_glColorMask(sogl_current_render_glue(), (r),(g),(b),(a))
#undef glStencilFunc
#define glStencilFunc(f,ref,m)          SoGLContext_glStencilFunc(sogl_current_render_glue(), (f),(ref),(m))
#undef glStencilOp
#define glStencilOp(f,z,p)              SoGLContext_glStencilOp(sogl_current_render_glue(), (f),(z),(p))
#undef glFrontFace
#define glFrontFace(mode)               SoGLContext_glFrontFace(sogl_current_render_glue(), (mode))
#undef glCullFace
#define glCullFace(mode)                SoGLContext_glCullFace(sogl_current_render_glue(), (mode))
#undef glPolygonMode
#define glPolygonMode(face,mode)        SoGLContext_glPolygonMode(sogl_current_render_glue(), (face),(mode))
#undef glPolygonStipple
#define glPolygonStipple(mask)          SoGLContext_glPolygonStipple(sogl_current_render_glue(), (mask))
#undef glLineWidth
#define glLineWidth(w)                  SoGLContext_glLineWidth(sogl_current_render_glue(), (w))
#undef glLineStipple
#define glLineStipple(factor,pattern)   SoGLContext_glLineStipple(sogl_current_render_glue(), (factor),(pattern))
#undef glPointSize
#define glPointSize(sz)                 SoGLContext_glPointSize(sogl_current_render_glue(), (sz))
#undef glClipPlane
#define glClipPlane(plane,eq)           SoGLContext_glClipPlane(sogl_current_render_glue(), (plane),(eq))
#undef glDrawBuffer
#define glDrawBuffer(mode)              SoGLContext_glDrawBuffer(sogl_current_render_glue(), (mode))
#undef glClearIndex
#define glClearIndex(c)                 SoGLContext_glClearIndex(sogl_current_render_glue(), (c))
#undef glClearStencil
#define glClearStencil(s)               SoGLContext_glClearStencil(sogl_current_render_glue(), (s))
#undef glAccum
#define glAccum(op,value)               SoGLContext_glAccum(sogl_current_render_glue(), (op),(value))

/* --- Pixel / image operations --- */
#undef glPixelStorei
#define glPixelStorei(pname,param)      SoGLContext_glPixelStorei(sogl_current_render_glue(), (pname),(param))
#undef glReadPixels
#define glReadPixels(x,y,w,h,f,t,p)     SoGLContext_glReadPixels(sogl_current_render_glue(), (x),(y),(w),(h),(f),(t),(p))
#undef glRasterPos2f
#define glRasterPos2f(x,y)              SoGLContext_glRasterPos2f(sogl_current_render_glue(), (x),(y))
#undef glRasterPos3f
#define glRasterPos3f(x,y,z)            SoGLContext_glRasterPos3f(sogl_current_render_glue(), (x),(y),(z))
#undef glBitmap
#define glBitmap(w,h,xo,yo,xm,ym,bm)    SoGLContext_glBitmap(sogl_current_render_glue(), (w),(h),(xo),(yo),(xm),(ym),(bm))
#undef glDrawPixels
#define glDrawPixels(w,h,f,t,p)         SoGLContext_glDrawPixels(sogl_current_render_glue(), (w),(h),(f),(t),(p))
#undef glPixelTransferf
#define glPixelTransferf(pname,param)   SoGLContext_glPixelTransferf(sogl_current_render_glue(), (pname),(param))
#undef glPixelTransferi
#define glPixelTransferi(pname,param)   SoGLContext_glPixelTransferi(sogl_current_render_glue(), (pname),(param))
#undef glPixelMapfv
#define glPixelMapfv(map,ms,vals)       SoGLContext_glPixelMapfv(sogl_current_render_glue(), (map),(ms),(vals))
#undef glPixelMapuiv
#define glPixelMapuiv(map,ms,vals)      SoGLContext_glPixelMapuiv(sogl_current_render_glue(), (map),(ms),(vals))
#undef glPixelZoom
#define glPixelZoom(xf,yf)              SoGLContext_glPixelZoom(sogl_current_render_glue(), (xf),(yf))

/* --- Lighting / material --- */
#undef glLightf
#define glLightf(light,pname,param)     SoGLContext_glLightf(sogl_current_render_glue(), (light),(pname),(param))
#undef glLightfv
#define glLightfv(light,pname,params)   SoGLContext_glLightfv(sogl_current_render_glue(), (light),(pname),(params))
#undef glLightModeli
#define glLightModeli(pname,param)      SoGLContext_glLightModeli(sogl_current_render_glue(), (pname),(param))
#undef glLightModelfv
#define glLightModelfv(pname,params)    SoGLContext_glLightModelfv(sogl_current_render_glue(), (pname),(params))
#undef glMaterialf
#define glMaterialf(face,pname,param)   SoGLContext_glMaterialf(sogl_current_render_glue(), (face),(pname),(param))
#undef glMaterialfv
#define glMaterialfv(face,pname,params) SoGLContext_glMaterialfv(sogl_current_render_glue(), (face),(pname),(params))
#undef glColorMaterial
#define glColorMaterial(face,mode)      SoGLContext_glColorMaterial(sogl_current_render_glue(), (face),(mode))

/* --- Fog --- */
#undef glFogi
#define glFogi(pname,param)             SoGLContext_glFogi(sogl_current_render_glue(), (pname),(param))
#undef glFogf
#define glFogf(pname,param)             SoGLContext_glFogf(sogl_current_render_glue(), (pname),(param))
#undef glFogfv
#define glFogfv(pname,params)           SoGLContext_glFogfv(sogl_current_render_glue(), (pname),(params))

/* --- Textures --- */
#undef glTexEnvi
#define glTexEnvi(t,pname,param)        SoGLContext_glTexEnvi(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexEnvf
#define glTexEnvf(t,pname,param)        SoGLContext_glTexEnvf(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexEnvfv
#define glTexEnvfv(t,pname,params)      SoGLContext_glTexEnvfv(sogl_current_render_glue(), (t),(pname),(params))
#undef glTexGeni
#define glTexGeni(coord,pname,param)    SoGLContext_glTexGeni(sogl_current_render_glue(), (coord),(pname),(param))
#undef glTexGenf
#define glTexGenf(coord,pname,param)    SoGLContext_glTexGenf(sogl_current_render_glue(), (coord),(pname),(param))
#undef glTexGenfv
#define glTexGenfv(coord,pname,params)  SoGLContext_glTexGenfv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glTexParameteri
#define glTexParameteri(t,pname,param)  SoGLContext_glTexParameteri(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexParameterf
#define glTexParameterf(t,pname,param)  SoGLContext_glTexParameterf(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexImage2D
#define glTexImage2D(t,l,if,w,h,b,f,ty,p) \
    SoGLContext_glTexImage2D(sogl_current_render_glue(), (t),(l),(if),(w),(h),(b),(f),(ty),(p))
#undef glCopyTexImage2D
#define glCopyTexImage2D(t,l,if,x,y,w,h,b) \
    SoGLContext_glCopyTexImage2D(sogl_current_render_glue(), (t),(l),(if),(x),(y),(w),(h),(b))
#undef glCopyTexSubImage2D
#define glCopyTexSubImage2D(t,l,xo,yo,x,y,w,h) \
    SoGLContext_glCopyTexSubImage2D(sogl_current_render_glue(), (t),(l),(xo),(yo),(x),(y),(w),(h))

/* --- Display lists --- */
#undef glNewList
#define glNewList(list,mode)            SoGLContext_glNewList(sogl_current_render_glue(), (list),(mode))
#undef glEndList
#define glEndList()                     SoGLContext_glEndList(sogl_current_render_glue())
#undef glCallList
#define glCallList(list)                SoGLContext_glCallList(sogl_current_render_glue(), (list))
#undef glDeleteLists
#define glDeleteLists(list,range)       SoGLContext_glDeleteLists(sogl_current_render_glue(), (list),(range))
#undef glGenLists
#define glGenLists(range)               SoGLContext_glGenLists(sogl_current_render_glue(), (range))

/* --- Vertex arrays --- */
#undef glDrawArrays
#define glDrawArrays(mode,first,count)  SoGLContext_glDrawArrays(sogl_current_render_glue(), (mode),(first),(count))
#undef glDrawElements
#define glDrawElements(mode,count,type,idx) \
    SoGLContext_glDrawElements(sogl_current_render_glue(), (mode),(count),(type),(idx))

#endif /* OBOL_INVENTOR_GL_H */
