#ifndef COIN_SOPROCEDURALSHAPE_H
#define COIN_SOPROCEDURALSHAPE_H

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

  SoProceduralShape allows applications to introduce custom 3D shape types
  into the Obol scene graph without writing C++ subclasses of SoShape for
  every shape variant.  Instead, the application:

  1. Defines a \e shape \e type by calling registerShapeType(), supplying:
     - A unique string name (e.g. "TruncatedCone")
     - A JSON schema string describing the editable parameters (names, types,
       defaults, and ranges) — this is stored as-is and made available to
       editor tools; it is not interpreted by Obol itself.
     - A bounding-box callback (SoProceduralBBoxCB) that fills in the AABB
       for a given parameter set.
     - A geometry callback (SoProceduralGeomCB) that fills triangles and/or
       a 3-D wireframe for a given parameter set.

  2. Creates one or more SoProceduralShape nodes, sets their \c shapeType
     field to the registered name, and populates the \c params field with
     the parameter values (ordered identically to the JSON schema).

  The node participates fully in the Obol scene-graph action system:
  - \c SoGetBoundingBoxAction calls the bbox callback.
  - \c SoGLRenderAction renders triangles (FILLED draw style) or the
    wireframe (LINES draw style) via the OpenGL fixed-function path.
  - \c SoRayPickAction picks against the triangle mesh.
  - \c SoCallbackAction / \c SoGetPrimitiveCountAction work through
    the standard SoShape::generatePrimitives() mechanism.

  \b JSON parameter schema format:
  \code
  {
    "type"  : "TruncatedCone",
    "label" : "Truncated Cone",
    "params": [
      { "name": "bottomRadius", "type": "float", "default": 1.0,
        "min": 0.001, "max": 100.0, "label": "Bottom Radius" },
      { "name": "topRadius",    "type": "float", "default": 0.5,
        "min": 0.0,   "max": 100.0, "label": "Top Radius" },
      { "name": "height",       "type": "float", "default": 2.0,
        "min": 0.001, "max": 100.0, "label": "Height" },
      { "name": "sides",        "type": "int",   "default": 16,
        "min": 3,     "max": 128,   "label": "Number of Sides" }
    ]
  }
  \endcode

  Supported parameter types: "float", "int", "bool" (stored as 0/1).

  \b FILE FORMAT/DEFAULTS:
  \code
    ProceduralShape {
        shapeType  ""
        schemaJSON ""
        params     []
    }
  \endcode

  \sa SoShape, SoIndexedFaceSet, SoIndexedLineSet
*/

#include <vector>
#include <cstdint>

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoMFFloat.h>

// ---------------------------------------------------------------------------
// Geometry data containers filled by application callbacks
// ---------------------------------------------------------------------------

/*!
  \struct SoProceduralTriangles
  \brief Triangle-mesh output filled by a SoProceduralGeomCB.

  The application callback populates \c vertices, (optionally) \c normals,
  and \c indices.  \c indices must contain triples of vertex indices;
  each triple describes one triangle.
*/
struct SoProceduralTriangles {
  std::vector<SbVec3f>  vertices;  //!< One entry per unique vertex position.
  std::vector<SbVec3f>  normals;   //!< Per-vertex normals; may be empty.
  std::vector<int32_t>  indices;   //!< Three indices per triangle.
};

/*!
  \struct SoProceduralWireframe
  \brief 3-D wireframe output filled by a SoProceduralGeomCB.

  \c vertices holds the unique 3-D positions; \c segments holds pairs of
  vertex indices that define line segments.  A value of \c -1 may be used
  as a polyline-end sentinel instead of repeating endpoints.
*/
struct SoProceduralWireframe {
  std::vector<SbVec3f>  vertices;  //!< One entry per unique vertex position.
  std::vector<int32_t>  segments;  //!< Index pairs; -1 = end of polyline.
};

// ---------------------------------------------------------------------------
// Callback typedefs
// ---------------------------------------------------------------------------

/*!
  \typedef SoProceduralBBoxCB
  \brief Callback that computes the axis-aligned bounding box of a shape.

  \param params    Pointer to the current parameter values array.
  \param numParams Number of values in \a params.
  \param minPt     [out] Minimum corner of the AABB in object space.
  \param maxPt     [out] Maximum corner of the AABB in object space.
  \param userdata  Opaque pointer supplied at registration time.
*/
typedef void (*SoProceduralBBoxCB)(const float* params,
                                   int          numParams,
                                   SbVec3f&     minPt,
                                   SbVec3f&     maxPt,
                                   void*        userdata);

/*!
  \typedef SoProceduralGeomCB
  \brief Callback that generates triangle and/or wireframe geometry.

  Either \a tris or \a wire (but not both) may be \c nullptr, indicating
  that the caller does not need that geometry variant.  The callback should
  only populate the non-null output.

  \param params    Pointer to the current parameter values array.
  \param numParams Number of values in \a params.
  \param tris      [out] Triangle mesh; may be \c nullptr.
  \param wire      [out] Wireframe geometry; may be \c nullptr.
  \param userdata  Opaque pointer supplied at registration time.
*/
typedef void (*SoProceduralGeomCB)(const float*             params,
                                   int                      numParams,
                                   SoProceduralTriangles*   tris,
                                   SoProceduralWireframe*   wire,
                                   void*                    userdata);

// ---------------------------------------------------------------------------
// SoProceduralShape node
// ---------------------------------------------------------------------------

class COIN_DLL_API SoProceduralShape : public SoShape {
  typedef SoShape inherited;

  SO_NODE_HEADER(SoProceduralShape);

public:
  static void initClass();

  SoProceduralShape();

  // ----- Fields -----

  //! Registered shape type name (e.g. "TruncatedCone").
  SoSFString shapeType;

  //! JSON parameter-schema string (informational — see class docs).
  //! Populated automatically from the registry when \c shapeType is set
  //! via setShapeType(), or set manually by the application.
  SoSFString schemaJSON;

  //! Current parameter values, ordered as defined in the JSON schema.
  SoMFFloat  params;

  // ----- Convenience -----

  //! Set \c shapeType and automatically populate \c schemaJSON and
  //! \c params (with schema defaults) from the type registry.
  void setShapeType(const char* typeName);

  // ----- Registry -----

  /*!
    Register a new procedural shape type.

    \param typeName   Unique identifier string for this shape type.
    \param schemaJSON JSON string describing the editable parameters
                      (see class documentation for format).  Pass an
                      empty string if no schema is provided.
    \param bboxCB     Bounding-box computation callback.  Must not be null.
    \param geomCB     Geometry generation callback.  Must not be null.
    \param userdata   Opaque pointer passed through to both callbacks.
    \return TRUE on success; FALSE if \a typeName is already registered.
  */
  static SbBool registerShapeType(const char*          typeName,
                                  const char*          schemaJSON,
                                  SoProceduralBBoxCB   bboxCB,
                                  SoProceduralGeomCB   geomCB,
                                  void*                userdata = nullptr);

  //! Return the JSON schema string for a registered type, or \c nullptr.
  static const char* getSchemaJSON(const char* typeName);

  //! Return true if a shape type with this name has been registered.
  static SbBool isRegistered(const char* typeName);

  virtual void getPrimitiveCount(SoGetPrimitiveCountAction* action) override;

protected:
  virtual ~SoProceduralShape();

  virtual void generatePrimitives(SoAction* action) override;
  virtual void computeBBox(SoAction* action, SbBox3f& box,
                           SbVec3f & center) override;

private:
  void emitTriangles(SoAction* action,
                     const SoProceduralTriangles& tris);
  void emitWireframe(SoAction* action,
                     const SoProceduralWireframe& wire);
};

#endif // !COIN_SOPROCEDURALSHAPE_H
