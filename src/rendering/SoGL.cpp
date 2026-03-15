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

// This is an internal class with utility functions we use when
// playing around with OpenGL.

// *************************************************************************

#include "rendering/SoGL.h"
#include "config.h"

#include <cassert>
#include <cstdio>
#include <cstring>


#include "CoinTidbits.h"
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/bundles/SoMaterialBundle.h>
#include <Inventor/bundles/SoTextureCoordinateBundle.h>
#include <Inventor/bundles/SoVertexAttributeBundle.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/elements/SoComplexityElement.h>
#include <Inventor/elements/SoComplexityTypeElement.h>
#include <Inventor/elements/SoCoordinateElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/elements/SoGLCoordinateElement.h>
#include <Inventor/elements/SoGLMultiTextureEnabledElement.h>
#include <Inventor/elements/SoGLMultiTextureImageElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoMultiTextureEnabledElement.h>
#include <Inventor/elements/SoProfileElement.h>
#include <Inventor/elements/SoShapeStyleElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoMultiTextureCoordinateElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoProfile.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/system/gl.h>
#include <Inventor/threads/SbStorage.h>

#include "glue/glp.h"
#include "misc/SoEnvironment.h"
#include "rendering/SoGLModernState.h"

// *************************************************************************

// Thread-local storage for the GL dispatch context active during the
// current SoGLRenderAction traversal pass on this thread.
// Set by sogl_set_current_render_glue() (called from
// SoGLRenderAction::beginTraversal / endTraversal).
static thread_local const SoGLContext * s_current_render_glue = nullptr;

const SoGLContext *
sogl_current_render_glue(void)
{
  return s_current_render_glue;
}

void
sogl_set_current_render_glue(const SoGLContext * glue)
{
  s_current_render_glue = glue;
}

// *************************************************************************

// Convenience function for access to OpenGL wrapper from an SoState
// pointer.
const SoGLContext *
sogl_glue_instance(const SoState * state)
{
  // Guard against callers that pass NULL state.  SoGLImage::setData() has a
  // default createinstate=NULL parameter and calls us unconditionally; when
  // state is absent there is no active render context to interrogate.
  if (!state) return nullptr;

  SoGLRenderAction * action = (SoGLRenderAction *)state->getAction();
  if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
    return SoGLContext_instance(action->getCacheContext());
  }
  static int didwarn = 0;
  if (!didwarn) {
    didwarn = 1;
    SoDebugError::postWarning("sogl_glue_instance",
                              "Wrong action type detected. Please report this to <coin-support@coin3d.org>, "
                              "and include information about your system (compiler, Linux version, etc.");
  }
  // just return some SoGLContext instance. It usually doesn't matter
  // that much unless multiple contexts on multiple displays are used.
  return SoGLContext_instance(1);
}

// Convenience wrapper: derive the GL context from the render action
// attached to the state.  By going through state->getAction() this works
// correctly at every point in the render pipeline, including during SoState
// construction when SoGLCacheContextElement has not yet been pushed.
// Obol always requires the calling code to have set up a GL context before
// invoking any render action; if getAction() does not carry a valid
// SoGLRenderAction (and thus no context), that is a caller error.
const SoGLContext *
sogl_glue_from_state(const SoState * state)
{
  return sogl_glue_instance(state);
}


// generate a 3d circle in the x-z plane
static void
sogl_generate_3d_circle(SbVec3f *coords, const int num, const float radius, const float y)
{
  float delta = 2.0f*float(M_PI)/float(num);
  float angle = 0.0f;
  for (int i = 0; i < num; i++) {
    coords[i][0] = -float(sin(angle)) * radius;
    coords[i][1] = y;
    coords[i][2] = -float(cos(angle)) * radius;
    angle += delta;
  }
}

// generate a 2d circle
static void
sogl_generate_2d_circle(SbVec2f *coords, const int num, const float radius)
{
  float delta = 2.0f*float(M_PI)/float(num);
  float angle = 0.0f;
  for (int i = 0; i < num; i++) {
    coords[i][0] = -float(sin(angle)) * radius;
    coords[i][1] = -float(cos(angle)) * radius;
    angle += delta;
  }
}

/* =========================================================================
 * Modern VAO+VBO rendering for a cone, a cylinder, and a cube.
 * Each function follows the same pattern as sogl_render_sphere_modern:
 *  1. Generate vertex data (position, normal, texcoord) CPU-side.
 *  2. Upload to a temporary VAO + VBO (+ EBO for indexed drawing).
 *  3. Activate the built-in Phong shader via SoGLModernState.
 *  4. Issue glDrawElements / glDrawArrays.
 *  5. Release GPU resources.
 * ===================================================================== */

/* --- Cone ------------------------------------------------------------ */

static void
sogl_render_cone_modern(SoGLModernState * ms,
                        float radius, float height,
                        int numslices,
                        bool renderSide, bool renderBottom,
                        bool needNormals, bool needTexCoords)
{
  int slices = numslices < 4 ? 4 : (numslices > 128 ? 128 : numslices);
  float h2   = height * 0.5f;

  /* The side of a cone consists of (slices) triangles: apex + two base verts.
   * The bottom cap is a triangle fan from the base centre.
   * We generate a flat array of (position+normal+texcoord) triangles.       */
  const int STRIDE = 8;

  /* Count triangles and allocate */
  int trisSide   = renderSide   ? slices : 0;
  int trisBottom = renderBottom ? slices : 0;
  int totalTris  = trisSide + trisBottom;
  if (totalTris == 0) return;

  float * vtx = new float[totalTris * 3 * STRIDE];
  int vi = 0;

  /* Helper lambda to emit a vertex into vtx[] */
  auto emit = [&](float px, float py, float pz,
                  float nx, float ny, float nz,
                  float u,  float v) {
    vtx[vi++] = px; vtx[vi++] = py; vtx[vi++] = pz;
    vtx[vi++] = nx; vtx[vi++] = ny; vtx[vi++] = nz;
    vtx[vi++] = u;  vtx[vi++] = v;
  };

  /* Precompute base ring */
  float * bx = new float[slices + 1];
  float * bz = new float[slices + 1];
  for (int i = 0; i <= slices; ++i) {
    float theta = 2.0f * float(M_PI) * float(i) / float(slices);
    bx[i] = -std::sin(theta) * radius;
    bz[i] = -std::cos(theta) * radius;
  }

  if (renderSide) {
    /* Cone side normal: perpendicular to the slant, lying in the xz plane.
     * For vertex at angle theta: outward tangent normal has y component
     * = sin(slant_angle) = radius / hypot(radius, height). */
    float sn = height / std::sqrt(radius * radius + height * height);
    float cn = radius / std::sqrt(radius * radius + height * height);

    for (int i = 0; i < slices; ++i) {
      float t0 = float(i)     / float(slices);
      float t1 = float(i + 1) / float(slices);
      /* Mid-slice normal for the apex vertex */
      float amid = 2.0f * float(M_PI) * (float(i) + 0.5f) / float(slices);
      float nxApex = cn * (-std::sin(amid));
      float nzApex = cn * (-std::cos(amid));

      /* Apex */
      emit(0.0f, h2, 0.0f,
           nxApex, sn, nzApex,
           (t0 + t1) * 0.5f, 1.0f);
      /* Base vertex i */
      float nx0 = cn * (-std::sin(2.0f*float(M_PI)*float(i)  /float(slices)));
      float nz0 = cn * (-std::cos(2.0f*float(M_PI)*float(i)  /float(slices)));
      emit(bx[i], -h2, bz[i], nx0, sn, nz0, t0, 0.0f);
      /* Base vertex i+1 */
      float nx1 = cn * (-std::sin(2.0f*float(M_PI)*float(i+1)/float(slices)));
      float nz1 = cn * (-std::cos(2.0f*float(M_PI)*float(i+1)/float(slices)));
      emit(bx[i+1], -h2, bz[i+1], nx1, sn, nz1, t1, 0.0f);
    }
  }

  if (renderBottom) {
    for (int i = 0; i < slices; ++i) {
      float t0 = float(i)     / float(slices);
      float t1 = float(i + 1) / float(slices);
      /* Centre */
      emit(0.0f, -h2, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
      /* Rim (reversed winding for inward-facing bottom) */
      emit(bx[i+1], -h2, bz[i+1], 0.0f, -1.0f, 0.0f,
           0.5f + 0.5f * bx[i+1] / radius,
           0.5f + 0.5f * bz[i+1] / radius);
      emit(bx[i],   -h2, bz[i],   0.0f, -1.0f, 0.0f,
           0.5f + 0.5f * bx[i]   / radius,
           0.5f + 0.5f * bz[i]   / radius);
    }
  }

  delete[] bx; delete[] bz;

  GLuint vao = 0, vbo = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               totalTris * 3 * STRIDE * (GLsizeiptr)sizeof(float),
               vtx, GL_STATIC_DRAW);

  const GLsizei byteStride = STRIDE * (GLsizei)sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(0*sizeof(float)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(3*sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byteStride, (void*)(6*sizeof(float)));
  glEnableVertexAttribArray(2);

  ms->activatePhong(needNormals, false, needTexCoords, false);
  glDrawArrays(GL_TRIANGLES, 0, totalTris * 3);
  ms->deactivate();

  glBindVertexArray(0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  delete[] vtx;
}

/* --- Cylinder -------------------------------------------------------- */

static void
sogl_render_cylinder_modern(SoGLModernState * ms,
                             float radius, float height,
                             int numslices,
                             bool renderSide, bool renderTop, bool renderBottom,
                             bool needNormals, bool needTexCoords)
{
  int slices = numslices < 4 ? 4 : (numslices > 128 ? 128 : numslices);
  float h2 = height * 0.5f;

  const int STRIDE = 8;
  /* Triangles: side = slices*2, top/bottom caps = slices each */
  int trisSide   = renderSide   ? slices * 2 : 0;
  int trisTop    = renderTop    ? slices      : 0;
  int trisBottom = renderBottom ? slices      : 0;
  int totalTris  = trisSide + trisTop + trisBottom;
  if (totalTris == 0) return;

  float * vtx = new float[totalTris * 3 * STRIDE];
  int vi = 0;

  auto emit = [&](float px, float py, float pz,
                  float nx, float ny, float nz,
                  float u,  float v) {
    vtx[vi++] = px; vtx[vi++] = py; vtx[vi++] = pz;
    vtx[vi++] = nx; vtx[vi++] = ny; vtx[vi++] = nz;
    vtx[vi++] = u;  vtx[vi++] = v;
  };

  /* Precompute ring */
  float * bx = new float[slices + 1];
  float * bz = new float[slices + 1];
  float * nx = new float[slices + 1];
  float * nz = new float[slices + 1];
  for (int i = 0; i <= slices; ++i) {
    float theta = 2.0f * float(M_PI) * float(i) / float(slices);
    bx[i] = -std::sin(theta) * radius;
    bz[i] = -std::cos(theta) * radius;
    nx[i] = -std::sin(theta);
    nz[i] = -std::cos(theta);
  }

  if (renderSide) {
    for (int i = 0; i < slices; ++i) {
      float t0 = float(i)     / float(slices);
      float t1 = float(i + 1) / float(slices);
      /* Quad as two triangles (CCW winding for outward normals) */
      emit(bx[i],   h2, bz[i],   nx[i], 0.0f, nz[i], t0, 1.0f);
      emit(bx[i],  -h2, bz[i],   nx[i], 0.0f, nz[i], t0, 0.0f);
      emit(bx[i+1], h2, bz[i+1], nx[i+1], 0.0f, nz[i+1], t1, 1.0f);

      emit(bx[i+1], h2, bz[i+1], nx[i+1], 0.0f, nz[i+1], t1, 1.0f);
      emit(bx[i],  -h2, bz[i],   nx[i], 0.0f, nz[i], t0, 0.0f);
      emit(bx[i+1],-h2, bz[i+1], nx[i+1], 0.0f, nz[i+1], t1, 0.0f);
    }
  }

  if (renderTop) {
    for (int i = 0; i < slices; ++i) {
      emit(0.0f, h2, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f);
      emit(bx[i],   h2, bz[i],   0.0f, 1.0f, 0.0f,
           0.5f + 0.5f*bx[i]/radius, 0.5f + 0.5f*bz[i]/radius);
      emit(bx[i+1], h2, bz[i+1], 0.0f, 1.0f, 0.0f,
           0.5f + 0.5f*bx[i+1]/radius, 0.5f + 0.5f*bz[i+1]/radius);
    }
  }

  if (renderBottom) {
    for (int i = 0; i < slices; ++i) {
      emit(0.0f, -h2, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
      emit(bx[i+1], -h2, bz[i+1], 0.0f, -1.0f, 0.0f,
           0.5f + 0.5f*bx[i+1]/radius, 0.5f + 0.5f*bz[i+1]/radius);
      emit(bx[i],   -h2, bz[i],   0.0f, -1.0f, 0.0f,
           0.5f + 0.5f*bx[i]/radius, 0.5f + 0.5f*bz[i]/radius);
    }
  }

  delete[] bx; delete[] bz; delete[] nx; delete[] nz;

  GLuint vao = 0, vbo = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               totalTris * 3 * STRIDE * (GLsizeiptr)sizeof(float),
               vtx, GL_STATIC_DRAW);

  const GLsizei byteStride = STRIDE * (GLsizei)sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(0*sizeof(float)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(3*sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byteStride, (void*)(6*sizeof(float)));
  glEnableVertexAttribArray(2);

  ms->activatePhong(needNormals, false, needTexCoords, false);
  glDrawArrays(GL_TRIANGLES, 0, totalTris * 3);
  ms->deactivate();

  glBindVertexArray(0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  delete[] vtx;
}

/* --- Cube ------------------------------------------------------------ */

static void
sogl_render_cube_modern(SoGLModernState * ms,
                        float w, float h, float d,
                        bool needNormals, bool needTexCoords)
{
  /* 6 faces × 2 triangles × 3 vertices, interleaved pos(3)+norm(3)+uv(2) */
  const int STRIDE = 8;
  const int NUM_VERTS = 6 * 2 * 3;
  float vtx[NUM_VERTS * STRIDE];
  int vi = 0;

  float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

  auto emit = [&](float px, float py, float pz,
                  float nx, float ny, float nz,
                  float u,  float v) {
    vtx[vi++] = px; vtx[vi++] = py; vtx[vi++] = pz;
    vtx[vi++] = nx; vtx[vi++] = ny; vtx[vi++] = nz;
    vtx[vi++] = u;  vtx[vi++] = v;
  };

  /* +Z face */
  emit(-hw,-hh, hd, 0,0,1, 0,0); emit( hw,-hh, hd, 0,0,1, 1,0); emit( hw, hh, hd, 0,0,1, 1,1);
  emit(-hw,-hh, hd, 0,0,1, 0,0); emit( hw, hh, hd, 0,0,1, 1,1); emit(-hw, hh, hd, 0,0,1, 0,1);
  /* -Z face */
  emit( hw,-hh,-hd, 0,0,-1, 0,0); emit(-hw,-hh,-hd, 0,0,-1, 1,0); emit(-hw, hh,-hd, 0,0,-1, 1,1);
  emit( hw,-hh,-hd, 0,0,-1, 0,0); emit(-hw, hh,-hd, 0,0,-1, 1,1); emit( hw, hh,-hd, 0,0,-1, 0,1);
  /* +X face */
  emit( hw,-hh, hd, 1,0,0, 0,0); emit( hw,-hh,-hd, 1,0,0, 1,0); emit( hw, hh,-hd, 1,0,0, 1,1);
  emit( hw,-hh, hd, 1,0,0, 0,0); emit( hw, hh,-hd, 1,0,0, 1,1); emit( hw, hh, hd, 1,0,0, 0,1);
  /* -X face */
  emit(-hw,-hh,-hd, -1,0,0, 0,0); emit(-hw,-hh, hd, -1,0,0, 1,0); emit(-hw, hh, hd, -1,0,0, 1,1);
  emit(-hw,-hh,-hd, -1,0,0, 0,0); emit(-hw, hh, hd, -1,0,0, 1,1); emit(-hw, hh,-hd, -1,0,0, 0,1);
  /* +Y face */
  emit(-hw, hh, hd, 0,1,0, 0,0); emit( hw, hh, hd, 0,1,0, 1,0); emit( hw, hh,-hd, 0,1,0, 1,1);
  emit(-hw, hh, hd, 0,1,0, 0,0); emit( hw, hh,-hd, 0,1,0, 1,1); emit(-hw, hh,-hd, 0,1,0, 0,1);
  /* -Y face */
  emit(-hw,-hh,-hd, 0,-1,0, 0,0); emit( hw,-hh,-hd, 0,-1,0, 1,0); emit( hw,-hh, hd, 0,-1,0, 1,1);
  emit(-hw,-hh,-hd, 0,-1,0, 0,0); emit( hw,-hh, hd, 0,-1,0, 1,1); emit(-hw,-hh, hd, 0,-1,0, 0,1);

  GLuint vao = 0, vbo = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vtx), vtx, GL_STATIC_DRAW);

  const GLsizei byteStride = STRIDE * (GLsizei)sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(0*sizeof(float)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(3*sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byteStride, (void*)(6*sizeof(float)));
  glEnableVertexAttribArray(2);

  ms->activatePhong(needNormals, false, needTexCoords, false);
  glDrawArrays(GL_TRIANGLES, 0, NUM_VERTS);
  ms->deactivate();

  glBindVertexArray(0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
}

void
sogl_render_cone(const float radius,
                 const float height,
                 const int numslices,
                 SoMaterialBundle * const material,
                 const unsigned int flagsin,
                 SoState * state)
{
  /* Phase 1 modernization: use VAO+VBO path when SoGLModernState is available. */
  if (state) {
    const SoGLContext * glue = sogl_glue_instance(state);
    if (glue) {
      uint32_t ctxid = SoGLContext_get_contextid(glue);
      SoGLModernState * ms = SoGLModernState::forContext(ctxid);
      if (ms && ms->isAvailable()) {
        sogl_render_cone_modern(ms, radius, height, numslices,
                                (flagsin & SOGL_RENDER_SIDE)   != 0,
                                (flagsin & SOGL_RENDER_BOTTOM) != 0,
                                (flagsin & SOGL_NEED_NORMALS)  != 0,
                                (flagsin & SOGL_NEED_TEXCOORDS)!= 0);
        if (SoComplexityTypeElement::get(state) ==
            SoComplexityTypeElement::OBJECT_SPACE) {
          SoGLCacheContextElement::shouldAutoCache(
              state, SoGLCacheContextElement::DO_AUTO_CACHE);
          SoGLCacheContextElement::incNumShapes(state);
        } else {
          SoGLCacheContextElement::shouldAutoCache(
              state, SoGLCacheContextElement::DONT_AUTO_CACHE);
        }
        (void)material;
        return;
      }
    }
  }

  /* GL3: modern VAO/VBO path required; no fixed-function fallback. */
  SoDebugError::postWarning("sogl_render_cone", "SoGLModernState unavailable; skipping render");
}

void
sogl_render_cylinder(const float radius,
                     const float height,
                     const int numslices,
                     SoMaterialBundle * const material,
                     const unsigned int flagsin,
                     SoState * state)
{
  /* Phase 1 modernization: use VAO+VBO path when SoGLModernState is available. */
  if (state) {
    const SoGLContext * glue = sogl_glue_instance(state);
    if (glue) {
      uint32_t ctxid = SoGLContext_get_contextid(glue);
      SoGLModernState * ms = SoGLModernState::forContext(ctxid);
      if (ms && ms->isAvailable()) {
        sogl_render_cylinder_modern(ms, radius, height, numslices,
                                    (flagsin & SOGL_RENDER_SIDE)  != 0,
                                    (flagsin & SOGL_RENDER_TOP)   != 0,
                                    (flagsin & SOGL_RENDER_BOTTOM)!= 0,
                                    (flagsin & SOGL_NEED_NORMALS) != 0,
                                    (flagsin & SOGL_NEED_TEXCOORDS)!=0);
        if (SoComplexityTypeElement::get(state) ==
            SoComplexityTypeElement::OBJECT_SPACE) {
          SoGLCacheContextElement::shouldAutoCache(
              state, SoGLCacheContextElement::DO_AUTO_CACHE);
          SoGLCacheContextElement::incNumShapes(state);
        } else {
          SoGLCacheContextElement::shouldAutoCache(
              state, SoGLCacheContextElement::DONT_AUTO_CACHE);
        }
        (void)material;
        return;
      }
    }
  }

  /* GL3: modern VAO/VBO path required; no fixed-function fallback. */
  SoDebugError::postWarning("sogl_render_cylinder", "SoGLModernState unavailable; skipping render");
}

static void
sogl_render_sphere_modern(SoGLModernState * ms,
                          const float radius,
                          const int numstacks,
                          const int numslices,
                          bool needNormals,
                          bool needTexCoords,
                          SoState * state)
{
  int stacks = numstacks < 3   ? 3   : (numstacks > 128 ? 128 : numstacks);
  int slices  = numslices < 4   ? 4   : (numslices > 128 ? 128 : numslices);

  /* Interleaved layout: position(3) + normal(3) + texcoord(2) = 8 floats */
  const int STRIDE = 8;
  int numVerts = (stacks + 1) * (slices + 1);
  int numTris  = stacks * slices * 2;

  float *   vtx = new float   [numVerts * STRIDE];
  uint32_t* idx = new uint32_t[numTris  * 3];

  /* Generate vertices */
  for (int si = 0; si <= stacks; ++si) {
    float phi = float(M_PI) * float(si) / float(stacks);
    float sp  = std::sin(phi);
    float cp  = std::cos(phi);
    for (int sl = 0; sl <= slices; ++sl) {
      float theta = 2.0f * float(M_PI) * float(sl) / float(slices);
      float st    = std::sin(theta);
      float ct    = std::cos(theta);
      /* Normal = unit vector on sphere */
      float nx = sp * ct, ny = cp, nz = sp * st;
      float * v = vtx + (si * (slices + 1) + sl) * STRIDE;
      v[0] = radius * nx; v[1] = radius * ny; v[2] = radius * nz; /* position  */
      v[3] = nx;           v[4] = ny;           v[5] = nz;          /* normal    */
      v[6] = float(sl) / float(slices);                              /* texcoord u */
      v[7] = 1.0f - float(si) / float(stacks);                      /* texcoord v */
    }
  }

  /* Generate triangle indices */
  int ti = 0;
  for (int si = 0; si < stacks; ++si) {
    for (int sl = 0; sl < slices; ++sl) {
      uint32_t v0 = (uint32_t)(si       * (slices + 1) + sl);
      uint32_t v1 = v0 + 1;
      uint32_t v2 = (uint32_t)((si + 1) * (slices + 1) + sl);
      uint32_t v3 = v2 + 1;
      idx[ti++] = v0; idx[ti++] = v2; idx[ti++] = v1;
      idx[ti++] = v1; idx[ti++] = v2; idx[ti++] = v3;
    }
  }

  /* Upload to GPU */
  GLuint vao = 0, vbo = 0, ebo = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               numVerts * STRIDE * (GLsizeiptr)sizeof(float), vtx, GL_STATIC_DRAW);

  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               numTris * 3 * (GLsizeiptr)sizeof(uint32_t), idx, GL_STATIC_DRAW);

  const GLsizei byteStride = STRIDE * (GLsizei)sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byteStride,
                        (void*)(0 * sizeof(float)));  /* aPosition */
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byteStride,
                        (void*)(3 * sizeof(float)));  /* aNormal   */
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byteStride,
                        (void*)(6 * sizeof(float)));  /* aTexCoord */
  glEnableVertexAttribArray(2);

  /* Activate built-in Phong shader and draw */
  ms->activatePhong(needNormals, /*hasColors=*/false, needTexCoords, /*hasTexture=*/false);
  glDrawElements(GL_TRIANGLES, numTris * 3, GL_UNSIGNED_INT, nullptr);
  ms->deactivate();

  /* Restore state and release GPU resources */
  glBindVertexArray(0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);

  delete[] vtx;
  delete[] idx;

  (void)state;   /* suppress unused-parameter warning for non-caching paths */
}

/* Forward declaration for sogl_render_sphere */
static void sogl_render_sphere_modern(SoGLModernState * ms, const float radius, const int numstacks, const int numslices, bool needNormals, bool needTexCoords, SoState * state);

void
sogl_render_sphere(const float radius,
                   const int numstacks,
                   const int numslices,
                   SoMaterialBundle * const /* material */,
                   const unsigned int flagsin,
                   SoState * state)
{
  /* Phase 1 modernization: use VAO+VBO path when a compiled SoGLModernState
   * is available for this context.  Fall through to the legacy immediate-mode
   * path if not (e.g. in headless / no-GLSL contexts). */
  if (state) {
    const SoGLContext * glue = sogl_glue_instance(state);
    if (glue) {
      uint32_t ctxid = SoGLContext_get_contextid(glue);
      SoGLModernState * ms = SoGLModernState::forContext(ctxid);
      if (ms && ms->isAvailable()) {
        sogl_render_sphere_modern(ms, radius, numstacks, numslices,
                                  (flagsin & SOGL_NEED_NORMALS)   != 0,
                                  (flagsin & SOGL_NEED_TEXCOORDS) != 0,
                                  state);
        if (SoComplexityTypeElement::get(state) ==
            SoComplexityTypeElement::OBJECT_SPACE) {
          SoGLCacheContextElement::shouldAutoCache(
              state, SoGLCacheContextElement::DO_AUTO_CACHE);
          SoGLCacheContextElement::incNumShapes(state);
        }
        else {
          SoGLCacheContextElement::shouldAutoCache(
              state, SoGLCacheContextElement::DONT_AUTO_CACHE);
        }
        return;
      }
    }
  }

  /* GL3: modern VAO/VBO path required; no fixed-function fallback. */
  SoDebugError::postWarning("sogl_render_sphere", "SoGLModernState unavailable; skipping render");
}

void
sogl_render_cube(const float width,
                 const float height,
                 const float depth,
                 SoMaterialBundle * const material,
                 const unsigned int flagsin,
                 SoState * state)
{
  /* Phase 1 modernization: use VAO+VBO path when SoGLModernState is available. */
  if (state) {
    const SoGLContext * glue = sogl_glue_instance(state);
    if (glue) {
      uint32_t ctxid = SoGLContext_get_contextid(glue);
      SoGLModernState * ms = SoGLModernState::forContext(ctxid);
      if (ms && ms->isAvailable()) {
        sogl_render_cube_modern(ms, width, height, depth,
                                (flagsin & SOGL_NEED_NORMALS)   != 0,
                                (flagsin & SOGL_NEED_TEXCOORDS) != 0);
        SoGLCacheContextElement::shouldAutoCache(
            state, SoGLCacheContextElement::DO_AUTO_CACHE);
        SoGLCacheContextElement::incNumShapes(state);
        (void)material;
        return;
      }
    }
  }

  /* GL3: modern VAO/VBO path required; no fixed-function fallback. */
  SoDebugError::postWarning("sogl_render_cube", "SoGLModernState unavailable; skipping render");
}

// **************************************************************************

#if !defined(NO_LINESET_RENDER)

namespace { namespace SoGL { namespace IndexedLineSet {

  enum AttributeBinding {
    OVERALL = 0,
    PER_SEGMENT = 1,
    PER_SEGMENT_INDEXED = 2,
    PER_LINE = 3,
    PER_LINE_INDEXED = 4,
    PER_VERTEX = 5,
    PER_VERTEX_INDEXED = 6
  };

  template < int NormalBinding,
             int MaterialBinding,
             int TexturingEnabled >
  static void GLRender(const SoGLContext * glue,
                       const SoGLCoordinateElement * coords,
                       const int32_t *indices,
                       int num_vertexindices,
                       const SbVec3f *normals,
                       const int32_t *normindices,
                       SoMaterialBundle *const materials,
                       const int32_t *matindices,
                       const SoTextureCoordinateBundle * const texcoords,
                       const int32_t *texindices,
                       const int drawAsPoints)
  {
    const SbVec3f * coords3d = NULL;
    const SbVec4f * coords4d = NULL;
    const SbBool is3d = coords->is3D();
    if (is3d) {
      coords3d = coords->getArrayPtr3();
    }
    else {
      coords4d = coords->getArrayPtr4();
    }
    int numcoords = coords->getNum();

    // This is the same code as in SoGLCoordinateElement::send().
    // It is inlined here for speed (~15% speed increase).
#define SEND_VERTEX(_idx_) \
    if (is3d) SoGLContext_glVertex3fv(glue, (const GLfloat*) (coords3d + _idx_)); \
    else SoGLContext_glVertex4fv(glue, (const GLfloat*) (coords4d + _idx_));

    // just in case someone forgot
    if (matindices == NULL) matindices = indices;
    if (normindices == NULL) normindices = indices;

    int matnr = 0;
    int texidx = 0;
    int32_t i;
    const int32_t *end = indices + num_vertexindices;

    SbVec3f dummynormal(0.0f, 0.0f, 1.0f);
    const SbVec3f *currnormal = &dummynormal;
    if (normals) currnormal = normals;
    if ((AttributeBinding)NormalBinding == OVERALL) {
      SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
    }

    if ((AttributeBinding)MaterialBinding == PER_SEGMENT ||
        (AttributeBinding)MaterialBinding == PER_SEGMENT_INDEXED ||
        (AttributeBinding)NormalBinding == PER_SEGMENT ||
        (AttributeBinding)NormalBinding == PER_SEGMENT_INDEXED) {
      int previ;

      if (drawAsPoints)
        SoGLContext_glBegin(glue, GL_POINTS);
      else
        SoGLContext_glBegin(glue, GL_LINES);

      while (indices < end) {
        previ = *indices++;

        // Variable used for counting errors and make sure not a bunch of
        // errormessages flood the screen.
        static uint32_t current_errors = 0;

        // This test is for robustness upon buggy data sets
        if (previ < 0 || previ >= numcoords) {
          if (current_errors < 1) {
            SoDebugError::postWarning("[indexedlineset]::GLRender", "Erroneous coordinate "
                                      "index: %d (Should be within [0, %d]). Aborting "
                                      "rendering. This message will be shown once, but "
                                      "there might be more errors", previ, numcoords - 1);
          }

          current_errors++;
          SoGLContext_glEnd(glue);
          return;
        }

        if ((AttributeBinding)MaterialBinding == PER_LINE ||
            (AttributeBinding)MaterialBinding == PER_VERTEX) {
          materials->send(matnr++, TRUE);
        } else if ((AttributeBinding)MaterialBinding == PER_LINE_INDEXED ||
                   (AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
          materials->send(*matindices++, TRUE);
        }

        if ((AttributeBinding)NormalBinding == PER_LINE ||
            (AttributeBinding)NormalBinding == PER_VERTEX) {
          currnormal = normals++;
          SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
        } else if ((AttributeBinding)NormalBinding == PER_LINE_INDEXED ||
                   (AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
          currnormal = &normals[*normindices++];
          SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
        }
        if (TexturingEnabled == TRUE) {
          texcoords->send(texindices ? *texindices++ : texidx++,coords->get3(previ), *currnormal);
        }
        i = (indices < end) ? *indices++ : -1;
        while (i >= 0) {
          // For robustness upon buggy data sets
          if (i >= numcoords) {
            if (current_errors < 1) {
              SoDebugError::postWarning("[indexedlineset]::GLRender", "Erroneous coordinate "
                                        "index: %d (Should be within [0, %d]). Aborting "
                                        "rendering. This message will be shown once, but "
                                        "there might be more errors", i, numcoords - 1);
            }
            current_errors++;
            break;
          }

          if ((AttributeBinding)MaterialBinding == PER_SEGMENT) {
            materials->send(matnr++, TRUE);
          } else if ((AttributeBinding)MaterialBinding == PER_SEGMENT_INDEXED) {
            materials->send(*matindices++, TRUE);
          }

          if ((AttributeBinding)NormalBinding == PER_SEGMENT) {
            currnormal = normals++;
            SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
          } else if ((AttributeBinding)NormalBinding == PER_SEGMENT_INDEXED) {
            currnormal = &normals[*normindices++];
            SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
          }
          SEND_VERTEX(previ);

          if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
            materials->send(matnr++, TRUE);
          } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
            materials->send(*matindices++, TRUE);
          }
          if ((AttributeBinding)NormalBinding == PER_VERTEX) {
            currnormal = normals++;
            SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
          } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
            currnormal = &normals[*normindices++];
            SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
          }
          if (TexturingEnabled == TRUE) {
            texcoords->send(texindices ? *texindices++ : texidx++, coords->get3(i), *currnormal);
          }
          SEND_VERTEX(i);
          previ = i;
          i = indices < end ? *indices++ : -1;
        }
        if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
          matindices++;
        }
        if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
          normindices++;
        }
        if (TexturingEnabled == TRUE) {
          if (texindices) texindices++;
        }
      }
      SoGLContext_glEnd(glue);

    } else { // no per_segment binding code below

      if (drawAsPoints)
        SoGLContext_glBegin(glue, GL_POINTS);

      while (indices < end) {
        if (!drawAsPoints)
          SoGLContext_glBegin(glue, GL_LINE_STRIP);

        i = *indices++;

        // Variable used for counting errors and make sure not a bunch of
        // errormessages flood the screen.
        static uint32_t current_errors = 0;

        // This test is for robustness upon buggy data sets
        if (i < 0 || i >= numcoords) {
          if (current_errors < 1) {
            SoDebugError::postWarning("[indexedlineset]::GLRender", "Erroneous coordinate "
                                      "index: %d (Should be within [0, %d]). Aborting "
                                      "rendering. This message will be shown once, but "
                                      "there might be more errors", i, numcoords - 1);
          }

          current_errors++;
          SoGLContext_glEnd(glue);
          return;
        }

        if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED ||
            (AttributeBinding)MaterialBinding == PER_LINE_INDEXED) {
          materials->send(*matindices++, TRUE);
        } else if ((AttributeBinding)MaterialBinding == PER_VERTEX ||
                   (AttributeBinding)MaterialBinding == PER_LINE) {
          materials->send(matnr++, TRUE);
        }

        if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
            (AttributeBinding)NormalBinding == PER_LINE_INDEXED) {
          currnormal = &normals[*normindices++];
          SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
        } else if ((AttributeBinding)NormalBinding == PER_VERTEX ||
                   (AttributeBinding)NormalBinding == PER_LINE) {
          currnormal = normals++;
          SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
        }
        if (TexturingEnabled == TRUE) {
          texcoords->send(texindices ? *texindices++ : texidx++, coords->get3(i), *currnormal);
        }

        SEND_VERTEX(i);
        i = indices < end ? *indices++ : -1;
        while (i >= 0) {
          // For robustness upon buggy data sets
          if (i >= numcoords) {
            if (current_errors < 1) {
              SoDebugError::postWarning("[indexedlineset]::GLRender", "Erroneous coordinate "
                                        "index: %d (Should be within [0, %d]). Aborting "
                                        "rendering. This message will be shown once, but "
                                        "there might be more errors", i, numcoords - 1);
            }
            current_errors++;
            break;
          }

          if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
            materials->send(matnr++, TRUE);
          } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
            materials->send(*matindices++, TRUE);
          }

          if ((AttributeBinding)NormalBinding == PER_VERTEX) {
            currnormal = normals++;
            SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
          } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
            currnormal = &normals[*normindices++];
            SoGLContext_glNormal3fv(glue, (const GLfloat*) currnormal);
          }
          if (TexturingEnabled == TRUE) {
            texcoords->send(texindices ? *texindices++ : texidx++, coords->get3(i), *currnormal);
          }

          SEND_VERTEX(i);
          i = indices < end ? *indices++ : -1;
        }
        if (!drawAsPoints)
          SoGLContext_glEnd(glue); // end of line strip

        if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
          matindices++;
        }
        if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
          normindices++;
        }
        if (TexturingEnabled == TRUE) {
          if (texindices) texindices++;
        }
      }
      if (drawAsPoints)
        SoGLContext_glEnd(glue);
    }
  }

} } } // namespace

#define SOGL_INDEXEDLINESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, texturing, args) \
  SoGL::IndexedLineSet::GLRender<normalbinding, materialbinding, texturing> args

#define SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, materialbinding, texturing, args) \
  if (texturing) { \
    SOGL_INDEXEDLINESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, TRUE, args); \
  } else { \
    SOGL_INDEXEDLINESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, FALSE, args); \
  }

#define SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(normalbinding, materialbinding, texturing, args) \
  switch (materialbinding) { \
  case SoGL::IndexedLineSet::OVERALL: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::OVERALL, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_SEGMENT: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::PER_SEGMENT, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_SEGMENT_INDEXED: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::PER_SEGMENT_INDEXED, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_LINE: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::PER_LINE, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_LINE_INDEXED: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::PER_LINE_INDEXED, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_VERTEX: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::PER_VERTEX, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_VERTEX_INDEXED: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::IndexedLineSet::PER_VERTEX_INDEXED, texturing, args); \
    break; \
  default: \
    assert(!"invalid material binding argument"); \
  }

#define SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG1(normalbinding, materialbinding, texturing, args) \
  switch (normalbinding) { \
  case SoGL::IndexedLineSet::OVERALL: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::OVERALL, materialbinding, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_SEGMENT: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::PER_SEGMENT, materialbinding, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_SEGMENT_INDEXED: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::PER_SEGMENT_INDEXED, materialbinding, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_LINE: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::PER_LINE, materialbinding, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_LINE_INDEXED: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::PER_LINE_INDEXED, materialbinding, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_VERTEX: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::PER_VERTEX, materialbinding, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_VERTEX_INDEXED: \
    SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2(SoGL::IndexedLineSet::PER_VERTEX_INDEXED, materialbinding, texturing, args); \
    break; \
  default: \
    assert(!"invalid normal binding argument"); \
  }

#define SOGL_INDEXEDLINESET_GLRENDER(normalbinding, materialbinding, texturing, args) \
  SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG1(normalbinding, materialbinding, texturing, args)

void
sogl_render_lineset(const SoGLContext * glue,
                    const SoGLCoordinateElement * const coords,
                    const int32_t *cindices,
                    int numindices,
                    const SbVec3f *normals,
                    const int32_t *nindices,
                    SoMaterialBundle *const mb,
                    const int32_t *mindices,
                    const SoTextureCoordinateBundle * const tb,
                    const int32_t *tindices,
                    int nbind,
                    int mbind,
                    const int texture,
                    const int drawAsPoints)
{

  SOGL_INDEXEDLINESET_GLRENDER(nbind, mbind, texture, (glue,
                                                       coords,
                                                       cindices,
                                                       numindices,
                                                       normals,
                                                       nindices,
                                                       mb,
                                                       mindices,
                                                       tb,
                                                       tindices,
                                                       drawAsPoints));
}

#undef SOGL_INDEXEDLINESET_GLRENDER_CALL_FUNC
#undef SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG1
#undef SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG2
#undef SOGL_INDEXEDLINESET_GLRENDER_RESOLVE_ARG3
#undef SOGL_INDEXEDLINESET_GLRENDER

#endif // !NO_LINESET_RENDER

// **************************************************************************

#if !defined(NO_FACESET_RENDER)

namespace { namespace SoGL { namespace FaceSet {

  enum AttributeBinding {
    OVERALL = 0,
    PER_FACE = 1,
    PER_FACE_INDEXED = 2,
    PER_VERTEX = 3,
    PER_VERTEX_INDEXED = 4
  };

  template < int NormalBinding,
             int MaterialBinding,
             int VertexAttributeBinding >
  static void GLRender(const SoGLContext * glue,
                       const SoGLCoordinateElement * const vertexlist,
                     const int32_t *vertexindices,
                     int numindices,
                     const SbVec3f *normals,
                     const int32_t *normalindices,
                     SoMaterialBundle *materials,
                     const int32_t *matindices,
                     const SoTextureCoordinateBundle * const texcoords,
                     const int32_t *texindices,
                       SoVertexAttributeBundle * const attribs,
                       const int dotexture,
                       const int doattribs)
  {

    // just in case someone forgot
    if (matindices == NULL) matindices = vertexindices;
    if (normalindices == NULL) normalindices = vertexindices;

    int texidx = 0;

    const SbVec3f * coords3d = NULL;
    const SbVec4f * coords4d = NULL;
    const SbBool is3d = vertexlist->is3D();
    if (is3d) {
      coords3d = vertexlist->getArrayPtr3();
    }
    else {
      coords4d = vertexlist->getArrayPtr4();
    }

    // This is the same code as in SoGLCoordinateElement::send().
    // It is inlined here for speed (~15% speed increase).
#define SEND_VERTEX(_idx_)                                           \
    if (is3d) SoGLContext_glVertex3fv(glue, (const GLfloat*) (coords3d + _idx_));             \
    else SoGLContext_glVertex4fv(glue, (const GLfloat*) (coords4d + _idx_));

    int mode = 0x0009 /*GL_POLYGON*/; // ...to save a test
    int newmode;
    const int32_t *viptr = vertexindices;
    const int32_t *vistartptr = vertexindices;
    const int32_t *viendptr = viptr + numindices;
    int32_t v1, v2, v3, v4, v5 = 0; // v5 init unnecessary, but kills a compiler warning.
    int numverts = vertexlist->getNum();

    SbVec3f dummynormal(0,0,1);
    const SbVec3f * currnormal = &dummynormal;
    if ((AttributeBinding)NormalBinding == PER_VERTEX ||
       (AttributeBinding)NormalBinding == PER_FACE ||
       (AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
       (AttributeBinding)NormalBinding == PER_FACE_INDEXED ||
       dotexture) {
      if (normals) currnormal = normals;
    }

    int matnr = 0;
    int attribnr = 0;

    if (doattribs && (AttributeBinding)VertexAttributeBinding == OVERALL) {
      attribs->send(0);
    }

    while (viptr + 2 < viendptr) {
      v1 = *viptr++;
      v2 = *viptr++;
      v3 = *viptr++;

      // Variable used for counting errors and make sure not a
      // bunch of errormessages flood the screen.
      static uint32_t current_errors = 0;

      // This test is for robustness upon buggy data sets
      if (v1 < 0 || v2 < 0 || v3 < 0 ||
          v1 >= numverts || v2 >= numverts || v3 >= numverts) {

        if (current_errors < 1) {
          SoDebugError::postWarning("[faceset]::GLRender", "Erroneous polygon detected. "
                                    "Ignoring (offset: %d, [%d %d %d]). Should be within "
                                    " [0, %d] This message will only be shown once, but "
                                    "more errors might be present",
                                    viptr - vistartptr - 3, v1, v2, v3, numverts - 1);
        }
        current_errors++;
        break;
      }
      v4 = viptr < viendptr ? *viptr++ : -1;
      if (v4  < 0) newmode = GL_TRIANGLES;
      // This test for numverts is for robustness upon buggy data sets
      else if (v4 >= numverts) {
        newmode = GL_TRIANGLES;

        if (current_errors < 1) {
          SoDebugError::postWarning("[faceset]::GLRender", "Erroneous polygon detected. "
                                    "(offset: %d, [%d %d %d %d]). Should be within "
                                    " [0, %d] This message will only be shown once, but "
                                    "more errors might be present",
                                    viptr - vistartptr - 4, v1, v2, v3, v4, numverts - 1);
        }
        current_errors++;
      }
      else {
        v5 = viptr < viendptr ? *viptr++ : -1;
        if (v5 < 0) newmode = 0x0007 /*GL_QUADS*/;
        // This test for numverts is for robustness upon buggy data sets
        else if (v5 >= numverts) {
          newmode = 0x0007 /*GL_QUADS*/;

          if (current_errors < 1) {
            SoDebugError::postWarning("[faceset]::GLRender", "Erroneous polygon detected. "
                                      "(offset: %d, [%d %d %d %d %d]). Should be within "
                                      " [0, %d] This message will only be shown once, but "
                                      "more errors might be present",
                                      viptr - vistartptr - 5, v1, v2, v3, v4, v5, numverts - 1);
          }
          current_errors++;
        }
        else newmode = 0x0009 /*GL_POLYGON*/;
      }
      if (newmode != mode) {
        if (mode != 0x0009 /*GL_POLYGON*/) SoGLContext_glEnd(glue);
        mode = newmode;
        SoGLContext_glBegin(glue, (GLenum) mode);
      }
      else if (mode == 0x0009 /*GL_POLYGON*/) SoGLContext_glBegin(glue, 0x0009 /*GL_POLYGON*/);

      /* vertex 1 *********************************************************/
      if ((AttributeBinding)MaterialBinding == PER_VERTEX ||
          (AttributeBinding)MaterialBinding == PER_FACE) {
        materials->send(matnr++, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED ||
                 (AttributeBinding)MaterialBinding == PER_FACE_INDEXED) {
        materials->send(*matindices++, TRUE);
      }

      if ((AttributeBinding)NormalBinding == PER_VERTEX ||
          (AttributeBinding)NormalBinding == PER_FACE) {
        currnormal = normals++;
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
                 (AttributeBinding)NormalBinding == PER_FACE_INDEXED) {
        currnormal = &normals[*normalindices++];
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      }

      if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX) {
       attribs->send(attribnr++);
      } else if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
        attribs->send(*vertexindices++);
      }

      if (dotexture) {
        texcoords->send(texindices ? *texindices++ : texidx++,
                        vertexlist->get3(v1),
                        *currnormal);
      }

      SEND_VERTEX(v1);

      /* vertex 2 *********************************************************/
      if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
        materials->send(matnr++, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
        materials->send(*matindices++, TRUE);
      }

      // re-send per-face material for each vertex to ensure correct colour on all drivers
      if ((AttributeBinding)MaterialBinding == PER_FACE) {
        materials->send(matnr-1, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_FACE_INDEXED) {
        materials->send(matindices[-1], TRUE);
      }

      if ((AttributeBinding)NormalBinding == PER_VERTEX) {
        currnormal = normals++;
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
        currnormal = &normals[*normalindices++];
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      }

      if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX) {
       attribs->send(attribnr++);
      } else if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
        attribs->send(*vertexindices++);
      }

      if (dotexture) {
        texcoords->send(texindices ? *texindices++ : texidx++,
                        vertexlist->get3(v2),
                        *currnormal);
      }

      SEND_VERTEX(v2);

      /* vertex 3 *********************************************************/
      if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
        materials->send(matnr++, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
        materials->send(*matindices++, TRUE);
      }

      // re-send per-face material for each vertex to ensure correct colour on all drivers
      if ((AttributeBinding)MaterialBinding == PER_FACE) {
        materials->send(matnr-1, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_FACE_INDEXED) {
        materials->send(matindices[-1], TRUE);
      }

      if ((AttributeBinding)NormalBinding == PER_VERTEX) {
        currnormal = normals++;
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
        currnormal = &normals[*normalindices++];
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      }

      if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX) {
       attribs->send(attribnr++);
      } else if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
        attribs->send(*vertexindices++);
      }

      if (dotexture) {
        texcoords->send(texindices ? *texindices++ : texidx++,
                        vertexlist->get3(v3),
                        *currnormal);
      }

      SEND_VERTEX(v3);

      if (mode != GL_TRIANGLES) {
        /* vertex 4 (quad or polygon)**************************************/
        if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
          materials->send(matnr++, TRUE);
        } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
          materials->send(*matindices++, TRUE);
        }

        // re-send per-face material for each vertex to ensure correct colour on all drivers
        if ((AttributeBinding)MaterialBinding == PER_FACE) {
          materials->send(matnr-1, TRUE);
        } else if ((AttributeBinding)MaterialBinding == PER_FACE_INDEXED) {
          materials->send(matindices[-1], TRUE);
        }

        if ((AttributeBinding)NormalBinding == PER_VERTEX) {
          currnormal = normals++;
          SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
        } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
          currnormal = &normals[*normalindices++];
          SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
        }

        if (dotexture) {
          texcoords->send(texindices ? *texindices++ : texidx++,
                          vertexlist->get3(v4),
                          *currnormal);
        }

        if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX) {
          attribs->send(attribnr++);
        } 
        else if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
          attribs->send(*vertexindices++);
        }
        SEND_VERTEX(v4);

        if (mode == 0x0009 /*GL_POLYGON*/) {
          /* vertex 5 (polygon) ********************************************/
          if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
            materials->send(matnr++, TRUE);
          } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
            materials->send(*matindices++, TRUE);
          }

          // re-send per-face material for each vertex to ensure correct colour on all drivers
          if ((AttributeBinding)MaterialBinding == PER_FACE) {
            materials->send(matnr-1, TRUE);
          } else if ((AttributeBinding)MaterialBinding == PER_FACE_INDEXED) {
            materials->send(matindices[-1], TRUE);
          }

          if ((AttributeBinding)NormalBinding == PER_VERTEX) {
            currnormal = normals++;
            SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
          } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
            currnormal = &normals[*normalindices++];
            SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
          }

          if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX) {
            attribs->send(attribnr++);
          } else if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
            attribs->send(*vertexindices++);
          }

          if (dotexture) {
            texcoords->send(texindices ? *texindices++ : texidx++,
                            vertexlist->get3(v5),
                            *currnormal);

          }

          SEND_VERTEX(v5);

          v1 = viptr < viendptr ? *viptr++ : -1;
          while (v1 >= 0) {
            // For robustness upon buggy data sets
            if (v1 >= numverts) {
              if (current_errors < 1) {
                SoDebugError::postWarning("[faceset]::GLRender", "Erroneous polygon detected. "
                                          "(offset: %d, [... %d]). Should be within "
                                          "[0, %d] This message will only be shown once, but "
                                          "more errors might be present",
                                          viptr - vistartptr - 1, v1, numverts - 1);
              }
              current_errors++;
              break;
            }

            /* vertex 6-n (polygon) *****************************************/
            if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
              materials->send(matnr++, TRUE);
            } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
              materials->send(*matindices++, TRUE);
            }

            // re-send per-face material for each vertex to ensure correct colour on all drivers
            if ((AttributeBinding)MaterialBinding == PER_FACE) {
              materials->send(matnr-1, TRUE);
            } else if ((AttributeBinding)MaterialBinding == PER_FACE_INDEXED) {
              materials->send(matindices[-1], TRUE);
            }

            if ((AttributeBinding)NormalBinding == PER_VERTEX) {
              currnormal = normals++;
              SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
            } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
              currnormal = &normals[*normalindices++];
              SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
            }

            if (dotexture) {
              texcoords->send(texindices ? *texindices++ : texidx++,
                              vertexlist->get3(v1),
                              *currnormal);
            }

            if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX) {
              attribs->send(attribnr++);
            } else if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
              attribs->send(*vertexindices++);
            }
            SEND_VERTEX(v1);

            v1 = viptr < viendptr ? *viptr++ : -1;
          }
          SoGLContext_glEnd(glue); /* draw polygon */
        }
      }

      if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
        matindices++;
      }
      if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
        normalindices++;
      }
      if ((AttributeBinding)VertexAttributeBinding == PER_VERTEX_INDEXED) {
        vertexindices++;
      }

      if (dotexture) {
        if (texindices) texindices++;
      }
    }
    // check if triangle or quad
    if (mode != 0x0009 /*GL_POLYGON*/) SoGLContext_glEnd(glue);
  }

} } } // namespace

#define SOGL_FACESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, vertexattributebinding, args) \
  SoGL::FaceSet::GLRender<normalbinding, materialbinding, vertexattributebinding> args

#define SOGL_FACESET_GLRENDER_RESOLVE_ARG3(normalbinding, materialbinding, vertexattributebinding, args) \
  switch (vertexattributebinding) { \
  case SoGL::FaceSet::OVERALL: \
    SOGL_FACESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, SoGL::FaceSet::OVERALL, args); \
    break; \
  case SoGL::FaceSet::PER_FACE: \
    SOGL_FACESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, SoGL::FaceSet::OVERALL, args); \
    break; \
  case SoGL::FaceSet::PER_FACE_INDEXED: \
    SOGL_FACESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, SoGL::FaceSet::OVERALL, args); \
    break; \
  case SoGL::FaceSet::PER_VERTEX: \
    SOGL_FACESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, SoGL::FaceSet::PER_VERTEX, args); \
    break; \
  case SoGL::FaceSet::PER_VERTEX_INDEXED: \
    SOGL_FACESET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, SoGL::FaceSet::PER_VERTEX_INDEXED, args); \
    break; \
  default: \
    assert(!"invalid vertex attribute binding argument"); \
  }

#define SOGL_FACESET_GLRENDER_RESOLVE_ARG2(normalbinding, materialbinding, vertexattributebinding, args) \
  switch (materialbinding) { \
  case SoGL::FaceSet::OVERALL: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::FaceSet::OVERALL, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_FACE: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::FaceSet::PER_FACE, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_FACE_INDEXED: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::FaceSet::PER_FACE_INDEXED, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_VERTEX: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::FaceSet::PER_VERTEX, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_VERTEX_INDEXED: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::FaceSet::PER_VERTEX_INDEXED, vertexattributebinding, args); \
    break; \
  default: \
    assert(!"invalid material binding argument"); \
  }

#define SOGL_FACESET_GLRENDER_RESOLVE_ARG1(normalbinding, materialbinding, vertexattributebinding, args) \
  switch (normalbinding) { \
  case SoGL::FaceSet::OVERALL: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG2(SoGL::FaceSet::OVERALL, materialbinding, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_FACE: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG2(SoGL::FaceSet::PER_FACE, materialbinding, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_FACE_INDEXED: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG2(SoGL::FaceSet::PER_FACE_INDEXED, materialbinding, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_VERTEX: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG2(SoGL::FaceSet::PER_VERTEX, materialbinding, vertexattributebinding, args); \
    break; \
  case SoGL::FaceSet::PER_VERTEX_INDEXED: \
    SOGL_FACESET_GLRENDER_RESOLVE_ARG2(SoGL::FaceSet::PER_VERTEX_INDEXED, materialbinding, vertexattributebinding, args); \
    break; \
  default: \
    assert(!"invalid normal binding argument"); \
  }

#define SOGL_FACESET_GLRENDER(normalbinding, materialbinding, vertexattributebinding, args) \
  SOGL_FACESET_GLRENDER_RESOLVE_ARG1(normalbinding, materialbinding, vertexattributebinding, args)


void
sogl_render_faceset(const SoGLContext * glue,
                    const SoGLCoordinateElement * const vertexlist,
                    const int32_t *vertexindices,
                    int num_vertexindices,
                    const SbVec3f *normals,
                    const int32_t *normindices,
                    SoMaterialBundle *const materials,
                    const int32_t *matindices,
                    SoTextureCoordinateBundle * const texcoords,
                    const int32_t *texindices,
                    SoVertexAttributeBundle * const attribs,
                    const int nbind,
                    const int mbind,
                    const int attribbind,
                    const int dotexture,
                    const int doattribs)
{
  SOGL_FACESET_GLRENDER(nbind, mbind, attribbind, (glue,
                                                   vertexlist,
                                                   vertexindices,
                                                   num_vertexindices,
                                                   normals,
                                                   normindices,
                                                   materials,
                                                   matindices,
                                                   texcoords,
                                                   texindices,
                                                   attribs,
                                                   dotexture,
                                                   doattribs));
}

#undef SOGL_FACESET_GLRENDER
#undef SOGL_FACESET_GLRENDER_RESOLVE_ARG1
#undef SOGL_FACESET_GLRENDER_RESOLVE_ARG2
#undef SOGL_FACESET_GLRENDER_RESOLVE_ARG3
#undef SOGL_FACESET_GLRENDER_RESOLVE_ARG4
#undef SOGL_FACESET_GLRENDER_CALL_FUNC

#endif // !NO_FACESET_RENDER

// **************************************************************************

//typedef void sogl_render_faceset_func(const SoGLCoordinateElement * const coords,
//                                      const int32_t *vertexindices,
//                                      int num_vertexindices,
//                                      const SbVec3f *normals,
//                                      const int32_t *normindices,
//                                      SoMaterialBundle *materials,
//                                      const int32_t *matindices,
//                                      const SoTextureCoordinateBundle * const texcoords,
//                                      const int32_t *texindices);

#if !defined(NO_TRISTRIPSET_RENDER)

namespace { namespace SoGL { namespace TriStripSet {

  enum AttributeBinding {
    OVERALL = 0,
    PER_STRIP = 1,
    PER_STRIP_INDEXED = 2,
    PER_TRIANGLE = 3,
    PER_TRIANGLE_INDEXED = 4,
    PER_VERTEX = 5,
    PER_VERTEX_INDEXED = 6
  };

  template < int NormalBinding,
             int MaterialBinding,
             int TexturingEnabled >
  static void GLRender(const SoGLContext * glue,
                       const SoGLCoordinateElement * const vertexlist,
                       const int32_t *vertexindices,
                       int numindices,
                       const SbVec3f *normals,
                       const int32_t *normalindices,
                       SoMaterialBundle *materials,
                       const int32_t *matindices,
                       const SoTextureCoordinateBundle * const texcoords,
                       const int32_t *texindices)
  {

    // just in case someone forgot...
    if (matindices == NULL) matindices = vertexindices;
    if (normalindices == NULL) normalindices = vertexindices;

    int texidx = 0;
    const int32_t *viptr = vertexindices;
    const int32_t *vistartptr = vertexindices;
    const int32_t *viendptr = viptr + numindices;
    int32_t v1, v2, v3;
    int numverts = vertexlist->getNum();

    const SbVec3f * coords3d = NULL;
    const SbVec4f * coords4d = NULL;
    const SbBool is3d = vertexlist->is3D();
    if (is3d) {
      coords3d = vertexlist->getArrayPtr3();
    }
    else {
      coords4d = vertexlist->getArrayPtr4();
    }

    // This is the same code as in SoGLCoordinateElement::send().
    // It is inlined here for speed (~15% speed increase).
#define SEND_VERTEX_TRISTRIP(_idx_) \
    if (is3d) SoGLContext_glVertex3fv(glue, (const GLfloat*) (coords3d + _idx_)); \
    else SoGLContext_glVertex4fv(glue, (const GLfloat*) (coords4d + _idx_));

    if ((AttributeBinding)NormalBinding == PER_VERTEX ||
        (AttributeBinding)NormalBinding == PER_TRIANGLE ||
        (AttributeBinding)NormalBinding == PER_STRIP ||
        (AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
        (AttributeBinding)NormalBinding == PER_TRIANGLE_INDEXED ||
        (AttributeBinding)NormalBinding == PER_STRIP_INDEXED) {
      assert(normals && "Aborting rendering of tristrip; got NULL normals");
    }

    SbVec3f dummynormal(0.0f, 0.0f, 1.0f);
    const SbVec3f *currnormal = &dummynormal;

    if ((AttributeBinding)NormalBinding == PER_VERTEX ||
        (AttributeBinding)NormalBinding == PER_TRIANGLE ||
        (AttributeBinding)NormalBinding == PER_STRIP ||
        (AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
        (AttributeBinding)NormalBinding == PER_TRIANGLE_INDEXED ||
        (AttributeBinding)NormalBinding == PER_STRIP_INDEXED ||
        TexturingEnabled == TRUE) {
      if (normals) currnormal = normals;
    }

    int matnr = 0;

    while (viptr + 2 < viendptr) {
      v1 = *viptr++;
      v2 = *viptr++;
      v3 = *viptr++;

      // This should be here to prevent illegal polygons from being rendered
      if (v1 < 0 || v2 < 0 || v3 < 0 ||
          v1 >= numverts || v2 >= numverts || v3 >= numverts) {

        static uint32_t current_errors = 0;
        if (current_errors < 1) {
          SoDebugError::postWarning("[tristrip]::GLRender", "Erroneous polygon detected. "
                                    "Ignoring (offset: %d, [%d %d %d]). Should be within "
                                    " [0, %d] This message will only be shown once, but "
                                    "more errors may be present",
                                    viptr - vistartptr - 3, v1, v2, v3, numverts - 1);
        }

        current_errors++;
        break;
      }

      SoGLContext_glBegin(glue, GL_TRIANGLE_STRIP);

      /* vertex 1 *********************************************************/
      if ((AttributeBinding)MaterialBinding == PER_VERTEX ||
          (AttributeBinding)MaterialBinding == PER_STRIP ||
          (AttributeBinding)MaterialBinding == PER_TRIANGLE) {
        materials->send(matnr++, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED ||
                 (AttributeBinding)MaterialBinding == PER_STRIP_INDEXED ||
                 (AttributeBinding)MaterialBinding == PER_TRIANGLE_INDEXED) {
        materials->send(*matindices++, TRUE);
      }
      if ((AttributeBinding)NormalBinding == PER_VERTEX ||
          (AttributeBinding)NormalBinding == PER_STRIP ||
          (AttributeBinding)NormalBinding == PER_TRIANGLE) {
        currnormal = normals++;
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
                 (AttributeBinding)NormalBinding == PER_TRIANGLE_INDEXED ||
                 (AttributeBinding)NormalBinding == PER_STRIP_INDEXED) {
        currnormal = &normals[*normalindices++];
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      }
      if (TexturingEnabled == TRUE) {
        texcoords->send(texindices ? *texindices++ : texidx++,
                        vertexlist->get3(v1),
                        *currnormal);
      }
      SEND_VERTEX_TRISTRIP(v1);

      /* vertex 2 *********************************************************/
      if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
        materials->send(matnr++, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
        materials->send(*matindices++, TRUE);
      }

      // re-send per-strip/triangle material for each vertex to ensure correct colour on all drivers
      if ((AttributeBinding)MaterialBinding == PER_TRIANGLE ||
          (AttributeBinding)MaterialBinding == PER_STRIP) {
        materials->send(matnr-1, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_TRIANGLE_INDEXED ||
                 (AttributeBinding)MaterialBinding == PER_STRIP_INDEXED) {
        materials->send(matindices[-1], TRUE);
      }

      if ((AttributeBinding)NormalBinding == PER_VERTEX) {
        currnormal = normals++;
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
        currnormal = &normals[*normalindices++];
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      }
      if (TexturingEnabled == TRUE) {
        texcoords->send(texindices ? *texindices++ : texidx++,
                        vertexlist->get3(v2),
                        *currnormal);
      }
      SEND_VERTEX_TRISTRIP(v2);

      /* vertex 3 *********************************************************/
      if ((AttributeBinding)MaterialBinding == PER_VERTEX) {
        materials->send(matnr++, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
        materials->send(*matindices++, TRUE);
      }

      // re-send per-strip/triangle material for each vertex to ensure correct colour on all drivers
      if ((AttributeBinding)MaterialBinding == PER_STRIP ||
          (AttributeBinding)MaterialBinding == PER_TRIANGLE) {
        materials->send(matnr-1, TRUE);
      } else if ((AttributeBinding)MaterialBinding == PER_TRIANGLE_INDEXED ||
                 (AttributeBinding)MaterialBinding == PER_STRIP_INDEXED) {
        materials->send(matindices[-1], TRUE);
      }

      if ((AttributeBinding)NormalBinding == PER_VERTEX) {
        currnormal = normals++;
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
        currnormal = &normals[*normalindices++];
        SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
      }
      if (TexturingEnabled == TRUE) {
        texcoords->send(texindices ? *texindices++ : texidx++,
                        vertexlist->get3(v3),
                        *currnormal);
      }
      SEND_VERTEX_TRISTRIP(v3);

      v1 = viptr < viendptr ? *viptr++ : -1;
      while (v1 >= 0) {
        if ((AttributeBinding)MaterialBinding == PER_VERTEX ||
            (AttributeBinding)MaterialBinding == PER_TRIANGLE) {
          materials->send(matnr++, TRUE);
        } else if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED ||
                   (AttributeBinding)MaterialBinding == PER_TRIANGLE_INDEXED) {
          materials->send(*matindices++, TRUE);
        }

        // re-send per-strip/triangle material for each vertex to ensure correct colour on all drivers
        if ((AttributeBinding)MaterialBinding == PER_STRIP) {
          materials->send(matnr-1, TRUE);
        } else if ((AttributeBinding)MaterialBinding == PER_STRIP_INDEXED) {
          materials->send(matindices[-1], TRUE);
        }
  
        if ((AttributeBinding)NormalBinding == PER_VERTEX ||
            (AttributeBinding)NormalBinding == PER_TRIANGLE) {
          currnormal = normals++;
          SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
        } else if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED ||
                   (AttributeBinding)NormalBinding == PER_TRIANGLE_INDEXED) {
          currnormal = &normals[*normalindices++];
          SoGLContext_glNormal3fv(glue, (const GLfloat*)currnormal);
        }
        if (TexturingEnabled == TRUE) {
          texcoords->send(texindices ? *texindices++ : texidx++,
                          vertexlist->get3(v1),
                          *currnormal);
        }

        SEND_VERTEX_TRISTRIP(v1);
        v1 = viptr < viendptr ? *viptr++ : -1;
      }
      SoGLContext_glEnd(glue); // end of tristrip

      if ((AttributeBinding)MaterialBinding == PER_VERTEX_INDEXED) {
        matindices++;
      }
      if ((AttributeBinding)NormalBinding == PER_VERTEX_INDEXED) {
        normalindices++;
      }
      if (TexturingEnabled == TRUE) {
        if (texindices) texindices++;
      }
    }
  }

} } } // namespace

#define SOGL_TRISTRIPSET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, texturing, args) \
  SoGL::TriStripSet::GLRender<normalbinding, materialbinding, texturing> args

#define SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, materialbinding, texturing, args) \
  if (texturing) { \
    SOGL_TRISTRIPSET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, TRUE, args); \
  } else { \
    SOGL_TRISTRIPSET_GLRENDER_CALL_FUNC(normalbinding, materialbinding, FALSE, args); \
  }

#define SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(normalbinding, materialbinding, texturing, args) \
  switch (materialbinding) { \
  case SoGL::TriStripSet::OVERALL: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::OVERALL, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_STRIP: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::PER_STRIP, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_STRIP_INDEXED: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::PER_STRIP_INDEXED, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_TRIANGLE: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::PER_TRIANGLE, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_TRIANGLE_INDEXED: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::PER_TRIANGLE_INDEXED, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_VERTEX: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::PER_VERTEX, texturing, args); \
    break; \
  case SoGL::IndexedLineSet::PER_VERTEX_INDEXED: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3(normalbinding, SoGL::TriStripSet::PER_VERTEX_INDEXED, texturing, args); \
    break; \
  default: \
    assert(!"invalid material binding argument"); \
  }

#define SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG1(normalbinding, materialbinding, texturing, args) \
  switch (normalbinding) { \
  case SoGL::TriStripSet::OVERALL: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::OVERALL, materialbinding, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_STRIP: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::PER_STRIP, materialbinding, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_STRIP_INDEXED: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::PER_STRIP_INDEXED, materialbinding, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_TRIANGLE: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::PER_TRIANGLE, materialbinding, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_TRIANGLE_INDEXED: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::PER_TRIANGLE_INDEXED, materialbinding, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_VERTEX: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::PER_VERTEX, materialbinding, texturing, args); \
    break; \
  case SoGL::TriStripSet::PER_VERTEX_INDEXED: \
    SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2(SoGL::TriStripSet::PER_VERTEX_INDEXED, materialbinding, texturing, args); \
    break; \
  default: \
    assert(!"invalid normal binding argument"); \
  }

#define SOGL_TRISTRIPSET_GLRENDER(normalbinding, materialbinding, texturing, args) \
  SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG1(normalbinding, materialbinding, texturing, args)

void
sogl_render_tristrip(const SoGLContext * glue,
                     const SoGLCoordinateElement * const vertexlist,
                     const int32_t *vertexindices,
                     int num_vertexindices,
                     const SbVec3f *normals,
                     const int32_t *normindices,
                     SoMaterialBundle *const materials,
                     const int32_t *matindices,
                     const SoTextureCoordinateBundle * const texcoords,
                     const int32_t *texindices,
                     const int nbind,
                     const int mbind,
                     const int texture)
{
  SOGL_TRISTRIPSET_GLRENDER(nbind, mbind, texture, (glue,
                                                    vertexlist,
                                                    vertexindices,
                                                    num_vertexindices,
                                                    normals,
                                                    normindices,
                                                    materials,
                                                    matindices,
                                                    texcoords,
                                                    texindices));
}

#undef SOGL_TRISTRIPSET_GLRENDER_CALL_FUNC
#undef SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG1
#undef SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG2
#undef SOGL_TRISTRIPSET_GLRENDER_RESOLVE_ARG3
#undef SOGL_TRISTRIPSET_GLRENDER

#endif // !NO_TRISTRIPSET_RENDER


// PointSet rendering
// here we include the 8 variations directly...

static void
sogl_render_pointset_m0n0t0(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * OBOL_UNUSED_ARG(normals),
                            SoMaterialBundle * OBOL_UNUSED_ARG(mb),
                            const SoTextureCoordinateBundle * OBOL_UNUSED_ARG(tb),
                            int32_t numpts,
                            int32_t idx)
{
  int i;
  const int unroll = numpts >> 2;
  const int rest = numpts & 3;

  // manually unroll this common loop

  SoGLContext_glBegin(glue, GL_POINTS);
  for (i = 0; i < unroll; i++) {
    coords->send(idx++);
    coords->send(idx++);
    coords->send(idx++);
    coords->send(idx++);
  }
  for (i = 0; i < rest; i++) {
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m0n0t1(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * OBOL_UNUSED_ARG(normals),
                            SoMaterialBundle * OBOL_UNUSED_ARG(mb),
                            const SoTextureCoordinateBundle * tb,
                            int32_t numpts,
                            int32_t idx)
{
  int texnr = 0;
  const SbVec3f currnormal(0.0f,0.0f,1.0f);

  SoGLContext_glBegin(glue, GL_POINTS);
  for (int i = 0; i < numpts; i++) {
    tb->send(texnr++, coords->get3(idx), currnormal);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m0n1t0(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * normals,
                            SoMaterialBundle * OBOL_UNUSED_ARG(mb),
                            const SoTextureCoordinateBundle * OBOL_UNUSED_ARG(tb),
                            int32_t numpts,
                            int32_t idx)
{
  SoGLContext_glBegin(glue, GL_POINTS);
  for (int i = 0; i < numpts; i++) {
    SoGLContext_glNormal3fv(glue, (const GLfloat*)normals++);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m0n1t1(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * normals,
                            SoMaterialBundle * OBOL_UNUSED_ARG(mb),
                            const SoTextureCoordinateBundle * tb,
                            int32_t numpts,
                            int32_t idx)
{
  int texnr = 0;
  const SbVec3f currnormal(0.0f,0.0f,1.0f);

  SoGLContext_glBegin(glue, GL_POINTS);
  for (int i = 0; i < numpts; i++) {
    SoGLContext_glNormal3fv(glue, (const GLfloat*)normals++);
    tb->send(texnr++, coords->get3(idx), currnormal);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m1n0t0(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * OBOL_UNUSED_ARG(normals),
                            SoMaterialBundle * mb,
                            const SoTextureCoordinateBundle * OBOL_UNUSED_ARG(tb),
                            int32_t numpts,
                            int32_t idx)
{
  int i;
  int matnr = 0;
  const int unroll = numpts >> 2;
  const int rest = numpts & 3;

  // manually unroll this common loop

  SoGLContext_glBegin(glue, GL_POINTS);
  for (i = 0; i < unroll; i++) {
    mb->send(matnr++, TRUE);
    coords->send(idx++);
    mb->send(matnr++, TRUE);
    coords->send(idx++);
    mb->send(matnr++, TRUE);
    coords->send(idx++);
    mb->send(matnr++, TRUE);
    coords->send(idx++);
  }
  for (i = 0; i < rest; i++) {
    mb->send(matnr++, TRUE);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m1n0t1(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * OBOL_UNUSED_ARG(normals),
                            SoMaterialBundle * mb,
                            const SoTextureCoordinateBundle * tb,
                            int32_t numpts,
                            int32_t idx)
{
  int matnr = 0;
  int texnr = 0;
  const SbVec3f currnormal(0.0f,0.0f,1.0f);

  SoGLContext_glBegin(glue, GL_POINTS);
  for (int i = 0; i < numpts; i++) {
    mb->send(matnr++, TRUE);
    tb->send(texnr++, coords->get3(idx), currnormal);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m1n1t0(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * normals,
                            SoMaterialBundle * mb,
                            const SoTextureCoordinateBundle * OBOL_UNUSED_ARG(tb),
                            int32_t numpts,
                            int32_t idx)
{
  int matnr = 0;

  SoGLContext_glBegin(glue, GL_POINTS);
  for (int i = 0; i < numpts; i++) {
    mb->send(matnr++, TRUE);
    SoGLContext_glNormal3fv(glue, (const GLfloat*)normals++);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

static void
sogl_render_pointset_m1n1t1(const SoGLContext * glue,
                            const SoGLCoordinateElement * coords,
                            const SbVec3f * normals,
                            SoMaterialBundle * mb,
                            const SoTextureCoordinateBundle * tb,
                            int32_t numpts,
                            int32_t idx)
{
  int texnr = 0;
  int matnr = 0;

  SoGLContext_glBegin(glue, GL_POINTS);
  for (int i = 0; i < numpts; i++) {
    mb->send(matnr++, TRUE);
    tb->send(texnr++, coords->get3(idx), *normals);
    SoGLContext_glNormal3fv(glue, (const GLfloat*)normals++);
    coords->send(idx++);
  }
  SoGLContext_glEnd(glue);
}

// ---

typedef void sogl_render_pointset_func(const SoGLContext * glue,
                                       const SoGLCoordinateElement * coords,
                                       const SbVec3f * normals,
                                       SoMaterialBundle * mb,
                                       const SoTextureCoordinateBundle * tb,
                                       int32_t numpts,
                                       int32_t idx);

static sogl_render_pointset_func * sogl_render_pointset_funcs[8];

void
sogl_render_pointset(const SoGLContext * glue,
                     const SoGLCoordinateElement * coords,
                     const SbVec3f * normals,
                     SoMaterialBundle * mb,
                     const SoTextureCoordinateBundle * tb,
                     int32_t numpts,
                     int32_t idx)
{
  static int first = 1;
  if (first) {
    sogl_render_pointset_funcs[0] = sogl_render_pointset_m0n0t0;
    sogl_render_pointset_funcs[1] = sogl_render_pointset_m0n0t1;
    sogl_render_pointset_funcs[2] = sogl_render_pointset_m0n1t0;
    sogl_render_pointset_funcs[3] = sogl_render_pointset_m0n1t1;
    sogl_render_pointset_funcs[4] = sogl_render_pointset_m1n0t0;
    sogl_render_pointset_funcs[5] = sogl_render_pointset_m1n0t1;
    sogl_render_pointset_funcs[6] = sogl_render_pointset_m1n1t0;
    sogl_render_pointset_funcs[7] = sogl_render_pointset_m1n1t1;
    first = 0;
  }

  int mat = mb ? 1 : 0;
  int norm = normals ? 1 : 0;
  int tex = tb ? 1 : 0;

  sogl_render_pointset_funcs[ (mat << 2) | (norm << 1) | tex ]
    ( glue,
      coords,
      normals,
      mb,
      tb,
      numpts,
      idx);
}

// Used by library code to decide whether or not to add extra
// debugging checks for glGetError().
SbBool
sogl_glerror_debugging(void)
{
  static int OBOL_GLERROR_DEBUGGING = -1;
  if (OBOL_GLERROR_DEBUGGING == -1) {
    const char * str = CoinInternal::getEnvironmentVariableRaw("OBOL_GLERROR_DEBUGGING");
    OBOL_GLERROR_DEBUGGING = str ? atoi(str) : 0;
  }
  return (OBOL_GLERROR_DEBUGGING == 0) ? FALSE : TRUE;
}

static int SOGL_AUTOCACHE_REMOTE_MIN = 500000;
static int SOGL_AUTOCACHE_REMOTE_MAX = 5000000;
static int SOGL_AUTOCACHE_LOCAL_MIN = 100000;
static int SOGL_AUTOCACHE_LOCAL_MAX = 1000000;
static int SOGL_AUTOCACHE_VBO_LIMIT = 65536;

/*!
  Called by each shape during rendering. Will enable/disable auto caching
  based on the number of primitives.
*/
void
sogl_autocache_update(SoState * state, const int numprimitives, SbBool didusevbo)
{
  static SbBool didtestenv = FALSE;
  if (!didtestenv) {
    const char * env;
    env = CoinInternal::getEnvironmentVariableRaw("OBOL_AUTOCACHE_REMOTE_MIN");
    if (env) {
      SOGL_AUTOCACHE_REMOTE_MIN = atoi(env);
    }
    env = CoinInternal::getEnvironmentVariableRaw("OBOL_AUTOCACHE_REMOTE_MAX");
    if (env) {
      SOGL_AUTOCACHE_REMOTE_MAX = atoi(env);
    }
    env = CoinInternal::getEnvironmentVariableRaw("OBOL_AUTOCACHE_LOCAL_MIN");
    if (env) {
      SOGL_AUTOCACHE_LOCAL_MIN = atoi(env);
    }
    env = CoinInternal::getEnvironmentVariableRaw("OBOL_AUTOCACHE_LOCAL_MAX");
    if (env) {
      SOGL_AUTOCACHE_LOCAL_MAX = atoi(env);
    }
    env = CoinInternal::getEnvironmentVariableRaw("OBOL_AUTOCACHE_VBO_LIMIT");
    if (env) {
      SOGL_AUTOCACHE_VBO_LIMIT = atoi(env);
    }
    didtestenv = TRUE;
  }

  int minval = SOGL_AUTOCACHE_LOCAL_MIN;
  int maxval = SOGL_AUTOCACHE_LOCAL_MAX;
  if (SoGLCacheContextElement::getIsRemoteRendering(state)) {
    minval = SOGL_AUTOCACHE_REMOTE_MIN;
    maxval = SOGL_AUTOCACHE_REMOTE_MAX;
  }
  if (numprimitives <= minval) {
    SoGLCacheContextElement::shouldAutoCache(state, SoGLCacheContextElement::DO_AUTO_CACHE);
  }
  else if (numprimitives >= maxval) {
    SoGLCacheContextElement::shouldAutoCache(state, SoGLCacheContextElement::DONT_AUTO_CACHE);
  }
  SoGLCacheContextElement::incNumShapes(state);

  if (didusevbo) {
    // avoid creating caches when rendering large VBOs
    if (numprimitives > SOGL_AUTOCACHE_VBO_LIMIT) {
      SoGLCacheContextElement::shouldAutoCache(state, SoGLCacheContextElement::DONT_AUTO_CACHE);
    }
  }
}
