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

#include "CadGLCaps.h"
#include "glue/glp.h"

namespace obol {
namespace internal {

CadGLCaps CadGLCaps::detect(const SoGLContext * glue)
{
    CadGLCaps caps;
    if (!glue) return caps;

    caps.hasVBO = (glue->glGenBuffers       != nullptr &&
                   glue->glBindBuffer       != nullptr &&
                   glue->glBufferData       != nullptr &&
                   glue->glDeleteBuffers    != nullptr &&
                   glue->glDrawElements     != nullptr &&
                   glue->glVertexAttribPointerARB        != nullptr &&
                   glue->glEnableVertexAttribArrayARB    != nullptr &&
                   glue->glDisableVertexAttribArrayARB   != nullptr);

    caps.hasShaderObjects = (glue->glCreateShaderObjectARB  != nullptr &&
                             glue->glShaderSourceARB         != nullptr &&
                             glue->glCompileShaderARB        != nullptr &&
                             glue->glCreateProgramObjectARB  != nullptr &&
                             glue->glAttachObjectARB         != nullptr &&
                             glue->glLinkProgramARB          != nullptr &&
                             glue->glUseProgramObjectARB     != nullptr &&
                             glue->glGetUniformLocationARB   != nullptr &&
                             glue->glUniformMatrix4fvARB     != nullptr &&
                             glue->glUniform4fvARB           != nullptr &&
                             glue->glUniform3fvARB           != nullptr &&
                             glue->glUniform1iARB            != nullptr &&
                             glue->glGetAttribLocationARB    != nullptr);

    caps.hasVAO = (glue->glGenVertexArrays    != nullptr &&
                   glue->glBindVertexArray    != nullptr &&
                   glue->glDeleteVertexArrays != nullptr);

    caps.hasInstancing = (glue->glDrawElementsInstanced != nullptr);

    caps.hasAttribDivisor = (glue->glVertexAttribDivisor != nullptr);

    return caps;
}

} // namespace internal
} // namespace obol
