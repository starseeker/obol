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

/*!
  \class SoIndexedMarkerSet SoIndexedMarkerSet.h Inventor/nodes/SoIndexedMarkerSet.h
  \brief The SoIndexedMarkerSet class is used to display a set of bitmap markers at 3D positions.

  \ingroup coin_nodes

  This node either uses the coordinates currently on the state
  (typically set up by a leading SoCoordinate3 node in the scene graph)
  or from a SoVertexProperty node attached to this node to render a
  set of 3D points.

  To add new markers, use the static functions in SoMarkerSet.

  Here's a simple usage example of SoIndexedMarkerSet in a scene graph:

  \verbatim
  #Inventor V2.1 ascii

  Separator {
     Material {
        diffuseColor [
         1 0 0, 0 1 0, 0 0 1, 1 1 0, 1 0 1, 1 1 1, 1 0.8 0.6, 0.6 0.8 1
        ]
     }
     MaterialBinding { value PER_VERTEX_INDEXED }

     Coordinate3 {
        point [
         -1 1 0, -1 -1 0, 1 -1 0, 1 1 0, 0 2 -1, -2 0 -1, 0 -2 -1, 2 0 -1
        ]
     }

     IndexedMarkerSet {
        coordIndex [0, 1, 2, 3, 4, 5, 6, 7]
        markerIndex [0, 1, 0, 1, 0, 1, 0, 1]
     }
  }

  \endverbatim

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
  IndexedMarkerSet {
    vertexProperty        NULL
    coordIndex        0
    materialIndex        -1
    normalIndex        -1
    textureCoordIndex        -1
    markerIndex   -1
  }
  \endcode

  \since TGS Inventor 6.0, Coin 3.1
*/

#include <Inventor/nodes/SoIndexedMarkerSet.h>

#include "config.h"

#include <Inventor/misc/SoState.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/system/gl.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoMarkerSet.h>
#include <Inventor/elements/SoGLCoordinateElement.h>
#include <Inventor/elements/SoMaterialBindingElement.h>
#include <Inventor/elements/SoMultiTextureEnabledElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoGLVBOElement.h>
#include <Inventor/elements/SoCullElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>

#include <Inventor/bundles/SoMaterialBundle.h>
#include <Inventor/SbViewVolume.h>

#if OBOL_DEBUG
#include <Inventor/errors/SoDebugError.h>
#endif // OBOL_DEBUG

#include <Inventor/elements/SoGLViewingMatrixElement.h>
#include "rendering/SoGLModernState.h"
#include <vector>
#include "nodes/SoSubNodeP.h"
#include "rendering/SoGL.h"

SO_NODE_SOURCE(SoIndexedMarkerSet);

/*!
  Constructor.
*/
SoIndexedMarkerSet::SoIndexedMarkerSet() : SoIndexedPointSet()
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoIndexedMarkerSet);
  SO_NODE_ADD_FIELD(markerIndex, (0));
}

/*!
  Destructor.
*/
SoIndexedMarkerSet::~SoIndexedMarkerSet()
{
}

/*!
  \copydetails SoNode::initClass(void)
*/
void
SoIndexedMarkerSet::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoIndexedMarkerSet, SO_FROM_INVENTOR_6_0);
}

// doc from parent
void
SoIndexedMarkerSet::GLRender(SoGLRenderAction * action)
{
  int32_t numpts = this->coordIndex.getNum();
  if (numpts == 0) return;

  SoState * state = action->getState();
  state->push();

  if (this->vertexProperty.getValue()) {
    this->vertexProperty.getValue()->GLRender(action);
  }

  if (!this->shouldGLRender(action)){
    state->pop();
    return;
  }

  SoGLCacheContextElement::shouldAutoCache(state, SoGLCacheContextElement::DONT_AUTO_CACHE);

  // We just disable lighting and texturing for markers, since we
  // can't see any reason this should ever be enabled.  send an angry
  // email to <pederb@coin3d.org> if you disagree.
  
  SoLazyElement::setLightModel(state, SoLazyElement::BASE_COLOR);
  SoMultiTextureEnabledElement::disableAll(state);

  SoMaterialBundle mb(action);

  const SbVec3f * normals;
  int numindices;
  const int32_t * cindices;
  const int32_t * nindices;
  const int32_t * tindices;
  const int32_t * mindices;
  const SoCoordinateElement * coords;
  Binding mbind;

  SbBool normalCacheUsed;

  this->getVertexData(state, coords, normals, cindices,
                      nindices, tindices, mindices, numindices,
                      false, normalCacheUsed);

  if (numindices == 0){//nothing to render
    state->pop();
    return;
  }

  mbind = this->findMaterialBinding(state);
  //if we don't have explicit material indices, use coord indices:
  if (mbind == PER_VERTEX_INDEXED && mindices == NULL) mindices = cindices;

  const SoGLCoordinateElement * glcoords = dynamic_cast<const SoGLCoordinateElement *>(coords);
  assert(glcoords && "could not cast to SoGLCoordinateElement");

  mb.sendFirst(); // always do this, even if mbind != OVERALL

  const SbMatrix & mat = SoModelMatrixElement::get(state);
  //const SbViewVolume & vv = SoViewVolumeElement::get(state);
  const SbViewportRegion & vp = SoViewportRegionElement::get(state);
  const SbMatrix & projmatrix = (mat * SoViewingMatrixElement::get(state) *
                                 SoProjectionMatrixElement::get(state));
  SbVec2s vpsize = vp.getViewportSizePixels();

  // GL3: clip-plane disable/re-enable loop removed (GL_MAX_CLIP_PLANES / GL_CLIP_PLANE0
  // not in GL 3 core).  SoCullElement::cullTest() in the render loop provides
  // per-marker frustum culling.

  // GL3: set up screen-space ortho matrices via SoGLModernState.
  SoGLModernState * ms_mk = SoGLModernState::forContext(action->getCacheContext());
  if (ms_mk && ms_mk->isAvailable()) {
    static const float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    ms_mk->setModelViewMatrix(ident);
    float W = static_cast<float>(vpsize[0]), H = static_cast<float>(vpsize[1]);
    const float ortho[16] = {
      2.0f/W, 0,      0,  -1.0f,
      0,      2.0f/H, 0,  -1.0f,
      0,      0,     -1.0f, 0,
      0,      0,      0,   1.0f
    };
    ms_mk->setProjectionMatrix(ortho);
    static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    ms_mk->setMaterial(white, white, white, white, 0.0f);
    ms_mk->activateBaseColor(false, true, true);
  }

  for (int i = 0; i < numindices; i++) {
    int32_t idx = cindices[i];

    int midx = i;
#if OBOL_DEBUG
      if (midx < 0 || midx > this->markerIndex.getNum() - 1) {
        static SbBool firsterror = TRUE;
        if (firsterror) {
          SoDebugError::postWarning("SoMarkerSet::GLRender",
                                    "no markerIndex for coordinate %d",
                                    i);
          firsterror = FALSE;
        }
        midx = this->markerIndex.getNum() - 1;
      }
#endif // OBOL_DEBUG

    int marker = this->markerIndex[midx];
    if (marker == SoMarkerSet::NONE) { continue; }

    SbVec2s size;
    const unsigned char * bytes;
    SbBool isLSBFirst;

    if (marker >= SoMarkerSet::getNumDefinedMarkers()) continue;

    SbBool validMarker = SoMarkerSet::getMarker(marker, size, bytes, isLSBFirst);
    if (!validMarker) continue;

    if (mbind == PER_VERTEX_INDEXED) mb.send(mindices[i], TRUE);
    else if (mbind == PER_VERTEX) mb.send(i, TRUE);

    SbVec3f point = glcoords->get3(idx);

    const SbBox3f bbox(point, point);
    if (SoCullElement::cullTest(state, bbox, TRUE)) { continue; }

    projmatrix.multVecMatrix(point, point);
    point[0] = (point[0] + 1.0f) * 0.5f * vpsize[0];
    point[1] = (point[1] + 1.0f) * 0.5f * vpsize[1];

    point[0] = point[0] - (size[0] - 1) / 2;
    point[1] = point[1] - (size[1] - 1) / 2;

    // GL3: convert 1-bit packed bitmap → RGBA texture, then render as quad.
    int mw = size[0], mh = size[1];
    int mal = (marker >= SoMarkerSet::NUM_MARKERS) ? 1 : 4;
    std::vector<unsigned char> rgba(static_cast<size_t>(mw) * mh * 4, 0);
    for (int my = 0; my < mh; ++my) {
      for (int mx = 0; mx < mw; ++mx) {
        int byterow = my * mal;
        int bitidx  = mx / 8;
        int bitpos  = 7 - (mx % 8);
        if (bytes && ((bytes[byterow + bitidx] >> bitpos) & 1)) {
          size_t pi = (static_cast<size_t>(my) * mw + mx) * 4;
          rgba[pi+0] = rgba[pi+1] = rgba[pi+2] = rgba[pi+3] = 255;
        }
      }
    }

    GLuint mk_texid = 0;
    SoGLContext_glGenTextures(sogl_glue_from_state(state), 1, &mk_texid);
    if (mk_texid) {
      SoGLContext_glActiveTexture(sogl_glue_from_state(state), GL_TEXTURE0);
      SoGLContext_glBindTexture(sogl_glue_from_state(state), GL_TEXTURE_2D, mk_texid);
      SoGLContext_glTexParameteri(sogl_glue_from_state(state), GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      SoGLContext_glTexParameteri(sogl_glue_from_state(state), GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      SoGLContext_glTexParameteri(sogl_glue_from_state(state), GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      SoGLContext_glTexParameteri(sogl_glue_from_state(state), GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      SoGLContext_glPixelStorei(sogl_glue_from_state(state), GL_UNPACK_ALIGNMENT, 1);
      SoGLContext_glTexImage2D(sogl_glue_from_state(state), GL_TEXTURE_2D, 0, GL_RGBA,
                               mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

      float qx  = point[0], qy  = point[1], qz = -point[2];
      float qx1 = qx + mw,  qy1 = qy + mh;
      SoGLContext_glEnable(sogl_glue_from_state(state), GL_TEXTURE_2D);
      SoGLContext_glBegin(sogl_glue_from_state(state), GL_TRIANGLES);
      SoGLContext_glTexCoord2f(sogl_glue_from_state(state), 0.0f, 0.0f); SoGLContext_glVertex3f(sogl_glue_from_state(state), qx,  qy,  qz);
      SoGLContext_glTexCoord2f(sogl_glue_from_state(state), 1.0f, 0.0f); SoGLContext_glVertex3f(sogl_glue_from_state(state), qx1, qy,  qz);
      SoGLContext_glTexCoord2f(sogl_glue_from_state(state), 1.0f, 1.0f); SoGLContext_glVertex3f(sogl_glue_from_state(state), qx1, qy1, qz);
      SoGLContext_glTexCoord2f(sogl_glue_from_state(state), 0.0f, 0.0f); SoGLContext_glVertex3f(sogl_glue_from_state(state), qx,  qy,  qz);
      SoGLContext_glTexCoord2f(sogl_glue_from_state(state), 1.0f, 1.0f); SoGLContext_glVertex3f(sogl_glue_from_state(state), qx1, qy1, qz);
      SoGLContext_glTexCoord2f(sogl_glue_from_state(state), 0.0f, 1.0f); SoGLContext_glVertex3f(sogl_glue_from_state(state), qx,  qy1, qz);
      SoGLContext_glEnd(sogl_glue_from_state(state));
      SoGLContext_glDisable(sogl_glue_from_state(state), GL_TEXTURE_2D);
      SoGLContext_glBindTexture(sogl_glue_from_state(state), GL_TEXTURE_2D, 0);
      SoGLContext_glDeleteTextures(sogl_glue_from_state(state), 1, &mk_texid);
    }
  }

  SoGLContext_glPixelStorei(sogl_glue_from_state(state), GL_UNPACK_ALIGNMENT, 4);

  // Restore SoGLModernState matrices from Coin state.
  if (ms_mk && ms_mk->isAvailable()) {
    SbMatrix mv = SoGLViewingMatrixElement::getResetMatrix(state);
    mv.multLeft(SoModelMatrixElement::get(state));
    ms_mk->setModelViewMatrix((const float*)mv.getValue());
    const SbMatrix &proj = SoProjectionMatrixElement::get(state);
    ms_mk->setProjectionMatrix((const float*)proj.getValue());
  }

  state->pop();

  sogl_autocache_update(state, numindices/3, FALSE);
}
