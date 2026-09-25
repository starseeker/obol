/**************************************************************************\
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
\**************************************************************************/

#include <Obol/cad/CadDisplayPlane.h>
#include <Obol/cad/CadGeometry.h>

#include <cmath>

namespace Obol {

bool
cadDisplayPlaneTransform(const CadDisplayPlane& plane,
    const SbMatrix& localToRoot, const SbMatrix& rootToClip,
    const SbVec2s& viewportSize, SbMatrix& geometryToRoot)
{
    if (viewportSize[0] <= 0 || viewportSize[1] <= 0 ||
            !std::isfinite(plane.pixelsPerUnit) || plane.pixelsPerUnit <= 0.0f)
        return false;
    SbVec3f anchor;
    localToRoot.multVecMatrix(plane.anchor, anchor);
    const double clipW = double(anchor[0]) * rootToClip[0][3] +
        double(anchor[1]) * rootToClip[1][3] +
        double(anchor[2]) * rootToClip[2][3] + rootToClip[3][3];
    if (!std::isfinite(clipW) || clipW <= 0.0)
        return false;

    const float determinant = rootToClip.det4();
    if (!std::isfinite(determinant) || determinant == 0.0f)
        return false;
    const SbMatrix clipToRoot = rootToClip.inverse();
    SbVec3f right, up;
    // Direction conversion avoids subtracting nearly equal world positions.
    const double rightScale = 2.0 * clipW * plane.pixelsPerUnit / viewportSize[0];
    const double upScale = 2.0 * clipW * plane.pixelsPerUnit / viewportSize[1];
    for (int axis = 0; axis < 3; ++axis) {
        right[axis] = static_cast<float>(clipToRoot[0][axis] * rightScale);
        up[axis] = static_cast<float>(clipToRoot[1][axis] * upScale);
    }
    SbVec3f normal = right.cross(up);
    const float normalLength = normal.length();
    if (!std::isfinite(normalLength) || normalLength <= 0.0f)
        return false;
    // A depth axis of comparable scale keeps the affine inverse well formed.
    normal *= std::sqrt(right.length() * up.length()) / normalLength;
    SbMatrix result = SbMatrix::identity();
    for (int axis = 0; axis < 3; ++axis) {
        result[0][axis] = right[axis];
        result[1][axis] = up[axis];
        result[2][axis] = normal[axis];
        result[3][axis] = anchor[axis];
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(result[row][axis]))
                return false;
    }
    geometryToRoot = result;
    return true;
}

SbBox3f
cadPartGeometryBounds(const PartGeometry& geometry)
{
    SbBox3f bounds;
    if (geometry.conservativeBounds)
        bounds.extendBy(*geometry.conservativeBounds);
    if (geometry.points)
        bounds.extendBy(geometry.points->bounds);
    if (geometry.wire)
        bounds.extendBy(geometry.wire->bounds);
    if (geometry.shaded)
        bounds.extendBy(geometry.shaded->bounds);
    return bounds;
}

SbBox3f
cadPartModelBounds(const PartGeometry& geometry)
{
    return geometry.displayPlane ?
        SbBox3f(geometry.displayPlane->anchor, geometry.displayPlane->anchor) :
        cadPartGeometryBounds(geometry);
}

} // namespace Obol
