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

#include "CadRendererGL.h"

#include <obol/cad/SoCADAssembly.h>

#include <Inventor/system/gl.h>
#include <Inventor/SbVec3f.h>
#include "glue/glp.h"

#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
// GLSL shader sources – Tier 1 (GL 2.0 / GLSL 1.10, no #version directive)
// ---------------------------------------------------------------------------

// Wire pass: no lighting, colour from uniform
static const char * kWireVS1 =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_model;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform vec4 u_color;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * u_model * vec4(a_pos, 1.0);\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kWireFS1 =
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = v_color;\n"
    "}\n";

// Shaded pass: simple directional light in world space
static const char * kShadedVS1 =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_norm;\n"
    "uniform mat4  u_model;\n"
    "uniform mat4  u_viewProj;\n"
    "uniform vec4  u_color;\n"
    "uniform int   u_hasNorm;\n"
    "varying vec3  v_norm;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * u_model * vec4(a_pos, 1.0);\n"
    "    if (u_hasNorm != 0) {\n"
    "        v_norm = mat3(u_model[0].xyz, u_model[1].xyz, u_model[2].xyz) * a_norm;\n"
    "    } else {\n"
    "        v_norm = vec3(0.0, 0.0, 1.0);\n"
    "    }\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kShadedFS1 =
    "uniform vec3 u_lightDir;\n"
    "varying vec3 v_norm;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_norm);\n"
    "    float nDotL = max(0.0, dot(n, u_lightDir));\n"
    "    vec3 ambient = v_color.rgb * 0.25;\n"
    "    vec3 diffuse = v_color.rgb * nDotL * 0.75;\n"
    "    gl_FragColor = vec4(ambient + diffuse, v_color.a);\n"
    "}\n";

// ---------------------------------------------------------------------------
// GLSL shader sources – Tier 2 (GL 3.1+ / GLSL 1.40, instanced)
// Per-instance transform and color are passed as vertex attributes with
// divisor=1.  The mat4 occupies 4 consecutive attribute locations.
// ---------------------------------------------------------------------------

static const char * kWireVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in mat4  a_instTransform;\n"  // locations: BASE_INST_LOC .. BASE_INST_LOC+3
    "in vec4  a_instColor;\n"      // location:  BASE_INST_LOC+4
    "uniform mat4 u_viewProj;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * a_instTransform * vec4(a_pos, 1.0);\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kWireFS2 =
    "#version 140\n"
    "in  vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() { fragColor = v_color; }\n";

static const char * kShadedVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in vec3  a_norm;\n"
    "in mat4  a_instTransform;\n"
    "in vec4  a_instColor;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform int  u_hasNorm;\n"
    "out vec3 v_norm;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * a_instTransform * vec4(a_pos, 1.0);\n"
    "    if (u_hasNorm != 0) {\n"
    "        mat3 nm = mat3(a_instTransform[0].xyz,\n"
    "                       a_instTransform[1].xyz,\n"
    "                       a_instTransform[2].xyz);\n"
    "        v_norm = nm * a_norm;\n"
    "    } else {\n"
    "        v_norm = vec3(0.0, 0.0, 1.0);\n"
    "    }\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kShadedFS2 =
    "#version 140\n"
    "uniform vec3 u_lightDir;\n"
    "in  vec3 v_norm;\n"
    "in  vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_norm);\n"
    "    float nDotL = max(0.0, dot(n, u_lightDir));\n"
    "    vec3 ambient = v_color.rgb * 0.25;\n"
    "    vec3 diffuse = v_color.rgb * nDotL * 0.75;\n"
    "    fragColor = vec4(ambient + diffuse, v_color.a);\n"
    "}\n";

// Attribute locations for instanced vertex attributes
// Attribute 0: a_pos (vec3)
// Attribute 1: a_norm (vec3)  -- shaded only
// Attributes 2..5: a_instTransform (mat4 = 4 × vec4 columns)
// Attribute 6: a_instColor (vec4)
static const GLuint kInstTransformLoc = 2;
static const GLuint kInstColorLoc     = 6;

// Fixed light direction (world space, normalised)
static const float kLightDir[3] = { 0.577f, 0.577f, 0.577f };

namespace obol {
namespace internal {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

CadRendererGL::CadRendererGL()  = default;
CadRendererGL::~CadRendererGL() = default;

// ---------------------------------------------------------------------------
// Shader compilation
// ---------------------------------------------------------------------------

GLuint CadRendererGL::compileShader(const SoGLContext* glue, GLenum type,
                                    const char* src)
{
    GLhandleARB h = glue->glCreateShaderObjectARB(type);
    if (!h) return 0;

    glue->glShaderSourceARB(h, 1, (const OBOL_GLchar**)&src, nullptr);
    glue->glCompileShaderARB(h);

    GLint ok = 0;
    glue->glGetObjectParameterivARB(h, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
    if (!ok) {
        GLint len = 0;
        glue->glGetObjectParameterivARB(h, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
        if (len > 1) {
            std::vector<char> log(static_cast<size_t>(len));
            glue->glGetInfoLogARB(h, len, nullptr,
                                  reinterpret_cast<OBOL_GLchar*>(log.data()));
            std::fprintf(stderr, "CadRendererGL: shader compile error:\n%s\n",
                         log.data());
        }
        glue->glDeleteObjectARB(h);
        return 0;
    }
    return static_cast<GLuint>(h);
}

GLuint CadRendererGL::linkProgram(const SoGLContext* glue, GLuint vs, GLuint fs)
{
    GLhandleARB prog = glue->glCreateProgramObjectARB();
    if (!prog) return 0;

    glue->glAttachObjectARB(prog, vs);
    glue->glAttachObjectARB(prog, fs);

    // Bind attribute locations before linking so they are always predictable,
    // regardless of what the GLSL compiler assigns implicitly.
    if (glue->glBindAttribLocationARB) {
        glue->glBindAttribLocationARB(prog, 0,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_pos")));
        glue->glBindAttribLocationARB(prog, 1,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_norm")));
    }

    glue->glLinkProgramARB(prog);

    GLint ok = 0;
    glue->glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &ok);
    if (!ok) {
        GLint len = 0;
        glue->glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
        if (len > 1) {
            std::vector<char> log(static_cast<size_t>(len));
            glue->glGetInfoLogARB(prog, len, nullptr,
                                  reinterpret_cast<OBOL_GLchar*>(log.data()));
            std::fprintf(stderr, "CadRendererGL: shader link error:\n%s\n",
                         log.data());
        }
        glue->glDeleteObjectARB(prog);
        return 0;
    }

    // Detach shaders – they are no longer needed after linking
    glue->glDetachObjectARB(prog, vs);
    glue->glDetachObjectARB(prog, fs);

    return static_cast<GLuint>(prog);
}

bool CadRendererGL::compileAllShaders(const SoGLContext* glue)
{
    // --- Tier-1 shaders ---
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kWireVS1);
        GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS1);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.wire = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.wire) return false;
    }
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedVS1);
        GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kShadedFS1);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.shaded = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.shaded) return false;
    }

    // --- Tier-2 shaders (only when instancing is available) ---
    if (caps_.canUseInstanced()) {
        {
            GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kWireVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS2);
            if (vs && fs) {
                shaders_.wireInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        {
            GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kShadedFS2);
            if (vs && fs) {
                shaders_.shadedInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        // If instanced shaders failed, fall back to Tier-1
        if (!shaders_.wireInst || !shaders_.shadedInst) {
            if (shaders_.wireInst)   glue->glDeleteObjectARB(shaders_.wireInst);
            if (shaders_.shadedInst) glue->glDeleteObjectARB(shaders_.shadedInst);
            shaders_.wireInst   = 0;
            shaders_.shadedInst = 0;
        }
    }

    return true;
}

void CadRendererGL::deleteShaders(const SoGLContext* glue)
{
    if (!glue) return;
    if (shaders_.wire)      { glue->glDeleteObjectARB(shaders_.wire);      shaders_.wire      = 0; }
    if (shaders_.shaded)    { glue->glDeleteObjectARB(shaders_.shaded);    shaders_.shaded    = 0; }
    if (shaders_.wireInst)  { glue->glDeleteObjectARB(shaders_.wireInst);  shaders_.wireInst  = 0; }
    if (shaders_.shadedInst){ glue->glDeleteObjectARB(shaders_.shadedInst);shaders_.shadedInst= 0; }
}

// ---------------------------------------------------------------------------
// ensureReady()
// ---------------------------------------------------------------------------

bool CadRendererGL::ensureReady(const SoGLContext* glue)
{
    if (!glue) {
        std::fprintf(stderr, "CadRendererGL: glue is null\n");
        return false;
    }

    if (!capsDetected_) {
        caps_ = CadGLCaps::detect(glue);
        capsDetected_ = true;
        std::fprintf(stderr,
            "CadRendererGL: caps detected: hasVBO=%d hasShaders=%d hasVAO=%d\n",
            (int)caps_.hasVBO, (int)caps_.hasShaderObjects, (int)caps_.hasVAO);
    }
    if (!caps_.canUseVbo()) {
        std::fprintf(stderr, "CadRendererGL: canUseVbo() false – cannot render\n");
        return false;
    }

    // (Re-)compile shaders if context changed
    if (shaders_.wire == 0 || shadersContextId_ != glue->contextid) {
        deleteShaders(glue);
        shadersContextId_ = glue->contextid;
        if (!compileAllShaders(glue)) {
            std::fprintf(stderr, "CadRendererGL: shader compilation failed\n");
            return false;
        }
        std::fprintf(stderr,
            "CadRendererGL: shaders compiled OK (wire=%u shaded=%u wireInst=%u shadedInst=%u)\n",
            shaders_.wire, shaders_.shaded, shaders_.wireInst, shaders_.shadedInst);
    }

    return true;
}

// ---------------------------------------------------------------------------
// ensurePartUploaded()
// ---------------------------------------------------------------------------

void CadRendererGL::ensurePartUploaded(PartId pid, const SoCADAssembly& assembly,
                                       uint64_t gen, const SoGLContext* glue)
{
    // Retrieve part geometry from the assembly
    const obol::PartGeometry* geom = assembly.partGeometry(pid);
    if (!geom) return;

    // Build CPU-side flat arrays for wire geometry
    std::vector<float>    wirePos;
    std::vector<uint32_t> wireSegIdx;
    if (geom->wire.has_value()) {
        const auto& wr = *geom->wire;
        for (const auto& poly : wr.polylines) {
            if (poly.points.size() < 2) continue;
            const uint32_t base = static_cast<uint32_t>(wirePos.size() / 3);
            for (const auto& pt : poly.points) {
                wirePos.push_back(pt[0]);
                wirePos.push_back(pt[1]);
                wirePos.push_back(pt[2]);
            }
            // Emit a segment (pair of indices) for each consecutive pair
            for (uint32_t i = 0; i + 1 < poly.points.size(); ++i) {
                wireSegIdx.push_back(base + i);
                wireSegIdx.push_back(base + i + 1);
            }
        }
    }

    // Flatten triangle geometry
    std::vector<float>    triPos;
    std::vector<float>    triNorm;
    std::vector<uint32_t> triIdx;
    if (geom->shaded.has_value()) {
        const auto& mesh = *geom->shaded;
        for (const auto& p : mesh.positions) {
            triPos.push_back(p[0]);
            triPos.push_back(p[1]);
            triPos.push_back(p[2]);
        }
        if (!mesh.normals.empty()) {
            for (const auto& n : mesh.normals) {
                triNorm.push_back(n[0]);
                triNorm.push_back(n[1]);
                triNorm.push_back(n[2]);
            }
        }
        triIdx = mesh.indices;
    }

    const float*    pWirePos  = wirePos.empty()    ? nullptr : wirePos.data();
    const uint32_t* pWireSeg  = wireSegIdx.empty() ? nullptr : wireSegIdx.data();
    const float*    pTriPos   = triPos.empty()     ? nullptr : triPos.data();
    const float*    pTriNorm  = triNorm.empty()    ? nullptr : triNorm.data();
    const uint32_t* pTriIdx   = triIdx.empty()     ? nullptr : triIdx.data();

    gpuRes_.upload(pid,
                   pWirePos,  static_cast<GLsizei>(wirePos.size() / 3),
                   pWireSeg,  static_cast<GLsizei>(wireSegIdx.size()),
                   pTriPos,   static_cast<GLsizei>(triPos.size() / 3),
                   pTriNorm,
                   pTriIdx,   static_cast<GLsizei>(triIdx.size()),
                   gen,
                   glue, caps_);
}

// ---------------------------------------------------------------------------
// render() – top-level entry point
// ---------------------------------------------------------------------------

void CadRendererGL::render(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    if (!ensureReady(glue)) return;
    if (plan.visibleInstances.empty()) return;

    // Upload geometry for all required part reps
    for (const auto& repKey : plan.requiredReps) {
        auto genIt = partGenMap.find(repKey.part);
        uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
        ensurePartUploaded(repKey.part, assembly, gen, glue);
    }

    std::fprintf(stderr,
        "CadRendererGL::render: vis=%zu wire=%zu shaded=%zu\n",
        plan.visibleInstances.size(), plan.wireItems.size(), plan.shadedItems.size());

    if (caps_.canUseInstanced() && shaders_.wireInst && shaders_.shadedInst) {
        renderInstanced(plan, assembly, glue, viewProj, partGenMap);
    } else {
        renderVboLoop(plan, assembly, glue, viewProj, partGenMap);
    }
}

// ---------------------------------------------------------------------------
// Tier-1: VBO-loop rendering (GL 2.0+)
// ---------------------------------------------------------------------------

// Bind a wire VBO, set up attribute 0 (position), draw segments.
// Works with or without VAO.
static void bindAndDrawWire(const CadWireGpu* w, const SoGLContext* glue,
                             GLint locPos)
{
    if (!w || w->segCount == 0) {
        std::fprintf(stderr, "bindAndDrawWire: skip (w=%p segCount=%d)\n",
                     (void*)w, w ? (int)w->segCount : -1);
        return;
    }

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(w->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
    }

    glue->glDrawElements(GL_LINES,
                         w->segCount * 2,
                         GL_UNSIGNED_INT,
                         nullptr);
    {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
            std::fprintf(stderr, "CadRendererGL: glDrawElements error: 0x%x\n", err);
    }

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// Bind a tri VBO, set up attributes 0 (position) and 1 (normal), draw.
static void bindAndDrawTri(const CadTriGpu* t, const SoGLContext* glue,
                            GLint locPos, GLint locNorm, bool& hasNorm)
{
    if (!t || t->idxCount == 0) return;

    hasNorm = (t->normBuf != 0);

    if (t->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(t->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));

        if (hasNorm && locNorm >= 0) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
            glue->glVertexAttribPointerARB(static_cast<GLuint>(locNorm), 3,
                                           GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
        }
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
    }

    glue->glDrawElements(GL_TRIANGLES, t->idxCount, GL_UNSIGNED_INT, nullptr);

    if (t->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        if (hasNorm && locNorm >= 0) {
            glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
        }
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void CadRendererGL::renderVboLoop(
        const CadFramePlan& plan,
        const SoCADAssembly& /*assembly*/,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& /*partGenMap*/)
{
    // OI stores matrices row-major.  GL reads them column-major.  Passing
    // the raw float[16] with GL_FALSE means GL transposes our row-major
    // matrix into the column-major form the shader expects, which is
    // exactly the GL column-vector convention.  (Same as SoGLSLShaderParameter.)
    const float* vpData = viewProj[0];

    // Debug: full viewProj matrix and GL state
    {
        std::fprintf(stderr,
            "CadRendererGL: viewProj=\n"
            "  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]\n"
            "  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]\n",
            vpData[0],vpData[1],vpData[2],vpData[3],
            vpData[4],vpData[5],vpData[6],vpData[7],
            vpData[8],vpData[9],vpData[10],vpData[11],
            vpData[12],vpData[13],vpData[14],vpData[15]);
        GLint viewport[4] = {};
        glGetIntegerv(GL_VIEWPORT, viewport);
        GLboolean dtest   = glIsEnabled(GL_DEPTH_TEST);
        GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        std::fprintf(stderr,
            "CadRendererGL: viewport=[%d,%d,%d,%d] depthTest=%d scissor=%d\n",
            viewport[0],viewport[1],viewport[2],viewport[3],
            (int)dtest, (int)scissor);
    }

    // a_pos=0, a_norm=1 are pinned via glBindAttribLocationARB before linking
    const GLint locPos  = 0;
    const GLint locNorm = 1;

    // ---- DIAGNOSTIC: draw a full-screen NDC triangle to verify GL path ----
    {
        // First: test raw clear
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        unsigned char pxClear[4] = {};
        glReadPixels(256, 256, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxClear);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        std::fprintf(stderr,
            "CadRendererGL: clear-to-red readPixels=[%u,%u,%u,%u]\n",
            pxClear[0], pxClear[1], pxClear[2], pxClear[3]);
        // Print function pointer state
        std::fprintf(stderr,
            "CadRendererGL: fn ptrs: useProg=%p vaptr=%p envatr=%p drawElem=%p genBuf=%p bindAttr=%p\n",
            (void*)(uintptr_t)glue->glUseProgramObjectARB,
            (void*)(uintptr_t)glue->glVertexAttribPointerARB,
            (void*)(uintptr_t)glue->glEnableVertexAttribArrayARB,
            (void*)(uintptr_t)glue->glDrawElements,
            (void*)(uintptr_t)glue->glGenBuffers,
            (void*)(uintptr_t)glue->glBindAttribLocationARB);

        static const float ndcTri[9] = {
            -0.9f, -0.9f, 0.0f,
             0.9f, -0.9f, 0.0f,
             0.0f,  0.9f, 0.0f,
        };
        static const GLuint ndcIdx[3] = { 0, 1, 2 };
        static const float identity16[16] = {
            1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1
        };
        static const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

        // Upload temporary VBOs
        GLuint tmpVBuf = 0, tmpIBuf = 0;
        glue->glGenBuffers(1, &tmpVBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, tmpVBuf);
        glue->glBufferData(GL_ARRAY_BUFFER, sizeof(ndcTri), ndcTri, GL_STATIC_DRAW);
        glue->glGenBuffers(1, &tmpIBuf);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tmpIBuf);
        glue->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ndcIdx), ndcIdx, GL_STATIC_DRAW);

        glue->glUseProgramObjectARB(shaders_.wire);
        GLint dlocVP    = glue->glGetUniformLocationARB(shaders_.wire, "u_viewProj");
        GLint dlocModel = glue->glGetUniformLocationARB(shaders_.wire, "u_model");
        GLint dlocColor = glue->glGetUniformLocationARB(shaders_.wire, "u_color");
        GLint dlocAPos  = glue->glGetAttribLocationARB(shaders_.wire, "a_pos");
        std::fprintf(stderr,
            "CadRendererGL: NDC-tri prog=%u locs: VP=%d model=%d color=%d a_pos=%d\n",
            shaders_.wire, dlocVP, dlocModel, dlocColor, dlocAPos);
        glue->glUniformMatrix4fvARB(dlocVP,    1, GL_FALSE, identity16);
        glue->glUniformMatrix4fvARB(dlocModel, 1, GL_FALSE, identity16);
        glue->glUniform4fvARB(dlocColor, 1, red);

        glue->glVertexAttribPointerARB(static_cast<GLuint>(dlocAPos < 0 ? 0 : dlocAPos),
                                       3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(dlocAPos < 0 ? 0 : dlocAPos));
        glue->glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
        GLenum err = glGetError();
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(dlocAPos < 0 ? 0 : dlocAPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glue->glUseProgramObjectARB(0);

        // Also try immediate-mode to confirm framebuffer writes work at all
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glBegin(GL_TRIANGLES);
        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        glVertex3f(-0.5f, -0.5f, 0.0f);
        glVertex3f( 0.5f, -0.5f, 0.0f);
        glVertex3f( 0.0f,  0.5f, 0.0f);
        glEnd();
        glEnable(GL_DEPTH_TEST);

        glue->glDeleteBuffers(1, &tmpVBuf);
        glue->glDeleteBuffers(1, &tmpIBuf);

        unsigned char px[4] = {};
        glReadPixels(256, 256, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        std::fprintf(stderr,
            "CadRendererGL: NDC-triangle test: drawErr=0x%x center=[%u,%u,%u,%u]\n",
            err, px[0], px[1], px[2], px[3]);
    }
    // ---- END DIAGNOSTIC ----

    // --- Wire pass ---
    if (!plan.wireItems.empty()) {
        glue->glUseProgramObjectARB(shaders_.wire);

        GLint locVP    = glue->glGetUniformLocationARB(shaders_.wire, "u_viewProj");
        GLint locModel = glue->glGetUniformLocationARB(shaders_.wire, "u_model");
        GLint locColor = glue->glGetUniformLocationARB(shaders_.wire, "u_color");

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vpData);

        for (const auto& item : plan.wireItems) {
            const CadWireGpu* w = gpuRes_.wireFor(item.rep.part);
            if (!w) continue;

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const auto& inst = plan.visibleInstances[item.baseInstance + i];
                if (i == 0) {
                    const float* t = inst.transform.data();
                    std::fprintf(stderr,
                        "CadRendererGL: wire inst[0] model=[%.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f | ...]\n",
                        t[0],t[1],t[2],t[3], t[4],t[5],t[6],t[7]);
                }
                glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(locColor, 1, rgba);
                bindAndDrawWire(w, glue, locPos);
            }
        }

        glue->glUseProgramObjectARB(0);
    }

    // Debug readback after wire pass
    {
        unsigned char pixel[4] = {};
        glReadPixels(256, 256, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        GLenum rderr = glGetError();
        std::fprintf(stderr,
            "CadRendererGL: post-wire readPixels(256,256)=[%u,%u,%u,%u] err=0x%x\n",
            pixel[0], pixel[1], pixel[2], pixel[3], rderr);
    }

    // --- Shaded pass ---
    if (!plan.shadedItems.empty()) {
        glue->glUseProgramObjectARB(shaders_.shaded);

        GLint locVP      = glue->glGetUniformLocationARB(shaders_.shaded, "u_viewProj");
        GLint locModel   = glue->glGetUniformLocationARB(shaders_.shaded, "u_model");
        GLint locColor   = glue->glGetUniformLocationARB(shaders_.shaded, "u_color");
        GLint locLight   = glue->glGetUniformLocationARB(shaders_.shaded, "u_lightDir");
        GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shaded, "u_hasNorm");

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vpData);
        glue->glUniform3fvARB(locLight, 1, kLightDir);

        for (const auto& item : plan.shadedItems) {
            const CadTriGpu* t = gpuRes_.triFor(item.rep.part);
            if (!t) continue;

            const bool hasNorm = (t->normBuf != 0);
            glue->glUniform1iARB(locHasNorm, hasNorm ? 1 : 0);

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const auto& inst = plan.visibleInstances[item.baseInstance + i];
                glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(locColor, 1, rgba);

                bool unused = hasNorm;
                bindAndDrawTri(t, glue, locPos, locNorm, unused);
            }
        }

        glue->glUseProgramObjectARB(0);
    }
}

// ---------------------------------------------------------------------------
// Tier-2: instanced rendering (GL 3.1+)
// ---------------------------------------------------------------------------

void CadRendererGL::renderInstanced(
        const CadFramePlan& plan,
        const SoCADAssembly& /*assembly*/,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& /*partGenMap*/)
{
    // Build per-instance vertex data (transform + colour)
    const size_t nInst = plan.visibleInstances.size();
    if (nInst == 0) return;

    std::vector<InstVertex> instData(nInst);
    for (size_t i = 0; i < nInst; ++i) {
        const auto& vi = plan.visibleInstances[i];
        std::memcpy(instData[i].transform, vi.transform.data(), 16 * sizeof(float));
        instData[i].color[0] = vi.rgba[0] / 255.0f;
        instData[i].color[1] = vi.rgba[1] / 255.0f;
        instData[i].color[2] = vi.rgba[2] / 255.0f;
        instData[i].color[3] = vi.rgba[3] / 255.0f;
    }

    gpuRes_.uploadInstanceData(instData.data(),
                                static_cast<GLsizeiptr>(nInst * sizeof(InstVertex)),
                                glue);

    const GLuint instVbo = gpuRes_.instanceVbo();
    const float* vp = viewProj[0];
    const GLsizei instStride = static_cast<GLsizei>(sizeof(InstVertex));

    // --- Helper to bind per-instance attributes ---
    auto bindInstAttribs = [&](GLuint prog) {
        glue->glBindBuffer(GL_ARRAY_BUFFER, instVbo);

        // a_instTransform occupies 4 consecutive attribute locations
        for (GLuint col = 0; col < 4; ++col) {
            GLint loc = glue->glGetAttribLocationARB(prog,
                col == 0 ? "a_instTransform" : nullptr);
            // If the name lookup works only for column 0, use fixed layout
            GLuint aloc = kInstTransformLoc + col;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                offsetof(InstVertex, transform) + col * 4 * sizeof(float));
            glue->glVertexAttribPointerARB(aloc, 4, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
            (void)loc;
        }

        // a_instColor
        {
            GLuint aloc = kInstColorLoc;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                offsetof(InstVertex, color));
            glue->glVertexAttribPointerARB(aloc, 4, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
        }
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    auto unbindInstAttribs = [&]() {
        for (GLuint col = 0; col < 4; ++col) {
            glue->glVertexAttribDivisor(kInstTransformLoc + col, 0);
            glue->glDisableVertexAttribArrayARB(kInstTransformLoc + col);
        }
        glue->glVertexAttribDivisor(kInstColorLoc, 0);
        glue->glDisableVertexAttribArrayARB(kInstColorLoc);
    };

    // --- Wire pass ---
    if (!plan.wireItems.empty()) {
        glue->glUseProgramObjectARB(shaders_.wireInst);

        GLint locVP  = glue->glGetUniformLocationARB(shaders_.wireInst, "u_viewProj");
        GLint locPos = glue->glGetAttribLocationARB(shaders_.wireInst,  "a_pos");
        if (locPos < 0) locPos = 0;

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vp);
        bindInstAttribs(shaders_.wireInst);

        for (const auto& item : plan.wireItems) {
            const CadWireGpu* w = gpuRes_.wireFor(item.rep.part);
            if (!w || w->segCount == 0) continue;

            if (w->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(w->vao);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
                glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                               GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
            }

            glue->glDrawElementsInstanced(GL_LINES,
                                          w->segCount * 2,
                                          GL_UNSIGNED_INT,
                                          nullptr,
                                          static_cast<GLsizei>(item.instanceCount));

            if (w->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(0);
            } else {
                glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }

        unbindInstAttribs();
        glue->glUseProgramObjectARB(0);
    }

    // --- Shaded pass ---
    if (!plan.shadedItems.empty()) {
        glue->glUseProgramObjectARB(shaders_.shadedInst);

        GLint locVP      = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_viewProj");
        GLint locLight   = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_lightDir");
        GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_hasNorm");
        GLint locPos     = glue->glGetAttribLocationARB(shaders_.shadedInst, "a_pos");
        GLint locNorm    = glue->glGetAttribLocationARB(shaders_.shadedInst, "a_norm");
        if (locPos < 0) locPos = 0;

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vp);
        glue->glUniform3fvARB(locLight, 1, kLightDir);
        bindInstAttribs(shaders_.shadedInst);

        for (const auto& item : plan.shadedItems) {
            const CadTriGpu* t = gpuRes_.triFor(item.rep.part);
            if (!t || t->idxCount == 0) continue;

            glue->glUniform1iARB(locHasNorm, (t->normBuf != 0) ? 1 : 0);

            if (t->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(t->vao);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
                glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                               GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                if (t->normBuf && locNorm >= 0) {
                    glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
                    glue->glVertexAttribPointerARB(static_cast<GLuint>(locNorm), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
                }
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
            }

            glue->glDrawElementsInstanced(GL_TRIANGLES,
                                          t->idxCount,
                                          GL_UNSIGNED_INT,
                                          nullptr,
                                          static_cast<GLsizei>(item.instanceCount));

            if (t->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(0);
            } else {
                if (t->normBuf && locNorm >= 0) {
                    glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
                }
                glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }

        unbindInstAttribs();
        glue->glUseProgramObjectARB(0);
    }
}

// ---------------------------------------------------------------------------
// releaseGpuResources()
// ---------------------------------------------------------------------------

void CadRendererGL::releaseGpuResources(const SoGLContext* glue)
{
    deleteShaders(glue);
    gpuRes_.releaseAll(glue);
    capsDetected_ = false;
    shadersContextId_ = 0;
}

} // namespace internal
} // namespace obol
