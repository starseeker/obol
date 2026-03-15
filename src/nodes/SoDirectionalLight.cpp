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
  \class SoDirectionalLight SoDirectionalLight.h Inventor/nodes/SoDirectionalLight.h
  \brief The SoDirectionalLight class is a node type for specifying directional light sources.

  \ingroup coin_nodes

  A directional light source provides a model of light sources which
  are at infinite distance from the geometry it illuminates, thereby
  having no set position and consisting of an infinite volume of
  parallel rays.

  This is of course a simplified model of far-away light sources, as
  "infinite distance" is impossible.

  The sun shining on objects on earth is a good example of something
  which can be modeled rather well for the most common purposes with a
  directional light source.

  See also documentation of parent class for important information
  regarding light sources in general.

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
    DirectionalLight {
        on TRUE
        intensity 1
        color 1 1 1
        direction 0 0 -1
    }
  \endcode


  A common thing to do with an SoDirectionalLight is to connect it to
  a camera, so it works in the style of a head light to that camera.
  This can easily be accomplished by linking an SoRotation::rotation
  field, influencing the light, to the SoCamera::orientation
  field. Here is a complete example iv-file demonstrating the
  technique:

  \verbatim
  #Inventor V2.1 ascii
  
  DEF mycam PerspectiveCamera { }
  
  TransformSeparator {
     SoRotation { rotation = USE mycam.orientation }
     DirectionalLight { direction 0 0 -1 }
  }
  
  Cube { }
  \endverbatim

  (The SoTransformSeparator is included to keep the effect of the
  SoRotation node within a scope where it will only influence the
  light, and not the geometry following the light in the scene graph.)
*/

// *************************************************************************

#include <Inventor/nodes/SoDirectionalLight.h>
#include "glue/glp.h"

#include "config.h"

#include <Inventor/SbColor4f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/elements/SoEnvironmentElement.h>
#include <Inventor/elements/SoGLLightIdElement.h>
#include <Inventor/elements/SoLightElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/system/gl.h>

#include "nodes/SoSubNodeP.h"
#include "rendering/SoGLModernState.h"

// *************************************************************************

/*!
  \var SoSFVec3f SoDirectionalLight::direction

  The direction of the light source. Defaults to pointing along the
  negative Z-axis.
*/

// *************************************************************************

SO_NODE_SOURCE(SoDirectionalLight);

// *************************************************************************

/*!
  Constructor.
*/
SoDirectionalLight::SoDirectionalLight(void)
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoDirectionalLight);

  SO_NODE_ADD_FIELD(direction, (0.0f, 0.0f, -1.0f));
}

/*!
  Destructor.
*/
SoDirectionalLight::~SoDirectionalLight()
{
}

/*!
  \copybrief SoBase::initClass(void)
*/
void
SoDirectionalLight::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoDirectionalLight, SO_FROM_INVENTOR_1);
}

// *************************************************************************

// Doc from superclass.
void
SoDirectionalLight::GLRender(SoGLRenderAction * action)
{
  if (!this->on.getValue()) return;

  SoState * state = action->getState();
  int idx = SoGLLightIdElement::increment(state);

  if (idx < 0) {
#if OBOL_DEBUG
    SoDebugError::postWarning("SoDirectionalLight::GLRender",
                              "Max # of OpenGL lights exceeded :(");
#endif // OBOL_DEBUG
    return;
  }


  SoLightElement::add(state, this, SoModelMatrixElement::get(state) * 
                      SoViewingMatrixElement::get(state));

  // GL3: glLightfv/glLightf (fixed-function lighting) removed.
  // Register this directional light with SoGLModernState only.

  // GL directional light is specified towards light source
  SbVec3f dir = - this->direction.getValue();
  if (dir.normalize() == 0.0f) {
#if OBOL_DEBUG
    SoDebugError::postWarning("SoDirectionalLight::GLRender",
                              "Direction is a null vector.");
#endif // OBOL_DEBUG
  }
  // directional when w = 0.0
  SbVec4f dirvec(dir[0], dir[1], dir[2], 0.0f);

  {
    const SoGLContext * glue = sogl_glue_from_state(state);
    uint32_t ctxid = SoGLContext_get_contextid(glue);
    SoGLModernState * ms = SoGLModernState::forContext(ctxid);
    if (ms) {
      if (idx == 0) ms->resetLights();

      SoGLModernState::Light ml;
      ml.position[0] = dirvec[0];
      ml.position[1] = dirvec[1];
      ml.position[2] = dirvec[2];
      ml.position[3] = 0.0f;
      ml.ambient[0] = ml.ambient[1] = ml.ambient[2] = 0.0f; ml.ambient[3] = 1.0f;
      SbColor col = this->color.getValue() * this->intensity.getValue();
      ml.diffuse[0]  = ml.specular[0] = col[0];
      ml.diffuse[1]  = ml.specular[1] = col[1];
      ml.diffuse[2]  = ml.specular[2] = col[2];
      ml.diffuse[3]  = ml.specular[3] = 1.0f;
      ml.spotDirection[0]  = 0.0f;
      ml.spotDirection[1]  = 0.0f;
      ml.spotDirection[2]  = -1.0f;
      ml.spotCutoff         = -1.0f;
      ml.spotExponent       = 0.0f;
      ml.constantAttenuation  = 1.0f;
      ml.linearAttenuation    = 0.0f;
      ml.quadraticAttenuation = 0.0f;
      ms->addLight(ml);
    }
  }
}

// *************************************************************************
