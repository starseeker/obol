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
  \class SoProceduralShape SoProceduralShape.h Inventor/nodes/SoProceduralShape.h
  \brief Generic scene-graph node for application-defined procedural geometry.
  \ingroup coin_nodes
  \sa SoShape
*/

#include <Inventor/nodes/SoProceduralShape.h>

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <Inventor/SbVec4f.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/elements/SoDrawStyleElement.h>
#include <Inventor/misc/SoState.h>

#include "nodes/SoSubNodeP.h"

// ---------------------------------------------------------------------------
// Shape type registry
// ---------------------------------------------------------------------------

namespace {

struct ShapeTypeEntry {
  std::string          typeName;
  std::string          schemaJSON;
  SoProceduralBBoxCB   bboxCB;
  SoProceduralGeomCB   geomCB;
  void*                userdata;
};

// Registry storage: simple sorted map, populated at app-init time.
// No mutex: registrations must complete before rendering starts.
std::map<std::string, ShapeTypeEntry> s_registry;

static const ShapeTypeEntry* findEntry(const char* typeName)
{
  if (!typeName || !*typeName) return nullptr;
  auto it = s_registry.find(typeName);
  return (it != s_registry.end()) ? &it->second : nullptr;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SO_NODE macros
// ---------------------------------------------------------------------------

SO_NODE_SOURCE(SoProceduralShape);

// ---------------------------------------------------------------------------
// initClass / constructor / destructor
// ---------------------------------------------------------------------------

void
SoProceduralShape::initClass()
{
  SO_NODE_INIT_CLASS(SoProceduralShape, SoShape, "SoShape");
}

SoProceduralShape::SoProceduralShape()
{
  SO_NODE_CONSTRUCTOR(SoProceduralShape);
  SO_NODE_ADD_FIELD(shapeType,  (""));
  SO_NODE_ADD_FIELD(schemaJSON, (""));
  SO_NODE_ADD_FIELD(params,     (0.0f));
  this->params.setNum(0);
}

SoProceduralShape::~SoProceduralShape()
{
}

// ---------------------------------------------------------------------------
// Registry helpers
// ---------------------------------------------------------------------------

// static
SbBool
SoProceduralShape::registerShapeType(const char*        typeName,
                                     const char*        schemaJSON,
                                     SoProceduralBBoxCB bboxCB,
                                     SoProceduralGeomCB geomCB,
                                     void*              userdata)
{
  if (!typeName || !*typeName || !bboxCB || !geomCB) return FALSE;

  if (s_registry.find(typeName) != s_registry.end()) return FALSE;

  ShapeTypeEntry entry;
  entry.typeName  = typeName;
  entry.schemaJSON = schemaJSON ? schemaJSON : "";
  entry.bboxCB    = bboxCB;
  entry.geomCB    = geomCB;
  entry.userdata  = userdata;

  s_registry[typeName] = entry;
  return TRUE;
}

// static
const char*
SoProceduralShape::getSchemaJSON(const char* typeName)
{
  auto it = s_registry.find(typeName);
  if (it == s_registry.end()) return nullptr;
  return it->second.schemaJSON.c_str();
}

// static
SbBool
SoProceduralShape::isRegistered(const char* typeName)
{
  return s_registry.find(typeName) != s_registry.end() ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Convenience: setShapeType
// ---------------------------------------------------------------------------

/*!
  Set the \c shapeType field to \a typeName, copy the registered JSON schema
  into \c schemaJSON, and populate \c params with the default values extracted
  from the schema's "default" entries (in declaration order).

  If \a typeName is not registered the fields are still set but \c params is
  left empty.
*/
void
SoProceduralShape::setShapeType(const char* typeName)
{
  this->shapeType.setValue(typeName ? typeName : "");

  const ShapeTypeEntry* entry = findEntry(typeName);
  if (!entry) return;

  this->schemaJSON.setValue(entry->schemaJSON.c_str());

  // Extract "default": <number> entries from the schema JSON in order.
  // We use a simple linear scan that works for the documented schema format.
  const char* schema = entry->schemaJSON.c_str();
  std::vector<float> defaults;
  const char* p = schema;
  while ((p = strstr(p, "\"default\"")) != nullptr) {
    p += 9; // skip past "default"
    // Skip whitespace and the colon separator
    while (*p && (*p == ':' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      ++p;
    if (!*p) break;
    char* end = nullptr;
    float v = strtof(p, &end);
    if (end && end > p)
      defaults.push_back(v);
    p = end ? end : p + 1;
  }

  if (!defaults.empty()) {
    this->params.setNum(0);
    this->params.setValues(0, static_cast<int>(defaults.size()), defaults.data());
  }
}

// ---------------------------------------------------------------------------
// computeBBox
// ---------------------------------------------------------------------------

void
SoProceduralShape::computeBBox(SoAction* /* action */, SbBox3f& box, SbVec3f& center)
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());

  if (!entry) {
    // Unknown type: return a unit bounding box so the scene graph stays valid.
    box.setBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    center.setValue(0.0f, 0.0f, 0.0f);
    return;
  }

  int   nparams     = this->params.getNum();
  const float* pv   = nparams > 0 ? this->params.getValues(0) : nullptr;

  SbVec3f minPt(-1.0f, -1.0f, -1.0f);
  SbVec3f maxPt( 1.0f,  1.0f,  1.0f);

  entry->bboxCB(pv, nparams, minPt, maxPt, entry->userdata);

  box.setBounds(minPt, maxPt);
  center = (minPt + maxPt) * 0.5f;
}

// ---------------------------------------------------------------------------
// generatePrimitives — used by all actions: GL render, picking, callback, etc.
//
// This is called by the base class SoShape::GLRender() after it sets up
// the material bundle and internal render state. For GLRenderAction the
// draw style is consulted so that wireframe geometry is emitted when LINES
// is active; for all other actions (ray-pick, callback, prim count) the
// triangle mesh is used.
// ---------------------------------------------------------------------------

void
SoProceduralShape::generatePrimitives(SoAction* action)
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return;

  int         nparams = this->params.getNum();
  const float* pv     = nparams > 0 ? this->params.getValues(0) : nullptr;

  // For GLRenderAction, respect the current draw style.
  bool wireframeMode = false;
  if (action->getTypeId().isDerivedFrom(SoGLRenderAction::getClassTypeId())) {
    SoState* state = static_cast<SoGLRenderAction*>(action)->getState();
    wireframeMode  = (SoDrawStyleElement::get(state) == SoDrawStyleElement::LINES);
  }

  if (wireframeMode) {
    SoProceduralWireframe wire;
    entry->geomCB(pv, nparams, nullptr, &wire, entry->userdata);
    emitWireframe(action, wire);
  } else {
    SoProceduralTriangles tris;
    entry->geomCB(pv, nparams, &tris, nullptr, entry->userdata);
    emitTriangles(action, tris);
  }
}

// ---------------------------------------------------------------------------
// getPrimitiveCount
// ---------------------------------------------------------------------------

void
SoProceduralShape::getPrimitiveCount(SoGetPrimitiveCountAction* action)
{
  if (!this->shouldPrimitiveCount(action)) return;

  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return;

  int         nparams = this->params.getNum();
  const float* pv     = nparams > 0 ? this->params.getValues(0) : nullptr;

  SoProceduralTriangles tris;
  entry->geomCB(pv, nparams, &tris, nullptr, entry->userdata);

  int ntris = static_cast<int>(tris.indices.size()) / 3;
  for (int i = 0; i < ntris; ++i)
    action->incNumTriangles();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void
SoProceduralShape::emitTriangles(SoAction*                    action,
                                 const SoProceduralTriangles& tris)
{
  if (tris.vertices.empty() || tris.indices.empty()) return;

  const int  nv          = static_cast<int>(tris.vertices.size());
  const bool hasNormals  = (static_cast<int>(tris.normals.size()) == nv);
  const SbVec3f defNorm(0.0f, 0.0f, 1.0f);

  SoPrimitiveVertex pv;

  this->beginShape(action, SoShape::TRIANGLES);
  for (size_t i = 0; i + 2 < tris.indices.size(); i += 3) {
    for (int j = 0; j < 3; ++j) {
      int32_t idx = tris.indices[i + static_cast<size_t>(j)];
      if (idx < 0 || idx >= nv) continue;

      pv.setPoint(tris.vertices[static_cast<size_t>(idx)]);
      pv.setNormal(hasNormals ? tris.normals[static_cast<size_t>(idx)]
                              : defNorm);
      pv.setTextureCoords(SbVec4f(0.0f, 0.0f, 0.0f, 1.0f));
      this->shapeVertex(&pv);
    }
  }
  this->endShape();
}

void
SoProceduralShape::emitWireframe(SoAction*                    action,
                                 const SoProceduralWireframe& wire)
{
  if (wire.vertices.empty() || wire.segments.empty()) return;

  const int     nv       = static_cast<int>(wire.vertices.size());
  const SbVec3f defNorm(0.0f, 0.0f, 1.0f);

  SoPrimitiveVertex pv;
  pv.setNormal(defNorm);
  pv.setTextureCoords(SbVec4f(0.0f, 0.0f, 0.0f, 1.0f));

  this->beginShape(action, SoShape::LINES);
  for (size_t i = 0; i + 1 < wire.segments.size(); i += 2) {
    int32_t a = wire.segments[i];
    int32_t b = wire.segments[i + 1];
    if (a < 0 || b < 0) continue;
    if (a >= nv || b >= nv) continue;

    pv.setPoint(wire.vertices[static_cast<size_t>(a)]);
    this->shapeVertex(&pv);
    pv.setPoint(wire.vertices[static_cast<size_t>(b)]);
    this->shapeVertex(&pv);
  }
  this->endShape();
}
