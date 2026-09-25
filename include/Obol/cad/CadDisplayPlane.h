/**************************************************************************\
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
\**************************************************************************/

#ifndef OBOL_CAD_DISPLAY_PLANE_H
#define OBOL_CAD_DISPLAY_PLANE_H

#include <Inventor/basic.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>

namespace Obol {

class PartGeometry;

/** Camera-facing geometry with a model-space anchor and fixed pixel scale.
 * Instance transforms place the anchor; they do not rotate or scale the
 * display offsets. Geometry coordinates are offsets from this anchor.
 */
struct CadDisplayPlane {
    SbVec3f anchor = SbVec3f(0.0f, 0.0f, 0.0f);
    float pixelsPerUnit = 1.0f;
};

/** Resolve display offsets to root coordinates for one camera and viewport.
 * Returns false for an unavailable viewport/projection or nonpositive
 * homogeneous anchor depth. On failure the output is unchanged. Projection is
 * shared by drawing, bounds, selection and scene collection.
 */
OBOL_DLL_API bool cadDisplayPlaneTransform(const CadDisplayPlane& plane,
    const SbMatrix& localToRoot, const SbMatrix& rootToClip,
    const SbVec2s& viewportSize, SbMatrix& geometryToRoot);

/** Bounds of stored coordinates, before any model or display placement. */
OBOL_DLL_API SbBox3f cadPartGeometryBounds(const PartGeometry& geometry);

/** Camera-independent source bounds; a display plane contributes its anchor. */
OBOL_DLL_API SbBox3f cadPartModelBounds(const PartGeometry& geometry);

} // namespace Obol

#endif // OBOL_CAD_DISPLAY_PLANE_H
