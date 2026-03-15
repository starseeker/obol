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
  \class SoGLEnvironmentElement Inventor/elements/SoGLEnvironmentElement.h
  \brief The SoGLEnvironmentElement class is for setting GL fog etc.

  \ingroup coin_elements
*/


#include <Inventor/elements/SoGLEnvironmentElement.h>
#include "glue/glp.h"
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/SbColor4f.h>
#include "config.h"


#include <Inventor/system/gl.h>

#include <cassert>

SO_ELEMENT_SOURCE(SoGLEnvironmentElement);

/*!
  \copydetails SoElement::initClass(void)
*/
void
SoGLEnvironmentElement::initClass()
{
  SO_ELEMENT_INIT_CLASS(SoGLEnvironmentElement, inherited);
}

/*!
  Destructor.
*/
SoGLEnvironmentElement::~SoGLEnvironmentElement()
{
}

// doc in superclass
void
SoGLEnvironmentElement::init(SoState * state)
{
  inherited::init(state);
}

// doc in superclass
void
SoGLEnvironmentElement::pop(SoState * OBOL_UNUSED_ARG(state),
                           const SoElement * OBOL_UNUSED_ARG(prevTopElement))
{
  this->capture(state);
  this->updategl(state);
}

// doc in superclass
void
SoGLEnvironmentElement::setElt(SoState * const stateptr,
                               const float ambientIntensityarg,
                               const SbColor & ambientColorarg,
                               const SbVec3f & attenuationarg,
                               const int32_t fogTypearg,
                               const SbColor & fogColorarg,
                               const float fogVisibilityarg,
                               const float fogStartarg)
{
  inherited::setElt(stateptr, ambientIntensityarg, ambientColorarg,
                    attenuationarg, fogTypearg, fogColorarg, fogVisibilityarg,
                    fogStartarg);
  this->updategl(stateptr);
}


//! FIXME: write doc.

void
SoGLEnvironmentElement::updategl(SoState * const state)
{
  const SoGLContext * glue = sogl_glue_from_state(state);
  float ambient[4];
  ambient[0] = ambientColor[0] * ambientIntensity;
  ambient[1] = ambientColor[1] * ambientIntensity;
  ambient[2] = ambientColor[2] * ambientIntensity;
  ambient[3] = 1.0f;

  // GL3: glLightModelfv (GL_LIGHT_MODEL_AMBIENT) removed from core.
  // Ambient light is handled via SoGLModernState's material system.
  (void)ambient;

  if (fogType == (int)NONE) {
    // GL3: glDisable(GL_FOG) removed from core.
    return;
  }

  // GL3: glFog* (fixed-function fog) removed from core.
  // Fog effects now require shader-based implementation.
}
