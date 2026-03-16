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

#include "SoGLSLShaderParameter.h"
#include "glue/glp.h"
#include "SoGLSLShaderObject.h"

#include <Inventor/errors/SoDebugError.h>
#include <cstdio>

// *************************************************************************

SoGLSLShaderParameter::SoGLSLShaderParameter(void)
{
  this->location  = -1;
  this->cacheType = GL_FLOAT;
  this->cacheName = "";
  this->cacheSize =  0;
  this->isActive = TRUE;
  this->programid = 0;
}

SoGLSLShaderParameter::~SoGLSLShaderParameter()
{
}

SoShader::Type
SoGLSLShaderParameter::shaderType(void) const
{
  return SoShader::GLSL_SHADER;
}

void
SoGLSLShaderParameter::set1f(const SoGLShaderObject * shader,
                             const float value, const char *name, const int)
{
  if (this->isValid(shader, name, GL_FLOAT))
    glUniform1f(this->location, value);
}

void
SoGLSLShaderParameter::set2f(const SoGLShaderObject * shader,
                             const float * value, const char *name, const int)
{
  if (this->isValid(shader, name, GL_FLOAT_VEC2_ARB))
    glUniform2f(this->location, value[0], value[1]);
}

void
SoGLSLShaderParameter::set3f(const SoGLShaderObject * shader,
                             const float * v, const char *name, const int)
{
  if (this->isValid(shader, name, GL_FLOAT_VEC3_ARB))
    glUniform3f(this->location, v[0], v[1], v[2]);
}

void
SoGLSLShaderParameter::set4f(const SoGLShaderObject * shader,
                             const float * v, const char *name, const int)
{
  if (this->isValid(shader, name, GL_FLOAT_VEC4_ARB))
    glUniform4f(this->location, v[0], v[1], v[2], v[3]);
}


void
SoGLSLShaderParameter::set1fv(const SoGLShaderObject * shader, const int num,
                              const float *value, const char * name, const int)
{
  int cnt = num;
  if (this->isValid(shader, name, GL_FLOAT, &cnt))
    glUniform1fv(this->location, cnt, value);
}

void
SoGLSLShaderParameter::set2fv(const SoGLShaderObject * shader, const int num,
                              const float* value, const char* name, const int)
{
  int cnt = num;
  if (this->isValid(shader, name, GL_FLOAT_VEC2_ARB, &cnt))
    glUniform2fv(this->location, cnt, value);
}

void
SoGLSLShaderParameter::set3fv(const SoGLShaderObject * shader, const int num,
                              const float* value, const char * name, const int)
{
  int cnt = num;
  if (this->isValid(shader, name, GL_FLOAT_VEC3_ARB, &cnt))
    glUniform3fv(this->location, cnt, value);
}

void
SoGLSLShaderParameter::set4fv(const SoGLShaderObject * shader, const int num,
                              const float* value, const char * name, const int)
{
  int cnt = num;
  if (this->isValid(shader, name, GL_FLOAT_VEC4_ARB, &cnt))
    glUniform4fv(this->location, cnt, value);
}

void
SoGLSLShaderParameter::setMatrix(const SoGLShaderObject *shader,
                                 const float * value, const char * name,
                                 const int)
{
  if (this->isValid(shader, name, GL_FLOAT_MAT4_ARB))
    glUniformMatrix4fv(this->location, 1, FALSE, value);
}


void
SoGLSLShaderParameter::setMatrixArray(const SoGLShaderObject *shader,
                                      const int num, const float *value,
                                      const char *name, const int)
{
  int cnt = num;
  if (this->isValid(shader, name, GL_FLOAT_MAT4_ARB, &cnt))
    glUniformMatrix4fv(this->location, cnt, FALSE, value);
}


void
SoGLSLShaderParameter::set1i(const SoGLShaderObject * shader,
                             const int32_t value, const char * name, const int)
{
  if (this->isValid(shader, name, GL_INT))
    glUniform1i(this->location, value);
}

void
SoGLSLShaderParameter::set2i(const SoGLShaderObject * shader,
                             const int32_t * value, const char * name,
                             const int)
{
  if (this->isValid(shader, name, GL_INT_VEC2_ARB))
    glUniform2i(this->location, value[0], value[1]);
}

void
SoGLSLShaderParameter::set3i(const SoGLShaderObject * shader,
                             const int32_t * v, const char * name,
                             const int)
{
  if (this->isValid(shader, name, GL_INT_VEC3_ARB))
    glUniform3i(this->location, v[0], v[1], v[2]);
}

void
SoGLSLShaderParameter::set4i(const SoGLShaderObject * shader,
                             const int32_t * v, const char * name,
                             const int)
{
  if (this->isValid(shader, name, GL_INT_VEC4_ARB))
    glUniform4i(this->location, v[0], v[1], v[2], v[3]);
}

void
SoGLSLShaderParameter::set1iv(const SoGLShaderObject * shader,
                              const int num,
                              const int32_t * value, const char * name,
                              const int)
{
  if (this->isValid(shader, name, GL_INT))
    glUniform1iv(this->location, num, (const GLint*) value);
}

void
SoGLSLShaderParameter::set2iv(const SoGLShaderObject * shader,
                              const int num,
                              const int32_t * value, const char * name,
                              const int)
{
  if (this->isValid(shader, name, GL_INT_VEC2_ARB))
    glUniform2iv(this->location, num, (const GLint*)value);
}

void
SoGLSLShaderParameter::set3iv(const SoGLShaderObject * shader,
                              const int num,
                              const int32_t * v, const char * name,
                              const int)
{
  if (this->isValid(shader, name, GL_INT_VEC3_ARB))
    glUniform3iv(this->location, num, (const GLint*)v);
}

void
SoGLSLShaderParameter::set4iv(const SoGLShaderObject * shader,
                              const int num,
                              const int32_t * v, const char * name,
                              const int)
{
  if (this->isValid(shader, name, GL_INT_VEC4_ARB))
    glUniform4iv(this->location, num, (const GLint*)v);
}

SbBool
SoGLSLShaderParameter::isEqual(GLenum type1, GLenum type2)
{
  if (type1 == type2)
    return TRUE;

  if (type2 == GL_INT) {
    switch (type1) {
    case GL_INT:
    case GL_SAMPLER_1D_ARB:
    case GL_SAMPLER_2D_ARB:
    case GL_SAMPLER_3D_ARB:
    case GL_SAMPLER_CUBE_ARB:
    case GL_SAMPLER_1D_SHADOW_ARB:
    case GL_SAMPLER_2D_SHADOW_ARB:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:
      return TRUE;
    default:
      return FALSE;
    }
  }
  return FALSE;
}

SbBool
SoGLSLShaderParameter::isValid(const SoGLShaderObject * shader,
                               const char * name, GLenum type,
                               int * num)
{
  assert(shader);
  assert(shader->shaderType() == SoShader::GLSL_SHADER);

  OBOL_GLhandle pHandle = const_cast<SoGLSLShaderObject*>(static_cast<const SoGLSLShaderObject*>(shader))->programHandle;
  int32_t pId = const_cast<SoGLSLShaderObject*>(static_cast<const SoGLSLShaderObject*>(shader))->programid;

  // return TRUE if uniform isn't active. We warned the user about
  // this when we found it to be inactive.
  if ((pId == this->programid) && (this->location > -1) && !this->isActive) return TRUE;

  if ((pId == this->programid) && (this->location > -1) &&
      (this->cacheName == name) && this->isEqual(this->cacheType, type)) {
    if (num) { // assume: ARRAY
      if (this->cacheSize < *num) {
        // FIXME: better error handling - 20050128 martin
        SoDebugError::postWarning("SoGLSLShaderParameter::isValid",
                                  "parameter %s[%d] < input[%d]!",
                                  this->cacheName.getString(),
                                  this->cacheSize, *num);
        *num = this->cacheSize;
      }
      return (*num > 0);
    }
    return TRUE;
  }

  (void) shader->GLContext(); // context no longer needed; all calls are now direct GL3

  this->cacheSize = 0;
  this->location = glGetUniformLocation(pHandle, (const GLchar *)name);
  this->programid = pId;

  if (this->location == -1)  {
#if OBOL_DEBUG
    SoDebugError::postWarning("SoGLSLShaderParameter::isValid",
                              "parameter '%s' not found in program.",
                              name);
#endif // OBOL_DEBUG
    return FALSE;
  }

  this->cacheName = name;
  // glGetUniformLocation succeeded; treat this uniform as active.
  // Skip glGetActiveUniform introspection — not available in all GL3
  // environments (e.g. PortableGL's C-function-pointer shader model).
  this->cacheSize = (num ? (*num > 0 ? *num : 1) : 1);
  this->cacheType = type;
  this->isActive = TRUE;
  if (!this->isActive) {
    // not critical, but warn user so they can remove the unused parameter
#if OBOL_DEBUG
    SoDebugError::postWarning("SoGLSLShaderParameter::isValid",
                              "parameter '%s' not active.",
                              this->cacheName.getString());
#endif // OBOL_DEBUG
    // return here since cacheSize and cacheType will not be properly initialized
    return TRUE;
  }

  if (!this->isEqual(this->cacheType, type)) {
    SoDebugError::postWarning("SoGLSLShaderParameter::isValid",
                              "parameter %s [%d] is "
                              "of wrong type [%d]!",
                              this->cacheName.getString(),
                              this->cacheType, type);
    this->cacheType = GL_FLOAT;
    return FALSE;
  }

  if (num) { // assume: ARRAY
    if (this->cacheSize < *num) {
      // FIXME: better error handling - 20050128 martin
      SoDebugError::postWarning("SoGLSLShaderParameter::isValid",
                                "parameter %s[%d] < input[%d]!",
                                this->cacheName.getString(),
                                this->cacheSize, *num);
      *num = this->cacheSize;
    }
    return (*num > 0);
  }
  return TRUE;
}
