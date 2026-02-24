/*
 * arb8_dragger_example.cpp
 *
 * Self-contained demonstration of SoProceduralShape with interactive
 * SoDragger handles for parameter editing.
 *
 * The "ARB8" shape type is an arbitrary 8-vertex solid (a generalized
 * hexahedron whose faces are not required to be planar or rectangular).
 * It is the kind of primitive used in solid-modelling tools such as BRL-CAD.
 *
 * Shape parameters (24 floats = 8 vertices × 3 coordinates):
 *   params[ 0.. 2]  vertex 0 (x,y,z)
 *   params[ 3.. 5]  vertex 1
 *   ...
 *   params[21..23]  vertex 7
 *
 * Vertex ordering (bottom face CCW, top face CW when viewed from outside):
 *   Bottom: 0,1,2,3    Top: 4,5,6,7
 *
 * Three classes of interactive handles are demonstrated:
 *
 *   VERTEX handles (8, DRAG_POINT)
 *     Each vertex gets a SoDragPointDragger.  The user can pull any corner
 *     freely in 3-D space.  The drag callback updates the three floats that
 *     encode that vertex's xyz.
 *
 *   EDGE handles (12, DRAG_ON_PLANE — the face that the edge borders)
 *     Each edge gets a SoTranslate2Dragger at the edge midpoint.  Dragging
 *     translates both endpoint vertices by the same delta, keeping the edge
 *     length constant while moving the edge.
 *
 *   FACE handles (6, DRAG_ALONG_AXIS — the face normal)
 *     Each face gets a SoTranslate1Dragger at the face centre.  Dragging
 *     pushes or pulls all four face vertices along the outward face normal,
 *     uniformly extruding or retracting the face.
 *
 * Scene layout:
 *   Left    : ARB8 rendered as a filled solid (default box, no handles)
 *   Centre  : ARB8 with all vertex draggers visible (no pre-drag motion here;
 *              the draggers are placed but not animated)
 *   Right   : ARB8 with face handles illustrating the DRAG_ALONG_AXIS pattern
 *
 * Output: argv[1]+".rgb"  (800×600 SGI RGB, suitable for compare with PNG)
 */

#include "headless_utils.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// ARB8 geometry helpers
// ============================================================================

// Standard face definitions (quad vertex indices, CCW from outside)
static const int kArb8Faces[6][4] = {
  {0,3,2,1},  // bottom  (-Y)
  {4,5,6,7},  // top     (+Y)
  {0,1,5,4},  // front   (-Z)
  {1,2,6,5},  // right   (+X)
  {2,3,7,6},  // back    (+Z)
  {3,0,4,7}   // left    (-X)
};

// Standard edge definitions (vertex index pairs)
static const int kArb8Edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},   // bottom ring
  {4,5},{5,6},{6,7},{7,4},   // top ring
  {0,4},{1,5},{2,6},{3,7}    // vertical
};

// Retrieve vertex i from params (3 floats per vertex)
static inline SbVec3f arb8Vertex(const float* params, int i)
{
  return SbVec3f(params[3*i], params[3*i+1], params[3*i+2]);
}

// Face centroid
static SbVec3f faceCentroid(const float* params, const int faceVerts[4])
{
  SbVec3f c(0,0,0);
  for (int k=0;k<4;++k) c += arb8Vertex(params, faceVerts[k]);
  return c * 0.25f;
}

// Face normal (approx, averaged from two triangle normals)
static SbVec3f faceNormal(const float* params, const int faceVerts[4])
{
  SbVec3f v0 = arb8Vertex(params, faceVerts[0]);
  SbVec3f v1 = arb8Vertex(params, faceVerts[1]);
  SbVec3f v2 = arb8Vertex(params, faceVerts[2]);
  SbVec3f v3 = arb8Vertex(params, faceVerts[3]);
  SbVec3f n = (v1-v0).cross(v2-v0) + (v2-v0).cross(v3-v0);
  n.normalize();
  return n;
}

// Default unit-box params (same as a 2×2×2 box centred at origin)
static const float kArb8Default[24] = {
  -1,-1,-1,   1,-1,-1,   1,-1, 1,  -1,-1, 1,   // bottom (y=-1)
  -1, 1,-1,   1, 1,-1,   1, 1, 1,  -1, 1, 1    // top    (y=+1)
};

// JSON schema: 8 vertices, each with 3 float params (x, y, z)
static const char* kArb8Schema = R"({
  "type"  : "ARB8",
  "label" : "Arbitrary 8-Vertex Solid",
  "params": [
    {"name":"v0x","type":"float","default":-1.0,"label":"V0 X"},
    {"name":"v0y","type":"float","default":-1.0,"label":"V0 Y"},
    {"name":"v0z","type":"float","default":-1.0,"label":"V0 Z"},
    {"name":"v1x","type":"float","default": 1.0,"label":"V1 X"},
    {"name":"v1y","type":"float","default":-1.0,"label":"V1 Y"},
    {"name":"v1z","type":"float","default":-1.0,"label":"V1 Z"},
    {"name":"v2x","type":"float","default": 1.0,"label":"V2 X"},
    {"name":"v2y","type":"float","default":-1.0,"label":"V2 Y"},
    {"name":"v2z","type":"float","default": 1.0,"label":"V2 Z"},
    {"name":"v3x","type":"float","default":-1.0,"label":"V3 X"},
    {"name":"v3y","type":"float","default":-1.0,"label":"V3 Y"},
    {"name":"v3z","type":"float","default": 1.0,"label":"V3 Z"},
    {"name":"v4x","type":"float","default":-1.0,"label":"V4 X"},
    {"name":"v4y","type":"float","default": 1.0,"label":"V4 Y"},
    {"name":"v4z","type":"float","default":-1.0,"label":"V4 Z"},
    {"name":"v5x","type":"float","default": 1.0,"label":"V5 X"},
    {"name":"v5y","type":"float","default": 1.0,"label":"V5 Y"},
    {"name":"v5z","type":"float","default":-1.0,"label":"V5 Z"},
    {"name":"v6x","type":"float","default": 1.0,"label":"V6 X"},
    {"name":"v6y","type":"float","default": 1.0,"label":"V6 Y"},
    {"name":"v6z","type":"float","default": 1.0,"label":"V6 Z"},
    {"name":"v7x","type":"float","default":-1.0,"label":"V7 X"},
    {"name":"v7y","type":"float","default": 1.0,"label":"V7 Y"},
    {"name":"v7z","type":"float","default": 1.0,"label":"V7 Z"}
  ]
})";

// ============================================================================
// Bbox callback
// ============================================================================
static void arb8_bbox(const float* params, int numParams,
                      SbVec3f& minPt, SbVec3f& maxPt, void*)
{
  if (numParams < 24) { minPt.setValue(-1,-1,-1); maxPt.setValue(1,1,1); return; }
  float mnx=params[0],mny=params[1],mnz=params[2];
  float mxx=params[0],mxy=params[1],mxz=params[2];
  for (int i=1;i<8;++i) {
    if (params[3*i  ]<mnx) mnx=params[3*i  ];
    if (params[3*i+1]<mny) mny=params[3*i+1];
    if (params[3*i+2]<mnz) mnz=params[3*i+2];
    if (params[3*i  ]>mxx) mxx=params[3*i  ];
    if (params[3*i+1]>mxy) mxy=params[3*i+1];
    if (params[3*i+2]>mxz) mxz=params[3*i+2];
  }
  minPt.setValue(mnx,mny,mnz);
  maxPt.setValue(mxx,mxy,mxz);
}

// ============================================================================
// Geometry callback
// ============================================================================
static void arb8_geom(const float* params, int numParams,
                      SoProceduralTriangles* tris,
                      SoProceduralWireframe* wire,
                      void*)
{
  // Fall back to a unit box if params are missing
  const float* p = (numParams >= 24) ? params : kArb8Default;

  if (tris) {
    tris->vertices.clear(); tris->normals.clear(); tris->indices.clear();

    // Emit 2 triangles per quad face (12 triangles total)
    for (int f=0;f<6;++f) {
      const int* fv = kArb8Faces[f];
      SbVec3f v0 = arb8Vertex(p, fv[0]);
      SbVec3f v1 = arb8Vertex(p, fv[1]);
      SbVec3f v2 = arb8Vertex(p, fv[2]);
      SbVec3f v3 = arb8Vertex(p, fv[3]);
      SbVec3f nrm = (v1-v0).cross(v2-v0); nrm.normalize();

      int base = static_cast<int>(tris->vertices.size());
      tris->vertices.insert(tris->vertices.end(), {v0,v1,v2,v3});
      tris->normals .insert(tris->normals .end(), {nrm,nrm,nrm,nrm});
      tris->indices .insert(tris->indices.end(),
        {base,base+1,base+2, base,base+2,base+3});
    }
  }

  if (wire) {
    wire->vertices.clear(); wire->segments.clear();
    for (int i=0;i<8;++i) wire->vertices.push_back(arb8Vertex(p,i));
    for (int e=0;e<12;++e) {
      wire->segments.push_back(kArb8Edges[e][0]);
      wire->segments.push_back(kArb8Edges[e][1]);
    }
  }
}

// ============================================================================
// Handles callback — returns vertex, edge, and face handles
//
// Handle index layout:
//   0..7   vertex handles  (DRAG_POINT)
//   8..19  edge handles    (DRAG_ON_PLANE, plane = adjacent face normal)
//   20..25 face handles    (DRAG_ALONG_AXIS, axis = face normal)
// ============================================================================
static void arb8_handles(const float* params, int numParams,
                          std::vector<SoProceduralHandle>& handles,
                          void*)
{
  const float* p = (numParams >= 24) ? params : kArb8Default;

  handles.clear();

  // --- Vertex handles ---
  for (int i=0;i<8;++i) {
    SoProceduralHandle h;
    h.position = arb8Vertex(p, i);
    h.dragType = SoProceduralHandle::DRAG_POINT;
    char buf[32]; snprintf(buf,sizeof(buf),"v%d",i);
    h.name = buf;
    handles.push_back(h);
  }

  // --- Edge handles (midpoint, constrained to a face plane) ---
  for (int e=0;e<12;++e) {
    SbVec3f a = arb8Vertex(p, kArb8Edges[e][0]);
    SbVec3f b = arb8Vertex(p, kArb8Edges[e][1]);
    SoProceduralHandle h;
    h.position = (a + b) * 0.5f;
    h.dragType = SoProceduralHandle::DRAG_ON_PLANE;
    // Use normal of the first face that contains this edge as the plane normal
    h.normal = SbVec3f(0,1,0); // fallback
    for (int f=0;f<6;++f) {
      const int* fv = kArb8Faces[f];
      for (int k=0;k<4;++k) {
        if (fv[k]==kArb8Edges[e][0] && fv[(k+1)%4]==kArb8Edges[e][1]) {
          h.normal = faceNormal(p, fv);
          goto done_edge;
        }
      }
    }
    done_edge:
    char buf[32]; snprintf(buf,sizeof(buf),"e%d",e);
    h.name = buf;
    handles.push_back(h);
  }

  // --- Face handles (centroid, constrained along face normal) ---
  for (int f=0;f<6;++f) {
    SoProceduralHandle h;
    h.position = faceCentroid(p, kArb8Faces[f]);
    h.dragType = SoProceduralHandle::DRAG_ALONG_AXIS;
    h.axis     = faceNormal(p, kArb8Faces[f]);
    char buf[32]; snprintf(buf,sizeof(buf),"f%d",f);
    h.name = buf;
    handles.push_back(h);
  }
}

// ============================================================================
// Handle drag callback — updates params when a handle is moved
// ============================================================================
static void arb8_handleDrag(float* params, int numParams,
                             int handleIndex,
                             const SbVec3f& oldPos, const SbVec3f& newPos,
                             void*)
{
  if (numParams < 24) return;
  SbVec3f delta = newPos - oldPos;

  if (handleIndex < 8) {
    // Vertex handle: move the single vertex
    int i = handleIndex;
    params[3*i  ] += delta[0];
    params[3*i+1] += delta[1];
    params[3*i+2] += delta[2];

  } else if (handleIndex < 20) {
    // Edge handle: move both endpoint vertices
    int e = handleIndex - 8;
    for (int k=0;k<2;++k) {
      int v = kArb8Edges[e][k];
      params[3*v  ] += delta[0];
      params[3*v+1] += delta[1];
      params[3*v+2] += delta[2];
    }

  } else if (handleIndex < 26) {
    // Face handle: move all four face vertices
    int f = handleIndex - 20;
    for (int k=0;k<4;++k) {
      int v = kArb8Faces[f][k];
      params[3*v  ] += delta[0];
      params[3*v+1] += delta[1];
      params[3*v+2] += delta[2];
    }
  }
}

// ============================================================================
// Build a scene node for one ARB8 instance with optional dragger handles
// ============================================================================
static SoSeparator* makeArb8Scene(const char* typeTag,
                                   const float* paramVals,
                                   float r, float g, float b,
                                   float tx,
                                   bool withHandles)
{
  SoSeparator* sep = new SoSeparator;

  SoTranslation* t = new SoTranslation;
  t->translation.setValue(tx, 0.f, 0.f);
  sep->addChild(t);

  // Solid shape
  SoSeparator* shapeSep = new SoSeparator;
  SoMaterial* mat = new SoMaterial;
  mat->diffuseColor.setValue(r, g, b);
  mat->specularColor.setValue(0.4f,0.4f,0.4f);
  mat->shininess.setValue(0.5f);
  shapeSep->addChild(mat);

  SoProceduralShape* shape = new SoProceduralShape;
  shape->shapeType.setValue(typeTag);
  shape->params.setValues(0, 24, paramVals);
  shapeSep->addChild(shape);
  sep->addChild(shapeSep);

  // Wireframe overlay so faces are visible even without lighting
  SoSeparator* wireSep = new SoSeparator;
  SoDrawStyle* ds = new SoDrawStyle;
  ds->style.setValue(SoDrawStyle::LINES);
  wireSep->addChild(ds);
  SoMaterial* wireMat = new SoMaterial;
  wireMat->diffuseColor.setValue(0.1f,0.1f,0.1f);
  wireSep->addChild(wireMat);
  SoProceduralShape* wireShape = new SoProceduralShape;
  wireShape->shapeType.setValue(typeTag);
  wireShape->params.setValues(0, 24, paramVals);
  wireSep->addChild(wireShape);
  sep->addChild(wireSep);

  // Optionally build dragger handles
  if (withHandles) {
    SoSeparator* handles = shape->buildHandleDraggers();
    if (handles) {
      sep->addChild(handles);
    }
  }

  return sep;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv)
{
  initCoinHeadless();

  // Register the ARB8 type with both geometry and handle callbacks.
  SoProceduralShape::registerShapeType(
      "ARB8", kArb8Schema,
      arb8_bbox, arb8_geom,
      arb8_handles, arb8_handleDrag);

  SoSeparator* root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera* cam = new SoPerspectiveCamera;
  root->addChild(cam);

  SoDirectionalLight* light = new SoDirectionalLight;
  light->direction.setValue(-0.5f, -0.8f, -0.6f);
  root->addChild(light);

  // A slightly sheared box to show non-rectangular faces
  float sheared[24];
  memcpy(sheared, kArb8Default, sizeof(sheared));
  // Tilt the top face: shift top vertices by 0.3 in X
  for (int i=4;i<8;++i) sheared[3*i] += 0.4f;

  // Left: plain solid ARB8 (default box params)
  root->addChild(makeArb8Scene("ARB8", kArb8Default,
                               0.2f, 0.5f, 0.9f, -3.5f, false));

  // Centre: sheared ARB8 with vertex draggers
  root->addChild(makeArb8Scene("ARB8", sheared,
                               0.9f, 0.5f, 0.2f,  0.0f, true));

  // Right: same sheared ARB8 with wireframe only
  SoSeparator* rightSep = new SoSeparator;
  SoTranslation* rt = new SoTranslation;
  rt->translation.setValue(3.5f, 0.f, 0.f);
  rightSep->addChild(rt);
  SoDrawStyle* rds = new SoDrawStyle;
  rds->style.setValue(SoDrawStyle::LINES);
  rightSep->addChild(rds);
  SoMaterial* rMat = new SoMaterial;
  rMat->diffuseColor.setValue(0.1f, 0.9f, 0.3f);
  rightSep->addChild(rMat);
  SoProceduralShape* rShape = new SoProceduralShape;
  rShape->shapeType.setValue("ARB8");
  rShape->params.setValues(0, 24, sheared);
  rightSep->addChild(rShape);
  root->addChild(rightSep);

  SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
  cam->viewAll(root, vp);
  cam->position.setValue(cam->position.getValue() * 1.3f);

  char outpath[1024];
  if (argc > 1)
    snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
  else
    snprintf(outpath, sizeof(outpath), "arb8_dragger_example.rgb");

  bool ok = renderToFile(root, outpath);
  root->unref();
  return ok ? 0 : 1;
}
