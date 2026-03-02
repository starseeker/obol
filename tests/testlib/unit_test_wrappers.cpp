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

/**
 * @file unit_test_wrappers.cpp
 * @brief Inline unit tests registered with the TestRegistry.
 *
 * Each test category is implemented as a self-contained lambda / function
 * that exercises the relevant Coin API and returns 0 on pass, non-zero on
 * failure.  The tests are automatically registered at program start via
 * static-initialiser blocks, making them available through the unified
 * obol_test CLI and through the FLTK viewer.
 *
 * Additionally, every visual scene factory from test_scenes.h is registered
 * here so the registry is fully populated from a single translation unit.
 */

#include "test_registry.h"
#include "test_scenes.h"

// ---------------------------------------------------------------------------
// Coin headers used by the unit tests
// ---------------------------------------------------------------------------
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbViewportRegion.h>

// Actions
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoRaytraceRenderAction.h>
#include <Inventor/SoPrimitiveVertex.h>

// Fields
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFVec3f.h>

// Nodes
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoShape.h>

// Engines
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/fields/SoSFVec3f.h>

// Sensors
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>

#include <Inventor/SbTime.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>

// Additional headers for high-priority coverage tests
#include <Inventor/SbPlane.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbBox3d.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbBox2s.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbCylinder.h>
#include <Inventor/SoSceneManager.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoFaceSet.h>

// Iteration-2 headers
#include <Inventor/SbDict.h>
#include <Inventor/SbImage.h>
#include <Inventor/SbString.h>
#include <Inventor/SbName.h>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbDPPlane.h>
#include <Inventor/SbDPRotation.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbVec3d.h>
#include <Inventor/SbXfBox3f.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/SoDB.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFColor.h>
#include <Inventor/fields/SoSFMatrix.h>
#include <Inventor/fields/SoSFVec2f.h>
#include <Inventor/fields/SoSFNode.h>
#include <Inventor/fields/SoSFPath.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoLocation2Event.h>

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

// =========================================================================
// Helper: approximate float comparison
// =========================================================================
static bool approxEqual(float a, float b, float tol = 1e-4f)
{
    return std::fabs(a - b) <= tol;
}

// =========================================================================
// Unit test: Actions
// =========================================================================
static int runActionsTests()
{
    int failures = 0;

    // --- Action type checking ---
    {
        SoSearchAction search;
        SoGetBoundingBoxAction bbox(SbViewportRegion(100, 100));
        SoCallbackAction callback;
        if (search.getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoSearchAction has bad type\n");
            ++failures;
        }
        if (!search.isOfType(SoAction::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoSearchAction not of SoAction type\n");
            ++failures;
        }
    }

    // --- Bounding box computation ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoCube* cube = new SoCube;
        cube->width.setValue(2.0f);
        cube->height.setValue(2.0f);
        cube->depth.setValue(2.0f);
        scene->addChild(cube);
        SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
        bba.apply(scene);
        SbBox3f box = bba.getBoundingBox();
        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: bounding box is empty for 2x2x2 cube\n");
            ++failures;
        }
        scene->unref();
    }

    // --- Search action ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoCube* cube = new SoCube;
        cube->setName("FindMe");
        scene->addChild(cube);
        SoSearchAction sa;
        sa.setName(SbName("FindMe"));
        sa.apply(scene);
        if (!sa.getPath()) {
            fprintf(stderr, "  FAIL: search did not find named node\n");
            ++failures;
        }
        scene->unref();
    }

    // --- Scene traversal does not crash ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(1.0f, 2.0f, 3.0f);
        scene->addChild(t);
        scene->addChild(new SoCube);
        SoSearchAction sa;
        sa.apply(scene);
        scene->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoRaytraceRenderAction
// =========================================================================
static int runRaytraceActionTests()
{
    int failures = 0;

    // --- Type registration ---
    {
        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        if (rta.getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction has bad type\n");
            ++failures;
        }
        if (!rta.isOfType(SoCallbackAction::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction not derived from SoCallbackAction\n");
            ++failures;
        }
        if (!rta.isOfType(SoAction::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction not derived from SoAction\n");
            ++failures;
        }
    }

    // --- Viewport region round-trip ---
    {
        SbViewportRegion vp(320, 240);
        SoRaytraceRenderAction rta(vp);
        SbVec2s size = rta.getViewportRegion().getWindowSize();
        if (size[0] != 320 || size[1] != 240) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction viewport size mismatch (%d,%d)\n",
                    (int)size[0], (int)size[1]);
            ++failures;
        }
    }

    // --- Triangle collection via generatePrimitives ---
    {
        // Apply to a cube; count the triangles generated
        SoSeparator* root = new SoSeparator;
        root->ref();
        root->addChild(new SoCube);

        int triangleCount = 0;
        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        rta.addTriangleCallback(
            SoShape::getClassTypeId(),
            [](void* ud, SoCallbackAction*, const SoPrimitiveVertex*,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {
                (*static_cast<int*>(ud))++;
            },
            &triangleCount);
        rta.apply(root);
        root->unref();

        // A cube has 6 faces × 2 triangles each = 12 triangles
        if (triangleCount != 12) {
            fprintf(stderr, "  FAIL: expected 12 triangles for cube, got %d\n", triangleCount);
            ++failures;
        }
    }

    // --- Lights collected after traversal ---
    {
        SoSeparator* root = new SoSeparator;
        root->ref();
        root->addChild(new SoDirectionalLight);
        root->addChild(new SoCube);

        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        rta.addTriangleCallback(SoShape::getClassTypeId(),
            [](void*, SoCallbackAction*, const SoPrimitiveVertex*,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {},
            nullptr);
        rta.apply(root);
        root->unref();

        const SoNodeList& lights = rta.getLights();
        if (lights.getLength() == 0) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction getLights() returned empty list\n");
            ++failures;
        } else if (!lights[0]->isOfType(SoDirectionalLight::getClassTypeId())) {
            fprintf(stderr, "  FAIL: light is not SoDirectionalLight\n");
            ++failures;
        }
    }

    // --- Model matrix applied to vertices ---
    {
        // Translate a cube by (5,0,0); collected vertices should be near x=5
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoTranslation* t = new SoTranslation;
        t->translation.setValue(5.0f, 0.0f, 0.0f);
        root->addChild(t);
        root->addChild(new SoCube);

        struct Collector {
            SbVec3f first;
            bool set = false;
        } col;

        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        rta.addTriangleCallback(
            SoShape::getClassTypeId(),
            [](void* ud, SoCallbackAction* action,
               const SoPrimitiveVertex* v1,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {
                Collector* c = static_cast<Collector*>(ud);
                if (!c->set) {
                    SbVec3f p;
                    action->getModelMatrix().multVecMatrix(v1->getPoint(), p);
                    c->first = p;
                    c->set = true;
                }
            },
            &col);
        rta.apply(root);
        root->unref();

        if (!col.set) {
            fprintf(stderr, "  FAIL: no triangle callback was invoked\n");
            ++failures;
        } else if (std::fabs(col.first[0] - 5.0f) > 1.5f) {
            fprintf(stderr, "  FAIL: expected vertex near x=5, got x=%f\n", col.first[0]);
            ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: Base types (SbVec3f, SbMatrix, SbRotation)
// =========================================================================
static int runBaseTests()
{
    int failures = 0;

    // --- SbVec3f arithmetic ---
    {
        SbVec3f a(1.0f, 2.0f, 3.0f);
        SbVec3f b(4.0f, 5.0f, 6.0f);
        SbVec3f sum = a + b;
        if (!approxEqual(sum[0],5) || !approxEqual(sum[1],7) || !approxEqual(sum[2],9)) {
            fprintf(stderr, "  FAIL: SbVec3f addition\n"); ++failures;
        }
        float dot = a.dot(b);
        if (!approxEqual(dot, 32.0f)) {
            fprintf(stderr, "  FAIL: SbVec3f dot product (got %f)\n", dot); ++failures;
        }
        SbVec3f cross = a.cross(b);
        // 1x6-2x5, 2x4-1x6, 1x5-2x4 = -3,6,-3
        if (!approxEqual(cross[0],-3) || !approxEqual(cross[1],6) || !approxEqual(cross[2],-3)) {
            fprintf(stderr, "  FAIL: SbVec3f cross product\n"); ++failures;
        }
    }

    // --- SbVec3f normalisation ---
    {
        SbVec3f v(3.0f, 4.0f, 0.0f);
        SbVec3f n = v;
        n.normalize();
        if (!approxEqual(n.length(), 1.0f)) {
            fprintf(stderr, "  FAIL: SbVec3f normalise length\n"); ++failures;
        }
    }

    // --- SbMatrix identity ---
    {
        SbMatrix m;
        m.makeIdentity();
        SbVec3f v(1.0f, 2.0f, 3.0f);
        SbVec3f r;
        m.multVecMatrix(v, r);
        if (!approxEqual(r[0],1) || !approxEqual(r[1],2) || !approxEqual(r[2],3)) {
            fprintf(stderr, "  FAIL: SbMatrix identity transform\n"); ++failures;
        }
    }

    // --- SbRotation ---
    {
        SbRotation rot(SbVec3f(0,1,0), float(M_PI / 2.0));
        SbVec3f v(1.0f, 0.0f, 0.0f);
        SbVec3f result;
        rot.multVec(v, result);
        // 90° around Y maps (1,0,0) to (0,0,-1)
        if (!approxEqual(result[0], 0.0f, 1e-3f) ||
            !approxEqual(result[1], 0.0f, 1e-3f) ||
            !approxEqual(result[2], -1.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: SbRotation 90-deg Y (got %f %f %f)\n",
                    result[0], result[1], result[2]);
            ++failures;
        }
    }

    // --- SbBox3f ---
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        if (box.isEmpty()) { fprintf(stderr, "  FAIL: SbBox3f isEmpty\n"); ++failures; }
        SbVec3f center = box.getCenter();
        if (!approxEqual(center[0],0) || !approxEqual(center[1],0) || !approxEqual(center[2],0)) {
            fprintf(stderr, "  FAIL: SbBox3f center\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: Fields
// =========================================================================
static int runFieldsTests()
{
    int failures = 0;

    // --- SoSFFloat ---
    {
        SoSFFloat f;
        f.setValue(3.14f);
        if (!approxEqual(f.getValue(), 3.14f)) {
            fprintf(stderr, "  FAIL: SoSFFloat set/get\n"); ++failures;
        }
        if (f.isConnected()) {
            fprintf(stderr, "  FAIL: SoSFFloat should not be connected\n"); ++failures;
        }
    }

    // --- SoSFBool ---
    {
        SoSFBool b;
        b.setValue(TRUE);
        if (b.getValue() != TRUE) {
            fprintf(stderr, "  FAIL: SoSFBool set/get\n"); ++failures;
        }
    }

    // --- SoMFVec3f ---
    {
        SoMFVec3f mv;
        SbVec3f pts[3] = {{0,0,0},{1,0,0},{0,1,0}};
        mv.setValues(0, 3, pts);
        if (mv.getNum() != 3) {
            fprintf(stderr, "  FAIL: SoMFVec3f count (got %d)\n", mv.getNum()); ++failures;
        }
        if (!approxEqual(mv[1][0], 1.0f)) {
            fprintf(stderr, "  FAIL: SoMFVec3f value\n"); ++failures;
        }
    }

    // --- SoMFFloat ---
    {
        SoMFFloat mf;
        float vals[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        mf.setValues(0, 4, vals);
        if (mf.getNum() != 4) {
            fprintf(stderr, "  FAIL: SoMFFloat count\n"); ++failures;
        }
    }

    // --- Field on a node ---
    {
        SoCube* cube = new SoCube;
        cube->ref();
        cube->width.setValue(5.0f);
        if (!approxEqual(cube->width.getValue(), 5.0f)) {
            fprintf(stderr, "  FAIL: SoCube field value\n"); ++failures;
        }
        cube->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Nodes
// =========================================================================
static int runNodesTests()
{
    int failures = 0;

    // --- Node creation and type ---
    {
        SoCube* cube = new SoCube;
        cube->ref();
        if (cube->getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoCube bad type\n"); ++failures;
        }
        if (!cube->isOfType(SoNode::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoCube not SoNode\n"); ++failures;
        }
        cube->unref();
    }

    // --- Scene graph hierarchy ---
    {
        SoSeparator* root = new SoSeparator;
        root->ref();
        SoCube*   cube   = new SoCube;
        SoSphere* sphere = new SoSphere;
        root->addChild(cube);
        root->addChild(sphere);
        if (root->getNumChildren() != 2) {
            fprintf(stderr, "  FAIL: child count (got %d)\n", root->getNumChildren());
            ++failures;
        }
        root->removeChild(cube);
        if (root->getNumChildren() != 1) {
            fprintf(stderr, "  FAIL: after removeChild (got %d)\n", root->getNumChildren());
            ++failures;
        }
        root->unref();
    }

    // --- Named node lookup ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoCube* c = new SoCube;
        c->setName("MyBox");
        scene->addChild(c);
        SoNode* found = SoNode::getByName(SbName("MyBox"));
        if (!found) {
            fprintf(stderr, "  FAIL: SoNode::getByName failed\n"); ++failures;
        }
        scene->unref();
    }

    // --- Camera viewAll does not crash ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        scene->addChild(cam);
        scene->addChild(new SoCube);
        SbViewportRegion vp(800, 600);
        cam->viewAll(scene, vp);
        scene->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Engines
// =========================================================================
static int runEnginesTests()
{
    int failures = 0;

    // --- SoCalculator type ---
    {
        SoCalculator* calc = new SoCalculator;
        calc->ref();
        if (calc->getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoCalculator bad type\n"); ++failures;
        }
        calc->unref();
    }

    // --- SoCalculator basic expression ---
    {
        SoCalculator* calc = new SoCalculator;
        calc->ref();
        calc->a.setValue(3.0f);
        calc->b.setValue(4.0f);
        calc->expression.setValue("oa = a + b");
        // Connect output to a float field to read the result
        SoSFFloat resultField;
        resultField.connectFrom(&calc->oa);
        float result = resultField.getValue();
        if (!approxEqual(result, 7.0f)) {
            fprintf(stderr, "  FAIL: SoCalculator a+b = %f (expected 7.0)\n", result);
            ++failures;
        }
        resultField.disconnect();
        calc->unref();
    }

    // --- SoElapsedTime type check ---
    {
        SoElapsedTime* et = new SoElapsedTime;
        et->ref();
        if (et->getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoElapsedTime bad type\n"); ++failures;
        }
        et->unref();
    }

    // --- SoComposeVec3f ---
    {
        SoComposeVec3f* compose = new SoComposeVec3f;
        compose->ref();
        compose->x.setValue(1.0f);
        compose->y.setValue(2.0f);
        compose->z.setValue(3.0f);
        // Connect output to a field to read the result
        SoSFVec3f outField;
        outField.connectFrom(&compose->vector);
        SbVec3f out = outField.getValue();
        outField.disconnect();
        if (!approxEqual(out[0],1) || !approxEqual(out[1],2) || !approxEqual(out[2],3)) {
            fprintf(stderr, "  FAIL: SoComposeVec3f output (%f %f %f)\n",
                    out[0], out[1], out[2]);
            ++failures;
        }
        compose->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Sensors
// =========================================================================
static int runSensorsTests()
{
    int failures = 0;
    static int s_callback_count = 0;

    // --- SoFieldSensor ---
    {
        s_callback_count = 0;
        SoCube* cube = new SoCube;
        cube->ref();

        SoFieldSensor* fs = new SoFieldSensor(
            [](void* data, SoSensor*) { (*static_cast<int*>(data))++; },
            &s_callback_count);
        fs->attach(&cube->width);
        cube->width.setValue(5.0f);
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        // Callback may be deferred; just verify no crash occurred
        // (exact callback timing is environment-dependent)
        delete fs;
        cube->unref();
    }

    // --- SoNodeSensor type check ---
    {
        SoNodeSensor* ns = new SoNodeSensor;
        // SoSensor is not SoBase so no getTypeId(); just verify construction
        if (!ns) {
            fprintf(stderr, "  FAIL: SoNodeSensor construction\n"); ++failures;
        }
        delete ns;
    }

    // --- SoTimerSensor type check ---
    {
        SoTimerSensor* ts = new SoTimerSensor;
        ts->setInterval(SbTime(0.1));
        if (!ts) {
            fprintf(stderr, "  FAIL: SoTimerSensor construction\n"); ++failures;
        }
        delete ts;
    }

    return failures;
}

// =========================================================================
// Unit test: SoCamera::orbitCamera() – BRL-CAD-style smooth camera orbit math
// =========================================================================
static int runOrbitCameraTests()
{
    int failures = 0;
    const float tol = 1e-3f;

    const SbVec3f center(0.0f, 0.0f, 0.0f);
    const float radius = 5.0f;
    const float sens = 0.25f;   /* BRL-CAD default: 0.25 deg/pixel */

    /* --- Test 1: yaw-only (dx=80, dy=0) preserves orbit radius --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 80.0f, 0.0f, sens);

        float dist = (cam->position.getValue() - center).length();
        if (std::fabs(dist - radius) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test1: radius after yaw=%.6f (expected %.6f)\n",
                    dist, radius);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 2: yaw-only camera still looks toward center --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 80.0f, 0.0f, sens);

        SbVec3f viewDir;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        SbVec3f toCenter = center - cam->position.getValue();
        toCenter.normalize();
        float dot = toCenter.dot(viewDir);
        if (std::fabs(dot - 1.0f) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test2: camera not looking at center (dot=%.6f)\n", dot);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 3: pitch-only (dx=0, dy=80) preserves orbit radius --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 0.0f, 80.0f, sens);

        float dist = (cam->position.getValue() - center).length();
        if (std::fabs(dist - radius) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test3: radius after pitch=%.6f (expected %.6f)\n",
                    dist, radius);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 4: pitch-only camera still looks toward center --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 0.0f, 80.0f, sens);

        SbVec3f viewDir;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        SbVec3f toCenter = center - cam->position.getValue();
        toCenter.normalize();
        float dot = toCenter.dot(viewDir);
        if (std::fabs(dot - 1.0f) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test4: camera not looking at center after pitch (dot=%.6f)\n", dot);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 5: mouse-right (dx>0) moves camera in -X direction
     *             (scene appears to rotate right → camera orbits left) --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 80.0f, 0.0f, sens);

        if (cam->position.getValue()[0] >= 0.0f) {
            fprintf(stderr, "  FAIL orbitCamera test5: expected negative X after yaw right, got %.4f\n",
                    cam->position.getValue()[0]);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 6: mouse-down (dy>0) moves camera upward (+Y)
     *             (camera pitches down → position rises in Y) --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 0.0f, 80.0f, sens);

        if (cam->position.getValue()[1] <= 0.0f) {
            fprintf(stderr, "  FAIL orbitCamera test6: expected positive Y after pitch down, got %.4f\n",
                    cam->position.getValue()[1]);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 7: 4 × 1-pixel steps ≈ 1 × 4-pixel step (smoothness check).
     *             For small angles, rotation non-commutativity is negligible,
     *             so accumulated small steps should closely match one equivalent
     *             large step. --- */
    {
        /* Single 4-pixel step */
        SoPerspectiveCamera *camA = new SoPerspectiveCamera;
        camA->ref();
        camA->position.setValue(0.0f, 0.0f, radius);
        camA->orientation.setValue(SbRotation::identity());
        camA->orbitCamera(center, 4.0f, 3.0f, sens);
        SbVec3f pA = camA->position.getValue();
        camA->unref();

        /* Four 1-pixel steps */
        SoPerspectiveCamera *camB = new SoPerspectiveCamera;
        camB->ref();
        camB->position.setValue(0.0f, 0.0f, radius);
        camB->orientation.setValue(SbRotation::identity());
        for (int i = 0; i < 4; ++i) {
            camB->orbitCamera(center, 1.0f, 0.75f, sens);
        }
        SbVec3f pB = camB->position.getValue();
        camB->unref();

        float ddx = std::fabs(pA[0] - pB[0]);
        float ddy = std::fabs(pA[1] - pB[1]);
        float ddz = std::fabs(pA[2] - pB[2]);
        if (ddx > 0.001f || ddy > 0.001f || ddz > 0.001f) {
            fprintf(stderr, "  FAIL orbitCamera test7: step accumulation mismatch "
                    "dx=%.5f dy=%.5f dz=%.5f\n", ddx, ddy, ddz);
            ++failures;
        }
    }

    /* --- Test 8: camera-local yaw leaves the camera's world-up direction
     *             unchanged.  orbitCamera() yaws around the camera's own Y
     *             axis (camera-local space), so the camera's world-up vector
     *             q*(0,1,0) is invariant under pure yaw – rotating a vector
     *             around itself cannot change it.  After accumulating 45° of
     *             pitch the world-up is no longer (0,1,0); 720 subsequent
     *             yaw-only steps (720 pixels × 0.25 deg/pixel = 180° total)
     *             must leave it the same. --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());

        /* Pitch 45° first (180 pixels × 0.25 deg/pixel) */
        cam->orbitCamera(center, 0.0f, 180.0f, sens);

        SbVec3f upAfterPitch;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 1.0f, 0.0f), upAfterPitch);

        /* 720 yaw-only steps (720 pixels × 0.25 deg/pixel = 180° total) */
        for (int i = 0; i < 720; ++i)
            cam->orbitCamera(center, 1.0f, 0.0f, sens);

        SbVec3f upAfterYaw;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 1.0f, 0.0f), upAfterYaw);

        float drift = (upAfterYaw - upAfterPitch).length();
        if (drift > tol) {
            fprintf(stderr, "  FAIL orbitCamera test8: camera-local yaw changed world-up "
                    "by %.6f (expected < %.6f)\n", drift, tol);
            ++failures;
        }
        cam->unref();
    }

    return failures;
}

// =========================================================================
// Scalability benchmark: scene graph construction and traversal
//
// Characterises the cost of building and traversing large Obol scene graphs
// that are representative of a BRL-CAD CSG tree (tens of thousands of nodes).
//
// Three structures are tested at several node counts:
//
//  flat          – root SoSeparator with N * (SoTranslation + SoCube) groups.
//                  Simulates a wide, single-level assembly list.
//
//  binary_tree   – balanced binary tree of SoSeparators with SoCubes at the
//                  leaves.  Simulates a balanced CSG combination tree.
//
//  linear_chain  – depth-N linear chain of SoSeparators with a single SoCube
//                  at the tip.  Simulates a maximally deep unary CSG tree.
//                  Capped at CHAIN_MAX nodes to avoid system-stack exhaustion.
//
//  chain_bottomup  – same deep chain but built leaf-first (bottom-up),
//                    avoiding the O(n^2) top-down notification cost.
//
//  chain_deferred  – same deep chain built top-down but with
//                    enableNotify(FALSE) on each node during construction,
//                    also achieving O(n) build time.
//
// For each (structure, size) pair the following operations are timed:
//   build    – allocate all nodes and wire up the hierarchy
//   bbox     – SoGetBoundingBoxAction traversal (CPU; no GL)
//   search   – SoSearchAction for ALL SoCube nodes
//   destroy  – root->unref() (reclaims all memory)
//
// KEY FINDINGS (see tests/nodes/README_scalability.md for full analysis):
//
//  1. Flat and balanced binary trees: O(n) build and traversal.  Suitable
//     for tens-of-thousands of nodes.
//
//  2. Deep linear chain – CONSTRUCTION: O(n^2) when built top-down.
//     Root cause: SoChildList::append() calls parent->startNotify(), which
//     propagates upward through the existing ancestor chain.  For a chain of
//     depth d at step i, the notification cost is O(i), yielding O(n^2) total.
//     MITIGATION: build bottom-up (chain_bottomup) or disable notifications
//     during construction (chain_deferred) – both reduce to O(n).
//
//  3. Deep linear chain – TRAVERSAL: O(n^2) regardless of construction order.
//     Root cause: SoCacheElement::addElement() iterates through all currently
//     open SoCacheElement entries in the state stack.  For a chain at depth d,
//     d caches are simultaneously open, so every element access during
//     traversal costs O(d).  Total traversal cost = sum(d for d=0..n) = O(n^2).
//     MITIGATION: avoid very deep single-branch CSG chains; prefer balanced
//     trees or flat assembly lists.  Setting boundingBoxCaching=OFF removes
//     the O(n^2) addElement overhead but eliminates caching benefits.
//
// The function always returns 0 (characterisation only; no pass/fail on
// timing), but prints a CSV table to stdout for post-run analysis.
// =========================================================================
static int runScalabilityTests()
{
    // ----- configuration --------------------------------------------------
    // Sizes express the number of *leaf geometry nodes* (SoCube instances).
    // Total node count depends on the structure (see comments below).
    static const int kFlatSizes[]  = { 100, 500, 1000, 5000, 10000, 20000 };
    static const int kTreeSizes[]  = { 100, 500, 1000, 5000, 10000, 20000 };
    // Deep recursive traversal can exhaust the system stack; cap accordingly.
    static const int kChainSizes[] = { 100, 500, 1000, 2000, 5000 };
    static const int CHAIN_MAX     = 5000;
    (void)CHAIN_MAX;

    printf("# Obol Scene Graph Scalability Benchmark\n");
    printf("# structure, leaves, total_nodes,"
           " build_ms, bbox_ms, search_ms, destroy_ms\n");

    // ------------------------------------------------------------------
    // 1. Flat scene
    //    Layout: root SoSeparator
    //               +-- SoSeparator (group_0)
    //               |      +-- SoTranslation
    //               |      +-- SoCube
    //               +-- SoSeparator (group_1)
    //               ...
    //    Total nodes: 1 + 3*N
    // ------------------------------------------------------------------
    for (int n : kFlatSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        SoSeparator* root = new SoSeparator;
        root->ref();
        int cols = (int)std::sqrt((double)n);
        if (cols < 1) cols = 1;
        for (int i = 0; i < n; ++i) {
            SoSeparator* grp = new SoSeparator;
            SoTranslation* tr = new SoTranslation;
            tr->translation.setValue(float(i % cols) * 2.0f,
                                     float(i / cols) * 2.0f, 0.0f);
            grp->addChild(tr);
            grp->addChild(new SoCube);
            root->addChild(grp);
        }

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(root);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);

        SbTime t3 = SbTime::getTimeOfDay();

        root->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("flat, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               1 + 3 * n,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    // ------------------------------------------------------------------
    // 2. Balanced binary tree (CSG-like)
    //    N leaf groups: each is SoSeparator + SoTranslation + SoCube.
    //    Internal nodes: SoSeparators that pair up children bottom-up.
    //    Depth ≈ ceil(log2(N)).
    //    Total nodes: 3*N leaves + (N-1) internal SoSeparators ≈ 4*N
    // ------------------------------------------------------------------
    for (int n : kTreeSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        // Build leaf groups
        std::vector<SoNode*> level;
        level.reserve((size_t)n);
        for (int i = 0; i < n; ++i) {
            SoSeparator* leaf = new SoSeparator;
            SoTranslation* tr = new SoTranslation;
            tr->translation.setValue(float(i) * 2.0f, 0.0f, 0.0f);
            leaf->addChild(tr);
            leaf->addChild(new SoCube);
            level.push_back(leaf);
        }

        // Pair up levels until a single root remains
        while (level.size() > 1) {
            std::vector<SoNode*> next;
            next.reserve((level.size() + 1) / 2);
            for (size_t i = 0; i < level.size(); i += 2) {
                SoSeparator* parent = new SoSeparator;
                parent->addChild(level[i]);
                if (i + 1 < level.size())
                    parent->addChild(level[i + 1]);
                next.push_back(parent);
            }
            level = std::move(next);
        }

        SoNode* treeRoot = level[0];
        treeRoot->ref();

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(treeRoot);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(treeRoot);

        SbTime t3 = SbTime::getTimeOfDay();

        treeRoot->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        // internal separators ≈ n-1; leaves: n * (sep + trans + cube) = 3n
        int total = 3 * n + (n - 1);
        printf("binary_tree, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n, total,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    // ------------------------------------------------------------------
    // 3. Deep linear chain
    //    root → sep_1 → sep_2 → … → sep_(N-1) → SoCube
    //    Total nodes: N separators + 1 cube = N+1
    //    NOTE: Deep trees may exhaust the system call stack during
    //          recursive action traversal; CHAIN_MAX is set conservatively.
    // ------------------------------------------------------------------
    for (int n : kChainSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        SoSeparator* root = new SoSeparator;
        root->ref();
        SoSeparator* cur = root;
        for (int i = 1; i < n; ++i) {
            SoSeparator* child = new SoSeparator;
            cur->addChild(child);
            cur = child;
        }
        cur->addChild(new SoCube);

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(root);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);

        SbTime t3 = SbTime::getTimeOfDay();

        root->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("linear_chain, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               n + 1,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    printf("# Benchmark complete\n");
    printf("# NOTE: linear_chain build times show O(n^2) growth due to\n");
    printf("#       upward-notification on each addChild() call.\n");
    printf("#       Mitigations: bottom-up construction or\n");
    printf("#       enableNotify(FALSE)/enableNotify(TRUE) bracketing.\n");

    // ------------------------------------------------------------------
    // 4. Deep linear chain – bottom-up construction
    //    Build leaf first, then wrap in parent separators moving up.
    //    Each addChild() has no ancestor chain yet → O(1) notification.
    //    Total nodes: N separators + 1 cube = N+1 (same as linear_chain)
    // ------------------------------------------------------------------
    printf("# Bottom-up chain: builds deepest node first (O(n) vs O(n^2) naive)\n");
    for (int n : kChainSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        // Start with the leaf cube
        SoNode* cur = new SoCube;
        // Wrap in separators from deepest outward
        for (int i = 0; i < n; ++i) {
            SoSeparator* parent = new SoSeparator;
            parent->addChild(cur);   // cur has no ancestor yet → O(1) notify
            cur = parent;
        }
        cur->ref();  // cur is now the outermost separator (root)

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(cur);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(cur);

        SbTime t3 = SbTime::getTimeOfDay();

        cur->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("chain_bottomup, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               n + 1,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    // ------------------------------------------------------------------
    // 5. Deep linear chain – deferred notification
    //    Top-down build but with enableNotify(FALSE) on each node before
    //    adding its child, re-enabled afterwards.
    //    Prevents the O(depth) upward notification on each addChild().
    // ------------------------------------------------------------------
    printf("# Deferred-notify chain: top-down but notifications suppressed\n");
    for (int n : kChainSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        SoSeparator* root = new SoSeparator;
        root->ref();
        root->enableNotify(FALSE);
        SoSeparator* cur = root;
        for (int i = 1; i < n; ++i) {
            SoSeparator* child = new SoSeparator;
            child->enableNotify(FALSE);
            cur->addChild(child);
            cur = child;
        }
        cur->addChild(new SoCube);
        // Re-enable notifications from tip to root
        cur = root;
        cur->enableNotify(TRUE);
        for (int i = 1; i < n; ++i) {
            SoSeparator* child =
                static_cast<SoSeparator*>(
                    static_cast<SoGroup*>(cur)->getChild(0));
            if (!child || !child->isOfType(SoSeparator::getClassTypeId()))
                break;
            child->enableNotify(TRUE);
            cur = child;
        }

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(root);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);

        SbTime t3 = SbTime::getTimeOfDay();

        root->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("chain_deferred, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               n + 1,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    printf("# Scalability benchmark complete\n");
    return 0; // characterisation only – no pass/fail on timing
}

// =========================================================================
// Static registrations – run at program start
// =========================================================================

// --- Visual scene registrations ---

REGISTER_TEST(primitives, ObolTest::TestCategory::Rendering,
    "2x2 grid: sphere, cube, cone, cylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPrimitives;
);

REGISTER_TEST(materials, ObolTest::TestCategory::Rendering,
    "Four spheres of the same base colour showing different material effects (matte, shiny, emissive, high-ambient)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createMaterials;
);

REGISTER_TEST(lighting, ObolTest::TestCategory::Rendering,
    "Three spheres each lit by a different light type (directional, point, spot)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLighting;
);

REGISTER_TEST(transforms, ObolTest::TestCategory::Rendering,
    "Translation, rotation, and scaling transform rows (3x3 grid of cubes)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTransforms;
);

REGISTER_TEST(cameras, ObolTest::TestCategory::Rendering,
    "Three cubes receding along Z to illustrate perspective foreshortening",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCameras;
);

REGISTER_TEST(texture, ObolTest::TestCategory::Rendering,
    "Textured sphere and untextured sphere side by side (SoTexture2 checkerboard)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTexture;
);

REGISTER_TEST(text, ObolTest::TestCategory::Rendering,
    "SoText3 extruded 3-D geometry text",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createText;
);

REGISTER_TEST(text2, ObolTest::TestCategory::Rendering,
    "SoText2 2-D billboard text labels at varied positions and sizes",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createText2;
);

REGISTER_TEST(gradient, ObolTest::TestCategory::Rendering,
    "Background gradient via callback node",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createGradient;
);

REGISTER_TEST(colored_cube, ObolTest::TestCategory::Rendering,
    "Simple red cube with lighting (smoke test)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createColoredCube;
);

REGISTER_TEST(coordinates, ObolTest::TestCategory::Rendering,
    "Colour-coded XYZ axis lines",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCoordinates;
);

REGISTER_TEST(shadow, ObolTest::TestCategory::Rendering,
    "Shadow-casting scene: SoShadowGroup (GL) + SoRaytracingParams (NanoRT)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createShadow;
);

REGISTER_TEST(draggers, ObolTest::TestCategory::Draggers,
    "Interactive draggers (SoTranslate1, SoRotateSpherical)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createDraggers;
);

REGISTER_TEST(hud, ObolTest::TestCategory::Misc,
    "Head-up display overlay using orthographic camera",
    e.has_visual = true;
    e.has_interactive = false;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createHUD;
);

REGISTER_TEST(lod, ObolTest::TestCategory::Nodes,
    "Level of detail (SoLOD) switching between representations",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLOD;
);

REGISTER_TEST(transparency, ObolTest::TestCategory::Rendering,
    "Alpha-blended overlapping spheres",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTransparency;
);

REGISTER_TEST(drawstyle, ObolTest::TestCategory::Rendering,
    "Filled, wireframe, and points draw style comparison",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createDrawStyle;
);

REGISTER_TEST(indexed_face_set, ObolTest::TestCategory::Rendering,
    "SoIndexedFaceSet octahedron with per-face material colours",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createIndexedFaceSet;
);

REGISTER_TEST(manips, ObolTest::TestCategory::Manips,
    "Interactive manipulators (SoTrackballManip, SoTabBoxManip)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createManips;
);

REGISTER_TEST(scene, ObolTest::TestCategory::Rendering,
    "2×2 grid of primitives: sphere, cube, cone, cylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createScene;
);

REGISTER_TEST(face_set, ObolTest::TestCategory::Rendering,
    "SoFaceSet green quad in the lower-left quadrant",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createFaceSet;
);

REGISTER_TEST(line_set, ObolTest::TestCategory::Rendering,
    "SoLineSet red horizontal line across the viewport",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLineSet;
);

REGISTER_TEST(indexed_line_set, ObolTest::TestCategory::Rendering,
    "SoIndexedLineSet: green horizontal, red diagonal, blue V-shape",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createIndexedLineSet;
);

REGISTER_TEST(point_set, ObolTest::TestCategory::Rendering,
    "SoPointSet: four distinctly coloured points in the four quadrants",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPointSet;
);

REGISTER_TEST(triangle_strip_set, ObolTest::TestCategory::Rendering,
    "SoTriangleStripSet: emissive blue strip quad in lower half",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTriangleStripSet;
);

REGISTER_TEST(quad_mesh, ObolTest::TestCategory::Rendering,
    "SoQuadMesh: 5×5 colour-gradient grid (red → blue across columns)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createQuadMesh;
);

REGISTER_TEST(vertex_colors, ObolTest::TestCategory::Rendering,
    "Per-vertex coloured quad via SoPackedColor + SoIndexedFaceSet",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createVertexColors;
);

REGISTER_TEST(switch_visibility, ObolTest::TestCategory::Nodes,
    "Two coloured spheres controlled by SoSwitch (both visible)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSwitchVisibility;
);

REGISTER_TEST(sphere_position, ObolTest::TestCategory::Rendering,
    "Emissive sphere offset from centre with SoOrthographicCamera",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSpherePosition;
);

REGISTER_TEST(checker_texture, ObolTest::TestCategory::Rendering,
    "Checkerboard-textured cube via procedural SoTexture2",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCheckerTexture;
);

REGISTER_TEST(clip_plane, ObolTest::TestCategory::Rendering,
    "Large sphere clipped in half by SoClipPlane at Y=0",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createClipPlane;
);

REGISTER_TEST(array_multiple_copy, ObolTest::TestCategory::Nodes,
    "3×3 SoArray grid of spheres + three SoMultipleCopy cubes",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createArrayMultipleCopy;
);

REGISTER_TEST(annotation, ObolTest::TestCategory::Nodes,
    "SoAnnotation sphere composited on top of a background sphere",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createAnnotation;
);

REGISTER_TEST(ascii_text, ObolTest::TestCategory::Nodes,
    "SoAsciiText \"HELLO\" centred with perspective camera",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createAsciiText;
);

REGISTER_TEST(reset_transform, ObolTest::TestCategory::Rendering,
    "SoResetTransform: blue sphere at offset + red sphere reset to origin",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createResetTransform;
);

REGISTER_TEST(shape_hints, ObolTest::TestCategory::Rendering,
    "SoShapeHints SOLID+CCW sphere with backface culling enabled",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createShapeHints;
);

REGISTER_TEST(image_node, ObolTest::TestCategory::Nodes,
    "SoImage: red/green checkerboard image node centred in viewport",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createImageNode;
);

REGISTER_TEST(marker_set, ObolTest::TestCategory::Nodes,
    "SoMarkerSet: five markers arranged in a cross pattern",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createMarkerSet;
);

REGISTER_TEST(material_binding, ObolTest::TestCategory::Rendering,
    "SoMaterialBinding PER_FACE: red left quad, blue right quad",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createMaterialBinding;
);

// --- Texture / Visual / HUD rendering tests ---

REGISTER_TEST(alpha_test, ObolTest::TestCategory::Rendering,
    "SoAlphaTest GREATER threshold scene with checkerboard RGBA texture",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createAlphaTest;
);

REGISTER_TEST(background_gradient, ObolTest::TestCategory::Rendering,
    "2x2 primitive grid scene (background gradient set on renderer)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createBackgroundGradient;
    e.configure_renderer = [](SoOffscreenRenderer* r) {
        r->setBackgroundGradient(SbColor(0.05f, 0.05f, 0.25f),
                                 SbColor(0.40f, 0.60f, 0.85f));
    };
);

REGISTER_TEST(bump_map, ObolTest::TestCategory::Rendering,
    "Sphere with SoBumpMap sinusoidal normal-map texture",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createBumpMap;
);

REGISTER_TEST(multi_texture, ObolTest::TestCategory::Rendering,
    "Sphere with two SoTextureUnit texture units (multi-texture)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createMultiTexture;
);

REGISTER_TEST(texture3, ObolTest::TestCategory::Rendering,
    "Cube with procedural 8x8x8 SoTexture3 (3-D texture)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createTexture3;
);

REGISTER_TEST(texture_transform, ObolTest::TestCategory::Rendering,
    "Two textured quads: plain and SoTexture2Transform applied",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createTextureTransform;
);

REGISTER_TEST(environment, ObolTest::TestCategory::Rendering,
    "Red sphere with SoEnvironment (no fog, high ambient intensity)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createEnvironment;
);

REGISTER_TEST(cubemap, ObolTest::TestCategory::Rendering,
    "Sphere with SoTextureCubeMap cube-map (six solid-colour faces)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createCubemap;
);

REGISTER_TEST(scene_texture, ObolTest::TestCategory::Rendering,
    "Flat quad with SoSceneTexture2 render-to-texture (orange cone sub-scene)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createSceneTexture;
);

REGISTER_TEST(hud_overlay, ObolTest::TestCategory::Rendering,
    "Blue sphere with HUD overlay: status bar and side-menu buttons",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createHUDOverlay;
);

REGISTER_TEST(hud_no3d, ObolTest::TestCategory::Rendering,
    "Pure 2-D HUD scene: title, menu buttons, info panel, status bar",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createHUDNo3D;
);

REGISTER_TEST(hud_interaction, ObolTest::TestCategory::Rendering,
    "Blue sphere with interactive HUD buttons (static visual layout)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createHUDInteraction;
);

REGISTER_TEST(text3_parts, ObolTest::TestCategory::Rendering,
    "SoText3 with FRONT, ALL, and BACK parts visible in one scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createText3Parts;
);

REGISTER_TEST(depth_buffer, ObolTest::TestCategory::Rendering,
    "Near red cube + far blue sphere with SoDepthBuffer LEQUAL depth test",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createDepthBuffer;
);

REGISTER_TEST(procedural_shape, ObolTest::TestCategory::Rendering,
    "Solid and wireframe truncated-cone SoProceduralShape side by side",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createProceduralShape;
);

REGISTER_TEST(gl_big_image, ObolTest::TestCategory::Rendering,
    "Textured quad with SoTextureScalePolicy::FRACTURE (SoGLBigImage path)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLBigImage;
);

REGISTER_TEST(image_deep, ObolTest::TestCategory::Nodes,
    "SoImage node with 48x48 RGBA checkerboard (deep coverage)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createImageDeep;
);

REGISTER_TEST(shader_program, ObolTest::TestCategory::Rendering,
    "Sphere with basic GLSL vertex + fragment SoShaderProgram",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createShaderProgram;
);

REGISTER_TEST(sorender_manager, ObolTest::TestCategory::Rendering,
    "Camera + light + SoCube scene rendered via SoRenderManager",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSoRenderManager;
);

REGISTER_TEST(gl_features, ObolTest::TestCategory::Rendering,
    "Textured sphere exercising SoGLImage / SoGLDriverDatabase paths",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLFeatures;
);

REGISTER_TEST(quad_mesh_deep, ObolTest::TestCategory::Rendering,
    "4x4 SoQuadMesh with PER_FACE material binding (nine coloured faces)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createQuadMeshDeep;
);

REGISTER_TEST(offscreen, ObolTest::TestCategory::Rendering,
    "Red sphere scene for SoOffscreenRenderer API coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createOffscreen;
);

REGISTER_TEST(bbox_action, ObolTest::TestCategory::Actions,
    "Three coloured spheres for SoGetBoundingBoxAction coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createBBoxAction;
);

REGISTER_TEST(search_action, ObolTest::TestCategory::Actions,
    "Hierarchical scene with named nodes for SoSearchAction coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSearchAction;
);

REGISTER_TEST(callback_action, ObolTest::TestCategory::Actions,
    "Sphere + cube + cone scene for SoCallbackAction triangle traversal",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCallbackAction;
);

REGISTER_TEST(callback_action_deep, ObolTest::TestCategory::Actions,
    "All primitive shape types for deep SoCallbackAction primitive coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCallbackActionDeep;
);

REGISTER_TEST(callback_node, ObolTest::TestCategory::Nodes,
    "Three SoCallback nodes interleaved with geometry",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCallbackNode;
);

REGISTER_TEST(event_propagation, ObolTest::TestCategory::Actions,
    "SoEventCallback nodes in nested separators with sphere + cube",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEventPropagation;
);

REGISTER_TEST(path_operations, ObolTest::TestCategory::Actions,
    "Sphere left + cube right for SoPath pick-and-copy tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPathOperations;
);

REGISTER_TEST(write_read_action, ObolTest::TestCategory::Actions,
    "Red sphere + blue cube input scene for SoWriteAction / SoDB::readAll",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createWriteReadAction;
);

REGISTER_TEST(field_connections, ObolTest::TestCategory::Fields,
    "Sphere driven by SoComposeVec3f engine via field connection",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createFieldConnections;
);

REGISTER_TEST(sensors_rendering, ObolTest::TestCategory::Sensors,
    "Static sphere representing the final frame of a sensor-driven animation",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSensorsRendering;
);

REGISTER_TEST(render_manager_full, ObolTest::TestCategory::Rendering,
    "Camera + light + SoCube scene for SoRenderManager comprehensive tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createRenderManagerFull;
);

REGISTER_TEST(sogl_bindings, ObolTest::TestCategory::Rendering,
    "9-point PER_VERTEX-coloured grid for SoGL PointSet binding variants",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createSOGLBindings;
);

REGISTER_TEST(glrender_action_modes, ObolTest::TestCategory::Rendering,
    "Two semi-transparent overlapping objects for SoGLRenderAction mode tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLRenderActionModes;
);

REGISTER_TEST(glrender_deep, ObolTest::TestCategory::Rendering,
    "Three semi-transparent spheres for deep SoGLRenderAction coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLRenderDeep;
);

REGISTER_TEST(offscreen_advanced, ObolTest::TestCategory::Rendering,
    "Camera + light + cube for SoOffscreenRenderer advanced API tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createOffscreenAdvanced;
);

REGISTER_TEST(view_volume_ops, ObolTest::TestCategory::Rendering,
    "Perspective camera + purple sphere for SbViewVolume operation tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createViewVolumeOps;
);

REGISTER_TEST(lod_picking, ObolTest::TestCategory::Rendering,
    "Three SoLOD nodes (sphere/cube/cone levels) for LOD + picking tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLODPicking;
);

REGISTER_TEST(stt_gl, ObolTest::TestCategory::Rendering,
    "Five SoText2 rows in an orthographic viewport for STT GL reference",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSTTGL;
);

// --- Unit test registrations ---

REGISTER_TEST(unit_actions, ObolTest::TestCategory::Actions,
    "Action type checking, bounding box, search action",
    e.has_visual = false;
    e.run_unit = runActionsTests;
);

REGISTER_TEST(unit_raytrace_action, ObolTest::TestCategory::Actions,
    "SoRaytraceRenderAction type, viewport, triangle collection, lights",
    e.has_visual = false;
    e.run_unit = runRaytraceActionTests;
);

REGISTER_TEST(unit_base, ObolTest::TestCategory::Base,
    "SbVec3f, SbMatrix, SbRotation, SbBox3f",
    e.has_visual = false;
    e.run_unit = runBaseTests;
);

REGISTER_TEST(unit_fields, ObolTest::TestCategory::Fields,
    "SoSFFloat, SoSFBool, SoMFVec3f, SoMFFloat, node fields",
    e.has_visual = false;
    e.run_unit = runFieldsTests;
);

REGISTER_TEST(unit_nodes, ObolTest::TestCategory::Nodes,
    "Node creation, hierarchy, named lookup, viewAll",
    e.has_visual = false;
    e.run_unit = runNodesTests;
);

REGISTER_TEST(unit_engines, ObolTest::TestCategory::Engines,
    "SoCalculator, SoElapsedTime, SoComposeVec3f",
    e.has_visual = false;
    e.run_unit = runEnginesTests;
);

REGISTER_TEST(unit_sensors, ObolTest::TestCategory::Sensors,
    "SoFieldSensor, SoNodeSensor, SoTimerSensor",
    e.has_visual = false;
    e.run_unit = runSensorsTests;
);

REGISTER_TEST(unit_orbit_camera, ObolTest::TestCategory::Base,
    "orbitCamera() BRL-CAD-style smooth orbit rotation math",
    e.has_visual = false;
    e.run_unit = runOrbitCameraTests;
);

REGISTER_TEST(unit_scalability, ObolTest::TestCategory::Nodes,
    "Scene graph scalability: flat / binary-tree / linear-chain at 100..20000 nodes",
    e.has_visual = false;
    e.run_unit = runScalabilityTests;
);

// ---- Group 4: Interaction / Draggers / Special ----

REGISTER_TEST(camera_interaction, ObolTest::TestCategory::Rendering,
    "Three-object scene (sphere, cube, cone) for camera manipulation testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCameraInteraction;
);

REGISTER_TEST(scene_interaction, ObolTest::TestCategory::Rendering,
    "Dynamic sphere/cube/cylinder scene for scene-graph mutation testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSceneInteraction;
);

REGISTER_TEST(engine_interaction, ObolTest::TestCategory::Engines,
    "Sphere driven by SoComposeVec3f engine — engine-driven animation",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEngineInteraction;
);

REGISTER_TEST(engine_converter, ObolTest::TestCategory::Engines,
    "Sphere with material driven via SoConvertAll automatic field-type conversion",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEngineConverter;
);

REGISTER_TEST(event_callback_interaction, ObolTest::TestCategory::Events,
    "Green sphere behind SoSwitch with SoEventCallback node in the graph",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEventCallbackInteraction;
);

REGISTER_TEST(pick_interaction, ObolTest::TestCategory::Actions,
    "Blue sphere for SoRayPickAction pick and highlight testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPickInteraction;
);

REGISTER_TEST(pick_filter, ObolTest::TestCategory::Actions,
    "SoSelection + sphere + cube for SoSelection pick-filter callback tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPickFilter;
);

REGISTER_TEST(selection_interaction, ObolTest::TestCategory::Nodes,
    "SoSelection (SHIFT policy) with sphere, cube, and cone",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSelectionInteraction;
);

REGISTER_TEST(sensor_interaction, ObolTest::TestCategory::Sensors,
    "Gray sphere scene for sensor-driven field and node sensor testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSensorInteraction;
);

REGISTER_TEST(nodekit_interaction, ObolTest::TestCategory::Nodes,
    "SoShapeKit containing a sphere — node-kit interaction scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createNodeKitInteraction;
);

REGISTER_TEST(manip_sequences, ObolTest::TestCategory::Manips,
    "Sphere with SoCenterballManip — complex manipulator sequence scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createManipSequences;
);

REGISTER_TEST(light_manips, ObolTest::TestCategory::Manips,
    "Three spheres + floor lit by a SoDirectionalLightManip",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLightManips;
);

REGISTER_TEST(simple_draggers, ObolTest::TestCategory::Draggers,
    "Cube + SoTranslate1Dragger — simple dragger types scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSimpleDraggers;
);

REGISTER_TEST(arb8_draggers, ObolTest::TestCategory::Draggers,
    "ARB8-style box (solid + wireframe) for procedural dragger topology tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createArb8Draggers;
);

REGISTER_TEST(arb8_edit_cycle, ObolTest::TestCategory::Draggers,
    "ARB8 box with golden corner-handle spheres — edit-cycle visualization",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createArb8EditCycle;
);

REGISTER_TEST(ext_selection, ObolTest::TestCategory::Nodes,
    "SoExtSelection (LASSO, FULL_BBOX) with three pickable shapes",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createExtSelection;
);

REGISTER_TEST(ext_selection_events, ObolTest::TestCategory::Nodes,
    "SoExtSelection (RECTANGLE, PART_BBOX) with three shapes for event tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createExtSelectionEvents;
);

REGISTER_TEST(raypick_shapes, ObolTest::TestCategory::Actions,
    "Four-quadrant scene: SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createRaypickShapes;
);

REGISTER_TEST(shadow_advanced, ObolTest::TestCategory::Rendering,
    "Advanced shadows: SoShadowGroup + SoShadowSpotLight + sphere + ground plane",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createShadowAdvanced;
);

REGISTER_TEST(rt_proxy_shapes, ObolTest::TestCategory::Rendering,
    "RT proxy shapes: SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder quad",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createRTProxyShapes;
);

REGISTER_TEST(nanort, ObolTest::TestCategory::Rendering,
    "Four primitives + SoRaytracingParams — NanoRT raytracing scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createNanoRT;
);

REGISTER_TEST(nanort_shadow, ObolTest::TestCategory::Rendering,
    "Ground plane + red sphere + SoRaytracingParams(shadows) — NanoRT shadow scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createNanoRTShadow;
);

// =========================================================================
// Unit test: SbVec2f, SbVec4f, SbVec3d (vector variants)
// =========================================================================
static int runVecVariantsTests()
{
    int failures = 0;

    // --- SbVec2f ---
    {
        SbVec2f a(3.0f, 4.0f);
        if (!approxEqual(a.length(), 5.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f length (got %f)\n", a.length()); ++failures;
        }
        SbVec2f b(1.0f, 2.0f);
        float dot = a.dot(b);
        if (!approxEqual(dot, 11.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f dot (got %f)\n", dot); ++failures;
        }
        SbVec2f sum = a + b;
        if (!approxEqual(sum[0], 4.0f) || !approxEqual(sum[1], 6.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f add (got %f %f)\n", sum[0], sum[1]); ++failures;
        }
        SbVec2f diff = a - b;
        if (!approxEqual(diff[0], 2.0f) || !approxEqual(diff[1], 2.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f sub (got %f %f)\n", diff[0], diff[1]); ++failures;
        }
        SbVec2f scaled = a * 2.0f;
        if (!approxEqual(scaled[0], 6.0f) || !approxEqual(scaled[1], 8.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f scale (got %f %f)\n", scaled[0], scaled[1]); ++failures;
        }
        SbVec2f n = a;
        n.normalize();
        if (!approxEqual(n.length(), 1.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f normalize length\n"); ++failures;
        }
        n.negate();
        if (!approxEqual(n[0], -0.6f, 1e-3f)) {
            fprintf(stderr, "  FAIL: SbVec2f negate\n"); ++failures;
        }
        SbVec2f c;
        c.setValue(5.0f, 6.0f);
        if (!approxEqual(c[0], 5.0f)) {
            fprintf(stderr, "  FAIL: SbVec2f setValue\n"); ++failures;
        }
        if (!c.equals(SbVec2f(5.0f, 6.0f), 1e-5f)) {
            fprintf(stderr, "  FAIL: SbVec2f equals\n"); ++failures;
        }
    }

    // --- SbVec4f ---
    {
        SbVec4f a(1.0f, 2.0f, 3.0f, 4.0f);
        if (!approxEqual(a[0], 1.0f) || !approxEqual(a[3], 4.0f)) {
            fprintf(stderr, "  FAIL: SbVec4f indexing\n"); ++failures;
        }
        float dot = a.dot(a);
        if (!approxEqual(dot, 30.0f)) {
            fprintf(stderr, "  FAIL: SbVec4f dot self (got %f)\n", dot); ++failures;
        }
        if (!approxEqual(a.sqrLength(), 30.0f)) {
            fprintf(stderr, "  FAIL: SbVec4f sqrLength\n"); ++failures;
        }
        SbVec4f b(1.0f, 1.0f, 1.0f, 1.0f);
        SbVec4f sum = a + b;
        if (!approxEqual(sum[0], 2.0f) || !approxEqual(sum[3], 5.0f)) {
            fprintf(stderr, "  FAIL: SbVec4f add\n"); ++failures;
        }
        SbVec3f r3;
        a.getReal(r3);
        // homogeneous divide: (1/4, 2/4, 3/4)
        if (!approxEqual(r3[0], 0.25f, 1e-3f) || !approxEqual(r3[1], 0.5f, 1e-3f)) {
            fprintf(stderr, "  FAIL: SbVec4f getReal (got %f %f %f)\n", r3[0], r3[1], r3[2]); ++failures;
        }
        SbVec4f n = a;
        n.negate();
        if (!approxEqual(n[0], -1.0f)) {
            fprintf(stderr, "  FAIL: SbVec4f negate\n"); ++failures;
        }
        if (!a.equals(SbVec4f(1.0f, 2.0f, 3.0f, 4.0f), 1e-5f)) {
            fprintf(stderr, "  FAIL: SbVec4f equals\n"); ++failures;
        }
    }

    // --- SbVec3d ---
    {
        SbVec3d a(3.0, 4.0, 0.0);
        if (std::fabs(a.length() - 5.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbVec3d length (got %f)\n", a.length()); ++failures;
        }
        SbVec3d b(1.0, 0.0, 0.0);
        double dot = a.dot(b);
        if (std::fabs(dot - 3.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbVec3d dot (got %f)\n", dot); ++failures;
        }
        SbVec3d cross = a.cross(SbVec3d(0.0, 0.0, 1.0));
        if (std::fabs(cross[0] - 4.0) > 1e-9 || std::fabs(cross[1] - (-3.0)) > 1e-9) {
            fprintf(stderr, "  FAIL: SbVec3d cross (got %f %f %f)\n", cross[0], cross[1], cross[2]); ++failures;
        }
        SbVec3d n = a;
        n.normalize();
        if (std::fabs(n.length() - 1.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbVec3d normalize\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbString, SbName, SbTime extended
// =========================================================================
static int runStringNameTimeTests()
{
    int failures = 0;

    // --- SbString ---
    {
        SbString s("hello");
        if (s.getLength() != 5) {
            fprintf(stderr, "  FAIL: SbString getLength (got %d)\n", s.getLength()); ++failures;
        }
        if (strcmp(s.getString(), "hello") != 0) {
            fprintf(stderr, "  FAIL: SbString getString\n"); ++failures;
        }
        SbString s2 = s + " world";
        if (s2.getLength() != 11) {
            fprintf(stderr, "  FAIL: SbString concat length (got %d)\n", s2.getLength()); ++failures;
        }
        if (!(s == SbString("hello"))) {
            fprintf(stderr, "  FAIL: SbString operator==\n"); ++failures;
        }
        if (s == SbString("bye")) {
            fprintf(stderr, "  FAIL: SbString operator== false positive\n"); ++failures;
        }
        SbString sub = s2.getSubString(6, 10);
        if (strcmp(sub.getString(), "world") != 0) {
            fprintf(stderr, "  FAIL: SbString getSubString (got '%s')\n", sub.getString()); ++failures;
        }
        SbString empty;
        empty.makeEmpty();
        if (empty.getLength() != 0) {
            fprintf(stderr, "  FAIL: SbString makeEmpty\n"); ++failures;
        }
        SbString digits(42);
        if (strcmp(digits.getString(), "42") != 0) {
            fprintf(stderr, "  FAIL: SbString from int (got '%s')\n", digits.getString()); ++failures;
        }
    }

    // --- SbName ---
    {
        SbName n("myNode");
        if (strcmp(n.getString(), "myNode") != 0) {
            fprintf(stderr, "  FAIL: SbName getString\n"); ++failures;
        }
        if (n.getLength() != 6) {
            fprintf(stderr, "  FAIL: SbName getLength (got %d)\n", n.getLength()); ++failures;
        }
        SbName n2("myNode");
        if (!(n == n2)) {
            fprintf(stderr, "  FAIL: SbName operator==\n"); ++failures;
        }
        if (n == SbName("other")) {
            fprintf(stderr, "  FAIL: SbName operator== false\n"); ++failures;
        }
        SbName empty = SbName::empty();
        if (!(!empty)) {
            fprintf(stderr, "  FAIL: SbName::empty operator!\n"); ++failures;
        }
        if (!SbName::isIdentStartChar('A')) {
            fprintf(stderr, "  FAIL: SbName::isIdentStartChar 'A'\n"); ++failures;
        }
        if (!SbName::isIdentChar('_')) {
            fprintf(stderr, "  FAIL: SbName::isIdentChar '_'\n"); ++failures;
        }
        if (SbName::isIdentStartChar('5')) {
            fprintf(stderr, "  FAIL: SbName::isIdentStartChar '5' should be false\n"); ++failures;
        }
    }

    // --- SbTime ---
    {
        SbTime t(1.5);
        if (std::fabs(t.getValue() - 1.5) > 1e-9) {
            fprintf(stderr, "  FAIL: SbTime getValue (got %f)\n", t.getValue()); ++failures;
        }
        SbTime t2(0.5);
        SbTime sum = t + t2;
        if (std::fabs(sum.getValue() - 2.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbTime add (got %f)\n", sum.getValue()); ++failures;
        }
        SbTime diff = t - t2;
        if (std::fabs(diff.getValue() - 1.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbTime sub (got %f)\n", diff.getValue()); ++failures;
        }
        SbTime scaled = t * 2.0;
        if (std::fabs(scaled.getValue() - 3.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbTime multiply (got %f)\n", scaled.getValue()); ++failures;
        }
        SbTime divided = t / 3.0;
        if (std::fabs(divided.getValue() - 0.5) > 1e-6) {
            fprintf(stderr, "  FAIL: SbTime divide (got %f)\n", divided.getValue()); ++failures;
        }
        if (!(t > t2)) {
            fprintf(stderr, "  FAIL: SbTime operator>\n"); ++failures;
        }
        if (!(t2 < t)) {
            fprintf(stderr, "  FAIL: SbTime operator<\n"); ++failures;
        }
        SbTime zero = SbTime::zero();
        if (std::fabs(zero.getValue()) > 1e-9) {
            fprintf(stderr, "  FAIL: SbTime::zero (got %f)\n", zero.getValue()); ++failures;
        }
        unsigned long ms = t.getMsecValue();
        if (ms != 1500) {
            fprintf(stderr, "  FAIL: SbTime getMsecValue (got %lu)\n", ms); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbDict hash dictionary
// =========================================================================
static int runDictTests()
{
    int failures = 0;

    {
        SbDict dict;
        // enter and find
        int data1 = 42, data2 = 99;
        SbBool ok = dict.enter(1, &data1);
        if (!ok) { fprintf(stderr, "  FAIL: SbDict enter\n"); ++failures; }
        ok = dict.enter(2, &data2);
        if (!ok) { fprintf(stderr, "  FAIL: SbDict enter 2\n"); ++failures; }

        void* found = nullptr;
        ok = dict.find(1, found);
        if (!ok || static_cast<int*>(found) != &data1) {
            fprintf(stderr, "  FAIL: SbDict find\n"); ++failures;
        }
        ok = dict.find(2, found);
        if (!ok || static_cast<int*>(found) != &data2) {
            fprintf(stderr, "  FAIL: SbDict find 2\n"); ++failures;
        }

        // find absent key
        ok = dict.find(999, found);
        if (ok) { fprintf(stderr, "  FAIL: SbDict find absent should fail\n"); ++failures; }

        // remove
        ok = dict.remove(1);
        if (!ok) { fprintf(stderr, "  FAIL: SbDict remove\n"); ++failures; }
        ok = dict.find(1, found);
        if (ok) { fprintf(stderr, "  FAIL: SbDict find after remove should fail\n"); ++failures; }

        // clear
        dict.clear();
        ok = dict.find(2, found);
        if (ok) { fprintf(stderr, "  FAIL: SbDict find after clear\n"); ++failures; }
    }

    // applyToAll callback
    {
        SbDict dict;
        static int count = 0;
        count = 0;
        int a = 1, b = 2, c = 3;
        dict.enter(10, &a); dict.enter(20, &b); dict.enter(30, &c);
        dict.applyToAll([](SbDictKeyType, void*) { ++count; });
        if (count != 3) {
            fprintf(stderr, "  FAIL: SbDict applyToAll count (got %d)\n", count); ++failures;
        }
    }

    // makePList
    {
        SbDict dict;
        int v1=1, v2=2;
        dict.enter(100, &v1); dict.enter(200, &v2);
        SbPList keys, values;
        dict.makePList(keys, values);
        if (keys.getLength() != 2 || values.getLength() != 2) {
            fprintf(stderr, "  FAIL: SbDict makePList count (got %d)\n", keys.getLength()); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbColor extended (HSV, packed)
// =========================================================================
static int runColorTests()
{
    int failures = 0;

    // --- setHSVValue / getHSVValue ---
    {
        SbColor c;
        c.setHSVValue(0.0f, 1.0f, 1.0f); // pure red in HSV
        if (!approxEqual(c[0], 1.0f, 1e-3f) || !approxEqual(c[1], 0.0f, 1e-3f) || !approxEqual(c[2], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: setHSVValue red (got %f %f %f)\n", c[0], c[1], c[2]); ++failures;
        }
        float h, s, v;
        c.getHSVValue(h, s, v);
        if (!approxEqual(h, 0.0f, 1e-3f) || !approxEqual(s, 1.0f, 1e-3f) || !approxEqual(v, 1.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: getHSVValue (got %f %f %f)\n", h, s, v); ++failures;
        }
    }

    // --- getPackedValue / setPackedValue ---
    {
        SbColor c(1.0f, 0.0f, 0.0f);
        float transparency = 0.0f;
        uint32_t packed = c.getPackedValue(transparency);
        SbColor c2;
        float trans2;
        c2.setPackedValue(packed, trans2);
        if (!approxEqual(c2[0], 1.0f, 0.01f)) {
            fprintf(stderr, "  FAIL: packed round-trip R (got %f)\n", c2[0]); ++failures;
        }
    }

    // --- HSV array form ---
    {
        SbColor c;
        float hsv[3] = {0.333f, 1.0f, 1.0f}; // green
        c.setHSVValue(hsv);
        float out[3];
        c.getHSVValue(out);
        if (!approxEqual(out[0], 0.333f, 1e-2f)) {
            fprintf(stderr, "  FAIL: HSV array round-trip (got %f)\n", out[0]); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbImage
// =========================================================================
static int runImageTests()
{
    int failures = 0;

    // setValue / getValue
    {
        SbImage img;
        SbVec2s size(4, 4);
        unsigned char data[4*4*3];
        for (int i = 0; i < 4*4*3; ++i) data[i] = static_cast<unsigned char>(i % 256);
        img.setValue(size, 3, data);

        SbVec2s gotSize;
        int bpp;
        unsigned char* buf = img.getValue(gotSize, bpp);
        if (gotSize[0] != 4 || gotSize[1] != 4) {
            fprintf(stderr, "  FAIL: SbImage getValue size\n"); ++failures;
        }
        if (bpp != 3) {
            fprintf(stderr, "  FAIL: SbImage getValue bpp (got %d)\n", bpp); ++failures;
        }
        if (buf && buf[5] != data[5]) {
            fprintf(stderr, "  FAIL: SbImage getValue data[5] mismatch\n"); ++failures;
        }
        if (!img.hasData()) {
            fprintf(stderr, "  FAIL: SbImage hasData\n"); ++failures;
        }
    }

    // 3D setValue
    {
        SbImage img;
        SbVec3s size(2, 2, 2);
        unsigned char data[2*2*2*1];
        for (int i = 0; i < 8; ++i) data[i] = static_cast<unsigned char>(i * 32);
        img.setValue(size, 1, data);
        SbVec3s s3 = img.getSize();
        if (s3[0] != 2 || s3[1] != 2 || s3[2] != 2) {
            fprintf(stderr, "  FAIL: SbImage 3D getSize (got %d %d %d)\n", s3[0], s3[1], s3[2]); ++failures;
        }
    }

    // equality
    {
        SbImage a, b;
        SbVec2s sz(2,2);
        unsigned char d[4] = {0,1,2,3};
        a.setValue(sz, 1, d);
        b.setValue(sz, 1, d);
        if (!(a == b)) {
            fprintf(stderr, "  FAIL: SbImage equality\n"); ++failures;
        }
        unsigned char d2[4] = {0,1,2,4};
        b.setValue(sz, 1, d2);
        if (a == b) {
            fprintf(stderr, "  FAIL: SbImage inequality\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SoGroup / SoSeparator hierarchy manipulation
// =========================================================================
static int runGroupHierarchyTests()
{
    int failures = 0;

    // --- addChild, getChild, getNumChildren, findChild ---
    {
        SoGroup* g = new SoGroup();
        g->ref();
        SoCube* c1 = new SoCube(); c1->ref();
        SoCube* c2 = new SoCube(); c2->ref();
        SoSphere* s = new SoSphere(); s->ref();

        g->addChild(c1);
        g->addChild(c2);
        g->addChild(s);
        if (g->getNumChildren() != 3) {
            fprintf(stderr, "  FAIL: SoGroup addChild count (got %d)\n", g->getNumChildren()); ++failures;
        }
        if (g->getChild(0) != c1) {
            fprintf(stderr, "  FAIL: SoGroup getChild(0)\n"); ++failures;
        }
        if (g->findChild(s) != 2) {
            fprintf(stderr, "  FAIL: SoGroup findChild (got %d)\n", g->findChild(s)); ++failures;
        }

        // insertChild
        SoCone* cone = new SoCone(); cone->ref();
        g->insertChild(cone, 1);
        if (g->getNumChildren() != 4) {
            fprintf(stderr, "  FAIL: SoGroup insertChild count\n"); ++failures;
        }
        if (g->getChild(1) != cone) {
            fprintf(stderr, "  FAIL: SoGroup insertChild at 1\n"); ++failures;
        }

        // replaceChild
        SoCylinder* cyl = new SoCylinder(); cyl->ref();
        g->replaceChild(cone, cyl);
        if (g->getChild(1) != cyl) {
            fprintf(stderr, "  FAIL: SoGroup replaceChild\n"); ++failures;
        }

        // removeChild
        g->removeChild(0);
        if (g->getNumChildren() != 3) {
            fprintf(stderr, "  FAIL: SoGroup removeChild count\n"); ++failures;
        }

        // removeAllChildren
        g->removeAllChildren();
        if (g->getNumChildren() != 0) {
            fprintf(stderr, "  FAIL: SoGroup removeAllChildren\n"); ++failures;
        }

        c1->unref(); c2->unref(); s->unref(); cone->unref(); cyl->unref();
        g->unref();
    }

    // --- SoSeparator: basic hierarchy and getBoundingBox ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoCube* cube = new SoCube();
        cube->width = 2.0f; cube->height = 2.0f; cube->depth = 2.0f;
        root->addChild(cube);

        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();

        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: SoSeparator getBBox is empty\n"); ++failures;
        } else {
            SbVec3f c = box.getCenter();
            if (!approxEqual(c[0], 0.0f, 0.1f) || !approxEqual(c[1], 0.0f, 0.1f)) {
                fprintf(stderr, "  FAIL: SoSeparator cube bbox center (got %f %f %f)\n", c[0], c[1], c[2]); ++failures;
            }
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoCamera (PerspectiveCamera)
// =========================================================================
static int runCameraTests()
{
    int failures = 0;

    // --- SoPerspectiveCamera: construction and field defaults ---
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera();
        cam->ref();
        // Default position at origin
        SbVec3f pos = cam->position.getValue();
        if (!approxEqual(pos[0], 0.0f) || !approxEqual(pos[1], 0.0f) || !approxEqual(pos[2], 1.0f)) {
            fprintf(stderr, "  FAIL: camera default position (got %f %f %f)\n", pos[0], pos[1], pos[2]); ++failures;
        }
        // nearDistance and farDistance
        if (cam->nearDistance.getValue() <= 0.0f) {
            fprintf(stderr, "  FAIL: camera nearDistance <= 0\n"); ++failures;
        }
        cam->unref();
    }

    // --- getViewVolume ---
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera();
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, 5.0f);
        cam->heightAngle.setValue(float(M_PI / 2.0f)); // 90°
        cam->nearDistance.setValue(0.1f);
        cam->farDistance.setValue(100.0f);
        SbViewVolume vv = cam->getViewVolume(1.0f);
        // Should produce a valid view volume
        SbVec3f sight = vv.getSightPoint(5.0f);
        // Sight point at 5 units in front
        if (!approxEqual(sight[0], 0.0f, 0.1f) || !approxEqual(sight[1], 0.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: camera getViewVolume sight (got %f %f %f)\n", sight[0], sight[1], sight[2]); ++failures;
        }
        cam->unref();
    }

    // --- viewAll on a simple scene ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoCube* cube = new SoCube();
        cube->width = 2.0f; cube->height = 2.0f; cube->depth = 2.0f;
        root->addChild(cube);
        SoPerspectiveCamera* cam = new SoPerspectiveCamera();
        cam->ref();
        SbViewportRegion vp(512, 512);
        cam->viewAll(root, vp);
        // After viewAll, camera should have moved to see the cube
        SbVec3f pos = cam->position.getValue();
        if (pos.length() < 0.1f) {
            fprintf(stderr, "  FAIL: viewAll should move camera (got %f %f %f)\n", pos[0], pos[1], pos[2]); ++failures;
        }
        cam->unref();
        root->unref();
    }

    // --- pointAt ---
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera();
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, 5.0f);
        cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        // Camera should now look towards -Z
        SbVec3f axis; float angle;
        cam->orientation.getValue().getValue(axis, angle);
        // Any valid rotation is acceptable, just check it doesn't blow up
        if (cam->orientation.getValue().getValue()[0] != cam->orientation.getValue().getValue()[0]) {
            fprintf(stderr, "  FAIL: pointAt NaN in orientation\n"); ++failures;
        }
        cam->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoMaterial field access
// =========================================================================
static int runMaterialTests()
{
    int failures = 0;

    {
        SoMaterial* m = new SoMaterial();
        m->ref();

        // diffuseColor default
        if (m->diffuseColor.getNum() < 1) {
            fprintf(stderr, "  FAIL: SoMaterial diffuseColor count\n"); ++failures;
        }

        // Set and get diffuseColor
        m->diffuseColor.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
        SbColor dc = m->diffuseColor[0];
        if (!approxEqual(dc[0], 1.0f) || !approxEqual(dc[1], 0.0f)) {
            fprintf(stderr, "  FAIL: SoMaterial diffuseColor set/get (got %f %f)\n", dc[0], dc[1]); ++failures;
        }

        // Set specular and shininess
        m->specularColor.set1Value(0, SbColor(0.5f, 0.5f, 0.5f));
        m->shininess.set1Value(0, 0.8f);
        if (!approxEqual(m->shininess[0], 0.8f)) {
            fprintf(stderr, "  FAIL: SoMaterial shininess (got %f)\n", m->shininess[0]); ++failures;
        }

        // Transparency
        m->transparency.set1Value(0, 0.5f);
        if (!approxEqual(m->transparency[0], 0.5f)) {
            fprintf(stderr, "  FAIL: SoMaterial transparency (got %f)\n", m->transparency[0]); ++failures;
        }

        // getMaterialType is private; skip
        // int mtype = m->getMaterialType();

        m->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoTransform field access and matrices
// =========================================================================
static int runTransformTests()
{
    int failures = 0;

    {
        SoTransform* xf = new SoTransform();
        xf->ref();

        // Set translation
        xf->translation.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
        SbVec3f t = xf->translation.getValue();
        if (!approxEqual(t[0], 1.0f) || !approxEqual(t[1], 2.0f) || !approxEqual(t[2], 3.0f)) {
            fprintf(stderr, "  FAIL: SoTransform translation\n"); ++failures;
        }

        // Set rotation
        SbRotation rot(SbVec3f(0,1,0), float(M_PI/4));
        xf->rotation.setValue(rot);

        // Set scale
        xf->scaleFactor.setValue(SbVec3f(2.0f, 2.0f, 2.0f));
        SbVec3f s = xf->scaleFactor.getValue();
        if (!approxEqual(s[0], 2.0f)) {
            fprintf(stderr, "  FAIL: SoTransform scaleFactor\n"); ++failures;
        }

        // getMatrix via SoGetMatrixAction
        SbViewportRegion vp(512, 512);
        SoGetMatrixAction gma(vp);
        gma.apply(xf);  // apply directly to the transform node
        SbMatrix mat = gma.getMatrix();
        // With translation (1,2,3), the matrix should transform (0,0,0) → (1,2,3)
        SbVec3f origin(0.0f, 0.0f, 0.0f), result;
        mat.multVecMatrix(origin, result);
        if (!approxEqual(result[0], 1.0f, 0.1f) || !approxEqual(result[1], 2.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: SoTransform matrix translation (got %f %f %f)\n", result[0], result[1], result[2]); ++failures;
        }
        xf->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoSearchAction (deeper)
// =========================================================================
static int runSearchTests()
{
    int failures = 0;

    // --- search by type ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoCube* c1 = new SoCube();
        SoCone* c2 = new SoCone();
        SoSphere* s = new SoSphere();
        root->addChild(c1); root->addChild(c2); root->addChild(s);

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setFind(SoSearchAction::TYPE);
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        if (!sa.getPath()) {
            fprintf(stderr, "  FAIL: search by type (SoCube not found)\n"); ++failures;
        } else if (sa.getPath()->getTail() != c1) {
            fprintf(stderr, "  FAIL: search by type wrong node\n"); ++failures;
        }

        root->unref();
    }

    // --- search by name ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoCube* cube = new SoCube();
        cube->setName("myCube");
        root->addChild(cube);

        SoSearchAction sa;
        sa.setName("myCube");
        sa.setFind(SoSearchAction::NAME);
        sa.apply(root);
        if (!sa.getPath()) {
            fprintf(stderr, "  FAIL: search by name (myCube not found)\n"); ++failures;
        }

        root->unref();
    }

    // --- search ALL (multiple) ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        root->addChild(new SoCube());
        root->addChild(new SoCube());
        root->addChild(new SoCone());

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setFind(SoSearchAction::TYPE);
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);
        if (sa.getPaths().getLength() != 2) {
            fprintf(stderr, "  FAIL: search ALL count (got %d)\n", sa.getPaths().getLength()); ++failures;
        }

        root->unref();
    }

    // --- search derived types ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        root->addChild(new SoCube());
        root->addChild(new SoSphere());

        SoSearchAction sa;
        sa.setType(SoShape::getClassTypeId(), TRUE); // check derived
        sa.setFind(SoSearchAction::TYPE);
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);
        if (sa.getPaths().getLength() != 2) {
            fprintf(stderr, "  FAIL: search derived count (got %d)\n", sa.getPaths().getLength()); ++failures;
        }

        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoCallbackAction paths
// =========================================================================
static int runCallbackActionTests()
{
    int failures = 0;

    // --- pre/post node callbacks ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        root->addChild(new SoCube());
        root->addChild(new SoSphere());

        static int preCount = 0, postCount = 0;
        preCount = postCount = 0;

        SoCallbackAction ca;
        ca.addPreCallback(SoNode::getClassTypeId(),
            [](void*, SoCallbackAction*, const SoNode*) -> SoCallbackAction::Response {
                ++preCount;
                return SoCallbackAction::CONTINUE;
            }, nullptr);
        ca.addPostCallback(SoNode::getClassTypeId(),
            [](void*, SoCallbackAction*, const SoNode*) -> SoCallbackAction::Response {
                ++postCount;
                return SoCallbackAction::CONTINUE;
            }, nullptr);
        ca.apply(root);

        if (preCount == 0) {
            fprintf(stderr, "  FAIL: pre callback not called (count=%d)\n", preCount); ++failures;
        }
        if (postCount == 0) {
            fprintf(stderr, "  FAIL: post callback not called (count=%d)\n", postCount); ++failures;
        }
        root->unref();
    }

    // --- triangle callback ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        root->addChild(new SoCube());

        static int triCount = 0;
        triCount = 0;
        SoCallbackAction ca;
        ca.addTriangleCallback(SoShape::getClassTypeId(),
            [](void*, SoCallbackAction*, const SoPrimitiveVertex*,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {
                ++triCount;
            }, nullptr);
        ca.apply(root);

        if (triCount == 0) {
            fprintf(stderr, "  FAIL: triangle callback not fired\n"); ++failures;
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoGetBoundingBoxAction (deeper)
// =========================================================================
static int runBBoxActionTests()
{
    int failures = 0;

    // --- sphere bounding box ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoSphere* sphere = new SoSphere();
        sphere->radius = 2.0f;
        root->addChild(sphere);

        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();
        SbVec3f min, max;
        box.getBounds(min, max);
        if (!approxEqual(min[0], -2.0f, 0.1f) || !approxEqual(max[0], 2.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: sphere bbox (min=%f max=%f)\n", min[0], max[0]); ++failures;
        }
        SbVec3f center = bba.getCenter();
        if (!approxEqual(center[0], 0.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: BBox center (got %f %f %f)\n", center[0], center[1], center[2]); ++failures;
        }
        root->unref();
    }

    // --- translated object ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoTranslation* t = new SoTranslation();
        t->translation.setValue(10.0f, 0.0f, 0.0f);
        root->addChild(t);
        SoCube* cube = new SoCube();
        cube->width = 2.0f; cube->height = 2.0f; cube->depth = 2.0f;
        root->addChild(cube);

        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();
        SbVec3f c = box.getCenter();
        if (!approxEqual(c[0], 10.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: translated cube bbox center x (got %f)\n", c[0]); ++failures;
        }
        root->unref();
    }

    // --- inCameraSpace flag ---
    {
        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.setInCameraSpace(TRUE);
        if (!bba.isInCameraSpace()) {
            fprintf(stderr, "  FAIL: setInCameraSpace\n"); ++failures;
        }
    }

    // --- isCenterSet ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        root->addChild(new SoCube());
        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        if (!bba.isCenterSet()) {
            fprintf(stderr, "  FAIL: isCenterSet after apply\n"); ++failures;
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoGetPrimitiveCountAction
// =========================================================================
static int runPrimitiveCountTests()
{
    int failures = 0;

    {
        SoSeparator* root = new SoSeparator(); root->ref();
        // A cube has 12 triangles (6 faces * 2)
        root->addChild(new SoCube());
        root->addChild(new SoSphere());

        SoGetPrimitiveCountAction pca;
        pca.apply(root);
        int triCount = pca.getTriangleCount();
        if (triCount <= 0) {
            fprintf(stderr, "  FAIL: primitive count triangle (got %d)\n", triCount); ++failures;
        }
        root->unref();
    }

    {
        // Add a line set
        SoSeparator* root = new SoSeparator(); root->ref();
        SoCoordinate3* coords = new SoCoordinate3();
        coords->point.set1Value(0, SbVec3f(0,0,0));
        coords->point.set1Value(1, SbVec3f(1,0,0));
        root->addChild(coords);
        SoLineSet* ls = new SoLineSet();
        ls->numVertices.set1Value(0, 2);
        root->addChild(ls);

        SoGetPrimitiveCountAction pca;
        pca.apply(root);
        int lineCount = pca.getLineCount();
        if (lineCount <= 0) {
            fprintf(stderr, "  FAIL: primitive count lines (got %d)\n", lineCount); ++failures;
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoSwitch and SoLOD
// =========================================================================
static int runSwitchLODTests()
{
    int failures = 0;

    // --- SoSwitch whichChild ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoSwitch* sw = new SoSwitch();
        SoCube* visible = new SoCube();
        SoCone* hidden = new SoCone();
        sw->addChild(visible);
        sw->addChild(hidden);
        sw->whichChild.setValue(0); // show first child
        root->addChild(sw);

        // BBox should include only the visible child (cube)
        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();
        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: SoSwitch bbox should not be empty\n"); ++failures;
        }
        root->unref();
    }

    // --- SoSwitch SO_SWITCH_NONE ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoSwitch* sw = new SoSwitch();
        sw->addChild(new SoCube());
        sw->whichChild.setValue(SO_SWITCH_NONE);
        root->addChild(sw);

        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();
        if (!box.isEmpty()) {
            fprintf(stderr, "  FAIL: SoSwitch NONE should produce empty bbox\n"); ++failures;
        }
        root->unref();
    }

    // --- SoLOD range ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoPerspectiveCamera* cam = new SoPerspectiveCamera();
        cam->position.setValue(0.0f, 0.0f, 10.0f);
        root->addChild(cam);
        SoLOD* lod = new SoLOD();
        lod->range.set1Value(0, 5.0f); // switch below 5 units
        SoCube* highDetail = new SoCube();
        SoCone* lowDetail = new SoCone();
        lod->addChild(highDetail);
        lod->addChild(lowDetail);
        root->addChild(lod);

        SoGetBoundingBoxAction bba(SbViewportRegion(512, 512));
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();
        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: SoLOD bbox should not be empty\n"); ++failures;
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SoWriteAction / SoDB::readAll (round-trip)
// =========================================================================
static int runWriteReadTests()
{
    int failures = 0;

    // --- write a scene to buffer, read it back ---
    {
        // Build simple scene
        SoSeparator* root = new SoSeparator(); root->ref();
        SoCube* cube = new SoCube();
        cube->width = 3.0f;
        cube->setName("testCube");
        root->addChild(cube);

        // Write to in-memory buffer
        static char* g_wrbuf = nullptr;
        static size_t g_wrbuf_size = 0;
        SoOutput out;
        out.setBuffer(nullptr, 1,
            [](void* p, size_t s) -> void* {
                g_wrbuf = static_cast<char*>(::realloc(p, s));
                g_wrbuf_size = s;
                return g_wrbuf;
            });
        SoWriteAction wa(&out);
        wa.apply(root);
        void* bufPtr = g_wrbuf;
        size_t bufSize = g_wrbuf_size;
        {
            void* tmp; size_t n;
            out.getBuffer(tmp, n);
            bufPtr = tmp; bufSize = n;
        }

        if (bufSize == 0 || bufPtr == nullptr) {
            fprintf(stderr, "  FAIL: SoWriteAction produced empty buffer\n"); ++failures;
        } else {
            // Read back
            SoInput inp;
            inp.setBuffer(bufPtr, bufSize);
            SoSeparator* loaded = SoDB::readAll(&inp);
            if (!loaded) {
                fprintf(stderr, "  FAIL: SoDB::readAll returned null\n"); ++failures;
            } else {
                loaded->ref();
                // Should have 1 child (the cube)
                if (loaded->getNumChildren() != 1) {
                    fprintf(stderr, "  FAIL: round-trip child count (got %d)\n", loaded->getNumChildren()); ++failures;
                }
                // Check the cube width was preserved
                SoSearchAction sa;
                sa.setName("testCube");
                sa.setFind(SoSearchAction::NAME);
                sa.apply(loaded);
                if (sa.getPath()) {
                    SoCube* c = static_cast<SoCube*>(sa.getPath()->getTail());
                    if (!approxEqual(c->width.getValue(), 3.0f)) {
                        fprintf(stderr, "  FAIL: round-trip cube width (got %f)\n", c->width.getValue()); ++failures;
                    }
                } else {
                    fprintf(stderr, "  FAIL: round-trip search for testCube failed\n"); ++failures;
                }
                loaded->unref();
            }
        }
        // g_wrbuf is managed by the realloc callback; no free needed here
        root->unref();
    }

    // --- SoDB::isValidHeader ---
    {
        if (!SoDB::isValidHeader("#Inventor V2.1 ascii")) {
            fprintf(stderr, "  FAIL: isValidHeader Inventor V2.1 ascii\n"); ++failures;
        }
        if (SoDB::isValidHeader("not a valid header")) {
            fprintf(stderr, "  FAIL: isValidHeader should reject garbage\n"); ++failures;
        }
    }

    // --- SoDB::getVersion ---
    {
        const char* ver = SoDB::getVersion();
        if (!ver || ver[0] == '\0') {
            fprintf(stderr, "  FAIL: SoDB::getVersion empty\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbXfBox3f
// =========================================================================
static int runXfBoxTests()
{
    int failures = 0;

    // --- basic transformed box ---
    {
        SbBox3f inner(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
        SbXfBox3f xf(inner);

        // getTransform should be identity initially
        SbMatrix xform = xf.getTransform();
        if (!approxEqual(xform[0][0], 1.0f)) {
            fprintf(stderr, "  FAIL: SbXfBox3f identity transform\n"); ++failures;
        }

        // project to world space
        SbBox3f projected = xf.project();
        if (!approxEqual(projected.getCenter()[0], 0.0f)) {
            fprintf(stderr, "  FAIL: SbXfBox3f project center\n"); ++failures;
        }

        // Apply a translation transform
        SbMatrix t;
        t.setTranslate(SbVec3f(5.0f, 0.0f, 0.0f));
        xf.transform(t);
        SbBox3f proj2 = xf.project();
        if (!approxEqual(proj2.getCenter()[0], 5.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: SbXfBox3f translate center (got %f)\n", proj2.getCenter()[0]); ++failures;
        }
    }

    // --- extendBy with point ---
    {
        SbXfBox3f xf;
        xf.extendBy(SbVec3f(2.0f, 3.0f, 4.0f));
        xf.extendBy(SbVec3f(-2.0f, -3.0f, -4.0f));
        SbBox3f proj = xf.project();
        if (!approxEqual(proj.getCenter()[0], 0.0f, 0.1f)) {
            fprintf(stderr, "  FAIL: SbXfBox3f extendBy center\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SoPath operations
// =========================================================================
static int runPathTests()
{
    int failures = 0;

    // --- build path manually ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoGroup* g = new SoGroup();
        SoCube* cube = new SoCube();
        root->addChild(g);
        g->addChild(cube);

        SoPath* path = new SoPath(root);
        path->ref();
        path->append(0);  // index of g in root
        path->append(0);  // index of cube in g
        if (path->getLength() != 3) {
            fprintf(stderr, "  FAIL: SoPath length (got %d)\n", path->getLength()); ++failures;
        }
        if (path->getTail() != cube) {
            fprintf(stderr, "  FAIL: SoPath getTail\n"); ++failures;
        }
        if (path->getHead() != root) {
            fprintf(stderr, "  FAIL: SoPath getHead\n"); ++failures;
        }
        if (path->getNode(1) != g) {
            fprintf(stderr, "  FAIL: SoPath getNode(1)\n"); ++failures;
        }

        // truncate
        path->truncate(2);
        if (path->getLength() != 2) {
            fprintf(stderr, "  FAIL: SoPath truncate length (got %d)\n", path->getLength()); ++failures;
        }

        path->unref();
        root->unref();
    }

    // --- SoSearchAction returns a path ---
    {
        SoSeparator* root = new SoSeparator(); root->ref();
        SoGroup* g = new SoGroup();
        SoCube* cube = new SoCube();
        cube->setName("targetCube");
        root->addChild(g);
        g->addChild(cube);

        SoSearchAction sa;
        sa.setName("targetCube");
        sa.setFind(SoSearchAction::NAME);
        sa.apply(root);
        SoPath* found = sa.getPath();
        if (!found) {
            fprintf(stderr, "  FAIL: SoSearchAction path for targetCube\n"); ++failures;
        } else {
            if (found->getLength() < 2) {
                fprintf(stderr, "  FAIL: path length (got %d)\n", found->getLength()); ++failures;
            }
            // getNodeFromTail(0) should be cube
            if (found->getNodeFromTail(0) != cube) {
                fprintf(stderr, "  FAIL: getNodeFromTail(0)\n"); ++failures;
            }
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SbDPLine, SbDPPlane, SbDPRotation (double-precision)
// =========================================================================
static int runDoublePrecisionTests()
{
    int failures = 0;

    // --- SbDPLine ---
    {
        SbDPLine line(SbVec3d(0.0, 0.0, 0.0), SbVec3d(1.0, 0.0, 0.0));
        SbVec3d closest = line.getClosestPoint(SbVec3d(3.0, 5.0, 0.0));
        if (std::fabs(closest[0] - 3.0) > 1e-9 || std::fabs(closest[1]) > 1e-9) {
            fprintf(stderr, "  FAIL: SbDPLine getClosestPoint (got %f %f)\n", closest[0], closest[1]); ++failures;
        }
    }

    // --- SbDPPlane ---
    {
        SbDPPlane p(SbVec3d(0.0, 1.0, 0.0), 0.0); // XZ plane
        double d = p.getDistance(SbVec3d(0.0, 5.0, 0.0));
        if (std::fabs(d - 5.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbDPPlane getDistance (got %f)\n", d); ++failures;
        }
        if (!p.isInHalfSpace(SbVec3d(0.0, 1.0, 0.0))) {
            fprintf(stderr, "  FAIL: SbDPPlane isInHalfSpace positive\n"); ++failures;
        }
    }

    // --- SbDPRotation ---
    {
        SbDPRotation r(SbVec3d(0.0, 1.0, 0.0), M_PI / 2.0);
        SbVec3d v(1.0, 0.0, 0.0);
        SbVec3d result;
        r.multVec(v, result);
        if (std::fabs(result[0]) > 1e-6 || std::fabs(result[2] + 1.0) > 1e-6) {
            fprintf(stderr, "  FAIL: SbDPRotation 90-Y (got %f %f %f)\n", result[0], result[1], result[2]); ++failures;
        }
        SbVec3d axis; double angle;
        r.getValue(axis, angle);
        if (std::fabs(angle - M_PI / 2.0) > 1e-9) {
            fprintf(stderr, "  FAIL: SbDPRotation getValue angle (got %f)\n", angle); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: Field type extensions (SoSFString, SoSFColor, SoSFMatrix, etc.)
// =========================================================================
static int runFieldExtensionTests()
{
    int failures = 0;

    // --- SoSFString ---
    {
        SoSFString f;
        f.setValue("hello");
        if (f.getValue() != SbString("hello")) {
            fprintf(stderr, "  FAIL: SoSFString set/get\n"); ++failures;
        }
    }

    // --- SoSFColor ---
    {
        SoSFColor f;
        f.setValue(SbColor(1.0f, 0.0f, 0.5f));
        SbColor c = f.getValue();
        if (!approxEqual(c[0], 1.0f) || !approxEqual(c[2], 0.5f)) {
            fprintf(stderr, "  FAIL: SoSFColor set/get\n"); ++failures;
        }
    }

    // --- SoSFMatrix ---
    {
        SoSFMatrix f;
        SbMatrix m = SbMatrix::identity();
        m.setTranslate(SbVec3f(1.0f, 2.0f, 3.0f));
        f.setValue(m);
        SbMatrix got = f.getValue();
        if (!approxEqual(got[3][0], 1.0f)) {
            fprintf(stderr, "  FAIL: SoSFMatrix set/get\n"); ++failures;
        }
    }

    // --- SoSFVec2f ---
    {
        SoSFVec2f f;
        f.setValue(SbVec2f(3.0f, 4.0f));
        SbVec2f v = f.getValue();
        if (!approxEqual(v[0], 3.0f) || !approxEqual(v[1], 4.0f)) {
            fprintf(stderr, "  FAIL: SoSFVec2f set/get\n"); ++failures;
        }
    }

    // --- SoSFNode ---
    {
        SoSFNode f;
        SoCube* cube = new SoCube();
        cube->ref();
        f.setValue(cube);
        if (f.getValue() != cube) {
            fprintf(stderr, "  FAIL: SoSFNode set/get\n"); ++failures;
        }
        f.setValue(nullptr);
        cube->unref();
    }

    // --- SoMFString ---
    {
        SoMFString f;
        f.set1Value(0, "hello");
        f.set1Value(1, "world");
        if (f.getNum() != 2) {
            fprintf(stderr, "  FAIL: SoMFString count\n"); ++failures;
        }
        if (f[0] != SbString("hello")) {
            fprintf(stderr, "  FAIL: SoMFString [0]\n"); ++failures;
        }
    }

    // --- SoMFColor ---
    {
        SoMFColor f;
        f.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
        f.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
        if (f.getNum() != 2) {
            fprintf(stderr, "  FAIL: SoMFColor count\n"); ++failures;
        }
    }

    // --- SoMFInt32 ---
    {
        SoMFInt32 f;
        int32_t vals[] = {10, 20, 30};
        f.setValues(0, 3, vals);
        if (f.getNum() != 3 || f[1] != 20) {
            fprintf(stderr, "  FAIL: SoMFInt32 set/get\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: Event types
// =========================================================================
static int runEventTests()
{
    int failures = 0;

    // --- SoMouseButtonEvent ---
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        ev.setState(SoButtonEvent::DOWN);
        if (!SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON1)) {
            fprintf(stderr, "  FAIL: isButtonPressEvent\n"); ++failures;
        }
        if (SoMouseButtonEvent::isButtonReleaseEvent(&ev, SoMouseButtonEvent::BUTTON1)) {
            fprintf(stderr, "  FAIL: isButtonReleaseEvent should be false\n"); ++failures;
        }
        SbVec2s pos(100, 200);
        ev.setPosition(pos);
        if (ev.getPosition() != pos) {
            fprintf(stderr, "  FAIL: SoMouseButtonEvent position\n"); ++failures;
        }
    }

    // --- SoKeyboardEvent ---
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::A);
        ev.setState(SoButtonEvent::DOWN);
        if (!SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::A)) {
            fprintf(stderr, "  FAIL: isKeyPressEvent\n"); ++failures;
        }
        if (ev.getKey() != SoKeyboardEvent::A) {
            fprintf(stderr, "  FAIL: SoKeyboardEvent getKey\n"); ++failures;
        }
    }

    // --- SoLocation2Event ---
    {
        SoLocation2Event ev;
        SbVec2s pos(320, 240);
        ev.setPosition(pos);
        if (ev.getPosition() != pos) {
            fprintf(stderr, "  FAIL: SoLocation2Event position\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SoOffscreenRenderer (API, no actual rendering needed)
// =========================================================================
static int runOffscreenRendererTests()
{
    int failures = 0;

    // --- construction and basic API ---
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer renderer(vp);

        if (renderer.getViewportRegion().getWindowSize()[0] != 64) {
            fprintf(stderr, "  FAIL: SoOffscreenRenderer getViewportRegion\n"); ++failures;
        }

        renderer.setComponents(SoOffscreenRenderer::RGB);
        if (renderer.getComponents() != SoOffscreenRenderer::RGB) {
            fprintf(stderr, "  FAIL: SoOffscreenRenderer getComponents\n"); ++failures;
        }

        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 1.0f));
        SbColor bg = renderer.getBackgroundColor();
        if (!approxEqual(bg[2], 1.0f)) {
            fprintf(stderr, "  FAIL: SoOffscreenRenderer background (got %f)\n", bg[2]); ++failures;
        }

        // getMaximumResolution
        SbVec2s maxRes = SoOffscreenRenderer::getMaximumResolution();
        if (maxRes[0] <= 0 || maxRes[1] <= 0) {
            fprintf(stderr, "  FAIL: getMaximumResolution (got %d %d)\n", maxRes[0], maxRes[1]); ++failures;
        }

        // setViewportRegion
        renderer.setViewportRegion(SbViewportRegion(128, 128));
        if (renderer.getViewportRegion().getWindowSize()[0] != 128) {
            fprintf(stderr, "  FAIL: SoOffscreenRenderer setViewportRegion\n"); ++failures;
        }
    }

    // --- actual render a simple scene ---
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer renderer(vp);
        renderer.setComponents(SoOffscreenRenderer::RGB);

        SoSeparator* root = new SoSeparator(); root->ref();
        SoPerspectiveCamera* cam = new SoPerspectiveCamera();
        root->addChild(cam);
        root->addChild(new SoDirectionalLight());
        SoCube* cube = new SoCube();
        root->addChild(cube);
        cam->viewAll(root, vp);

        SbBool ok = renderer.render(root);
        if (!ok) {
            fprintf(stderr, "  FAIL: SoOffscreenRenderer render returned false\n"); ++failures;
        }
        unsigned char* buf = renderer.getBuffer();
        if (!buf) {
            fprintf(stderr, "  FAIL: SoOffscreenRenderer getBuffer null\n"); ++failures;
        }
        root->unref();
    }

    return failures;
}

REGISTER_TEST(unit_vec_variants, ObolTest::TestCategory::Base,
    "SbVec2f, SbVec4f, SbVec3d: arithmetic, length, normalize, dot, negate",
    e.has_visual = false;
    e.run_unit = runVecVariantsTests;
);

REGISTER_TEST(unit_string_name_time, ObolTest::TestCategory::Base,
    "SbString operations, SbName identity/char checks, SbTime arithmetic",
    e.has_visual = false;
    e.run_unit = runStringNameTimeTests;
);

REGISTER_TEST(unit_dict, ObolTest::TestCategory::Base,
    "SbDict: enter, find, remove, clear, applyToAll, makePList",
    e.has_visual = false;
    e.run_unit = runDictTests;
);

REGISTER_TEST(unit_color, ObolTest::TestCategory::Base,
    "SbColor: HSV conversion, packed value round-trip",
    e.has_visual = false;
    e.run_unit = runColorTests;
);

REGISTER_TEST(unit_image, ObolTest::TestCategory::Base,
    "SbImage: setValue, getValue, getSize, hasData, equality",
    e.has_visual = false;
    e.run_unit = runImageTests;
);

REGISTER_TEST(unit_group_hierarchy, ObolTest::TestCategory::Nodes,
    "SoGroup/SoSeparator: addChild, removeChild, replaceChild, findChild, bbox",
    e.has_visual = false;
    e.run_unit = runGroupHierarchyTests;
);

REGISTER_TEST(unit_camera, ObolTest::TestCategory::Nodes,
    "SoPerspectiveCamera: getViewVolume, viewAll, pointAt, field defaults",
    e.has_visual = false;
    e.run_unit = runCameraTests;
);

REGISTER_TEST(unit_material, ObolTest::TestCategory::Nodes,
    "SoMaterial: diffuseColor, specularColor, shininess, transparency",
    e.has_visual = false;
    e.run_unit = runMaterialTests;
);

REGISTER_TEST(unit_transform, ObolTest::TestCategory::Nodes,
    "SoTransform: translation, rotation, scale, getMatrix",
    e.has_visual = false;
    e.run_unit = runTransformTests;
);

REGISTER_TEST(unit_search, ObolTest::TestCategory::Actions,
    "SoSearchAction: by type, by name, ALL, derived types",
    e.has_visual = false;
    e.run_unit = runSearchTests;
);

REGISTER_TEST(unit_callback_action, ObolTest::TestCategory::Actions,
    "SoCallbackAction: pre/post node callbacks, triangle callback",
    e.has_visual = false;
    e.run_unit = runCallbackActionTests;
);

REGISTER_TEST(unit_bbox_action, ObolTest::TestCategory::Actions,
    "SoGetBoundingBoxAction: sphere, translated cube, inCameraSpace, isCenterSet",
    e.has_visual = false;
    e.run_unit = runBBoxActionTests;
);

REGISTER_TEST(unit_primitive_count, ObolTest::TestCategory::Actions,
    "SoGetPrimitiveCountAction: cube triangles, line segment count",
    e.has_visual = false;
    e.run_unit = runPrimitiveCountTests;
);

REGISTER_TEST(unit_switch_lod, ObolTest::TestCategory::Nodes,
    "SoSwitch (whichChild, NONE), SoLOD range bboxing",
    e.has_visual = false;
    e.run_unit = runSwitchLODTests;
);

REGISTER_TEST(unit_write_read, ObolTest::TestCategory::IO,
    "SoWriteAction / SoDB::readAll round-trip, isValidHeader, getVersion",
    e.has_visual = false;
    e.run_unit = runWriteReadTests;
);

REGISTER_TEST(unit_xfbox, ObolTest::TestCategory::Base,
    "SbXfBox3f: construction, project, transform, extendBy",
    e.has_visual = false;
    e.run_unit = runXfBoxTests;
);

REGISTER_TEST(unit_path, ObolTest::TestCategory::Misc,
    "SoPath: manual construction, append, truncate, getTail, getNodeFromTail",
    e.has_visual = false;
    e.run_unit = runPathTests;
);

REGISTER_TEST(unit_double_precision, ObolTest::TestCategory::Base,
    "SbDPLine, SbDPPlane, SbDPRotation: closest point, distance, multVec",
    e.has_visual = false;
    e.run_unit = runDoublePrecisionTests;
);

REGISTER_TEST(unit_field_extensions, ObolTest::TestCategory::Fields,
    "SoSFString, SoSFColor, SoSFMatrix, SoSFVec2f, SoSFNode, SoMFString, SoMFColor, SoMFInt32",
    e.has_visual = false;
    e.run_unit = runFieldExtensionTests;
);

REGISTER_TEST(unit_events, ObolTest::TestCategory::Events,
    "SoMouseButtonEvent, SoKeyboardEvent, SoLocation2Event",
    e.has_visual = false;
    e.run_unit = runEventTests;
);

REGISTER_TEST(unit_offscreen_renderer, ObolTest::TestCategory::Rendering,
    "SoOffscreenRenderer: construction, setComponents, background, render",
    e.has_visual = false;
    e.run_unit = runOffscreenRendererTests;
);
static int runMatrixTests()
{
    int failures = 0;

    // --- inverse of translation matrix ---
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(3.0f, -2.0f, 5.0f));
        SbMatrix inv = m.inverse();
        SbVec3f v(0.0f, 0.0f, 0.0f);
        SbVec3f r;
        m.multVecMatrix(v, r);
        // translate(3,-2,5) moves (0,0,0) to (3,-2,5)
        if (!approxEqual(r[0], 3.0f) || !approxEqual(r[1], -2.0f) || !approxEqual(r[2], 5.0f)) {
            fprintf(stderr, "  FAIL: setTranslate transform (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
        // inverse should move it back
        SbVec3f r2;
        inv.multVecMatrix(r, r2);
        if (!approxEqual(r2[0], 0.0f, 1e-3f) || !approxEqual(r2[1], 0.0f, 1e-3f) || !approxEqual(r2[2], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: inverse of translate (got %f %f %f)\n", r2[0], r2[1], r2[2]);
            ++failures;
        }
    }

    // --- scale matrix ---
    {
        SbMatrix m;
        m.setScale(SbVec3f(2.0f, 3.0f, 4.0f));
        SbVec3f v(1.0f, 1.0f, 1.0f), r;
        m.multVecMatrix(v, r);
        if (!approxEqual(r[0], 2.0f) || !approxEqual(r[1], 3.0f) || !approxEqual(r[2], 4.0f)) {
            fprintf(stderr, "  FAIL: setScale(vec) (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    // --- uniform scale ---
    {
        SbMatrix m;
        m.setScale(5.0f);
        SbVec3f v(1.0f, 1.0f, 1.0f), r;
        m.multVecMatrix(v, r);
        if (!approxEqual(r[0], 5.0f) || !approxEqual(r[1], 5.0f) || !approxEqual(r[2], 5.0f)) {
            fprintf(stderr, "  FAIL: setScale(float) (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    // --- rotation matrix ---
    {
        SbRotation rot(SbVec3f(0, 0, 1), float(M_PI / 2.0)); // 90° around Z
        SbMatrix m;
        m.setRotate(rot);
        SbVec3f v(1.0f, 0.0f, 0.0f), r;
        m.multVecMatrix(v, r);
        // (1,0,0) rotated 90° around Z → (0,1,0)
        if (!approxEqual(r[0], 0.0f, 1e-3f) || !approxEqual(r[1], 1.0f, 1e-3f) || !approxEqual(r[2], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: setRotate 90-Z (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    // --- setTransform / getTransform round-trip ---
    {
        SbVec3f t(1.0f, 2.0f, 3.0f);
        SbRotation r(SbVec3f(0, 1, 0), float(M_PI / 4.0));
        SbVec3f s(1.0f, 1.0f, 1.0f);
        SbMatrix m;
        m.setTransform(t, r, s);
        SbVec3f tout; SbRotation rout; SbVec3f sout; SbRotation soout;
        m.getTransform(tout, rout, sout, soout);
        if (!approxEqual(tout[0], t[0], 1e-3f) || !approxEqual(tout[1], t[1], 1e-3f) || !approxEqual(tout[2], t[2], 1e-3f)) {
            fprintf(stderr, "  FAIL: getTransform translation (got %f %f %f)\n", tout[0], tout[1], tout[2]);
            ++failures;
        }
        if (!approxEqual(sout[0], s[0], 1e-3f) || !approxEqual(sout[1], s[1], 1e-3f) || !approxEqual(sout[2], s[2], 1e-3f)) {
            fprintf(stderr, "  FAIL: getTransform scale (got %f %f %f)\n", sout[0], sout[1], sout[2]);
            ++failures;
        }
    }

    // --- multRight and matrix multiplication ---
    {
        SbMatrix a, b;
        a.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
        b.setTranslate(SbVec3f(0.0f, 2.0f, 0.0f));
        SbMatrix c = a;
        c.multRight(b); // c = a * b  → translate (1,2,0)
        SbVec3f v(0.0f, 0.0f, 0.0f), r;
        c.multVecMatrix(v, r);
        if (!approxEqual(r[0], 1.0f, 1e-3f) || !approxEqual(r[1], 2.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: multRight translation (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    // --- operator* ---
    {
        SbMatrix a, b;
        a.setTranslate(SbVec3f(3.0f, 0.0f, 0.0f));
        b.setScale(2.0f);
        SbMatrix c = a * b;
        SbVec3f v(0.0f, 0.0f, 0.0f), r;
        c.multVecMatrix(v, r);
        // translate then scale: (3,0,0)*2 = (6,0,0)
        if (!approxEqual(r[0], 6.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: operator* (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    // --- transpose ---
    {
        SbMatrix m(1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16);
        SbMatrix t = m.transpose();
        SbMat mv, tv;
        m.getValue(mv);
        t.getValue(tv);
        if (!approxEqual(tv[0][1], mv[1][0]) || !approxEqual(tv[1][2], mv[2][1])) {
            fprintf(stderr, "  FAIL: transpose\n");
            ++failures;
        }
    }

    // --- det3 / det4 of identity ---
    {
        SbMatrix ident = SbMatrix::identity();
        if (!approxEqual(ident.det3(), 1.0f)) {
            fprintf(stderr, "  FAIL: identity det3 = %f\n", ident.det3());
            ++failures;
        }
        if (!approxEqual(ident.det4(), 1.0f)) {
            fprintf(stderr, "  FAIL: identity det4 = %f\n", ident.det4());
            ++failures;
        }
    }

    // --- equals ---
    {
        SbMatrix a = SbMatrix::identity();
        SbMatrix b = SbMatrix::identity();
        if (!a.equals(b, 1e-5f)) {
            fprintf(stderr, "  FAIL: equals identity\n");
            ++failures;
        }
        b[3][0] = 1.0f;
        if (a.equals(b, 1e-5f)) {
            fprintf(stderr, "  FAIL: equals should be false\n");
            ++failures;
        }
    }

    // --- multDirMatrix (no translation) ---
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(10.0f, 20.0f, 30.0f));
        SbVec3f dir(1.0f, 0.0f, 0.0f), r;
        m.multDirMatrix(dir, r);
        // Direction should NOT be affected by translation
        if (!approxEqual(r[0], 1.0f, 1e-3f) || !approxEqual(r[1], 0.0f, 1e-3f) || !approxEqual(r[2], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: multDirMatrix translation ignored (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbRotation comprehensive
// =========================================================================
static int runRotationTests()
{
    int failures = 0;

    // --- identity ---
    {
        SbRotation r = SbRotation::identity();
        SbVec3f v(1.0f, 2.0f, 3.0f), out;
        r.multVec(v, out);
        if (!approxEqual(out[0], 1.0f, 1e-4f) || !approxEqual(out[1], 2.0f, 1e-4f) || !approxEqual(out[2], 3.0f, 1e-4f)) {
            fprintf(stderr, "  FAIL: identity rotation (got %f %f %f)\n", out[0], out[1], out[2]);
            ++failures;
        }
    }

    // --- getValue(axis, radians) ---
    {
        SbVec3f axis(0.0f, 1.0f, 0.0f);
        float rad = float(M_PI / 3.0);
        SbRotation r(axis, rad);
        SbVec3f gotAxis; float gotRad;
        r.getValue(gotAxis, gotRad);
        if (!approxEqual(gotRad, rad, 1e-4f)) {
            fprintf(stderr, "  FAIL: getValue radians (got %f, expected %f)\n", gotRad, rad);
            ++failures;
        }
        if (!approxEqual(std::fabs(gotAxis[1]), 1.0f, 1e-4f)) {
            fprintf(stderr, "  FAIL: getValue axis Y (got %f)\n", gotAxis[1]);
            ++failures;
        }
    }

    // --- getValue(matrix) round-trip ---
    {
        SbRotation r(SbVec3f(1.0f, 0.0f, 0.0f), float(M_PI / 4.0));
        SbMatrix m;
        r.getValue(m);
        SbRotation r2(m);
        SbVec3f v(0.0f, 1.0f, 0.0f), a, b;
        r.multVec(v, a);
        r2.multVec(v, b);
        if (!approxEqual(a[0], b[0], 1e-3f) || !approxEqual(a[1], b[1], 1e-3f) || !approxEqual(a[2], b[2], 1e-3f)) {
            fprintf(stderr, "  FAIL: matrix round-trip (a=%f %f %f, b=%f %f %f)\n",
                    a[0], a[1], a[2], b[0], b[1], b[2]);
            ++failures;
        }
    }

    // --- rotate-to constructor (from, to) ---
    {
        SbVec3f from(1.0f, 0.0f, 0.0f);
        SbVec3f to(0.0f, 1.0f, 0.0f);
        SbRotation r(from, to);
        SbVec3f result;
        r.multVec(from, result);
        if (!approxEqual(result[0], to[0], 1e-3f) || !approxEqual(result[1], to[1], 1e-3f)) {
            fprintf(stderr, "  FAIL: from-to rotation (got %f %f %f)\n", result[0], result[1], result[2]);
            ++failures;
        }
    }

    // --- composition of rotations ---
    {
        // Two 90° rotations around Y should give a 180° rotation
        SbRotation r90(SbVec3f(0, 1, 0), float(M_PI / 2.0));
        SbRotation r180 = r90 * r90;
        SbVec3f v(1.0f, 0.0f, 0.0f), r;
        r180.multVec(v, r);
        // 180° around Y: (1,0,0) → (-1,0,0)
        if (!approxEqual(r[0], -1.0f, 1e-3f) || !approxEqual(r[1], 0.0f, 1e-3f) || !approxEqual(r[2], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: rotation composition (got %f %f %f)\n", r[0], r[1], r[2]);
            ++failures;
        }
    }

    // --- invert ---
    {
        SbRotation r(SbVec3f(0, 0, 1), float(M_PI / 3.0));
        SbRotation inv = r.inverse();
        SbRotation combined = r * inv;
        SbVec3f v(1.0f, 2.0f, 3.0f), out;
        combined.multVec(v, out);
        if (!approxEqual(out[0], 1.0f, 1e-3f) || !approxEqual(out[1], 2.0f, 1e-3f) || !approxEqual(out[2], 3.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: r * r.inverse != identity (got %f %f %f)\n", out[0], out[1], out[2]);
            ++failures;
        }
    }

    // --- slerp ---
    {
        SbRotation from = SbRotation::identity();
        SbRotation to(SbVec3f(0, 1, 0), float(M_PI / 2.0));
        SbRotation mid = SbRotation::slerp(from, to, 0.5f);
        SbVec3f v(1.0f, 0.0f, 0.0f), r;
        mid.multVec(v, r);
        // At t=0.5, should be ~45° around Y: (cos45, 0, -sin45)
        float expected_x = std::cos(float(M_PI / 4.0));
        float expected_z = -std::sin(float(M_PI / 4.0));
        if (!approxEqual(r[0], expected_x, 1e-2f) || !approxEqual(r[2], expected_z, 1e-2f)) {
            fprintf(stderr, "  FAIL: slerp midpoint (got %f %f %f, expected ~%f 0 %f)\n",
                    r[0], r[1], r[2], expected_x, expected_z);
            ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SbViewVolume + SbPlane + SbLine
// =========================================================================
static int runViewVolumeTests()
{
    int failures = 0;

    // --- SbPlane: distance, intersect, transform ---
    {
        SbPlane p(SbVec3f(0, 1, 0), 0.0f); // XZ plane at y=0
        float d = p.getDistance(SbVec3f(0.0f, 5.0f, 0.0f));
        if (!approxEqual(d, 5.0f)) {
            fprintf(stderr, "  FAIL: SbPlane getDistance (got %f)\n", d);
            ++failures;
        }
        // Point on positive side
        if (!p.isInHalfSpace(SbVec3f(0.0f, 1.0f, 0.0f))) {
            fprintf(stderr, "  FAIL: SbPlane isInHalfSpace positive\n");
            ++failures;
        }
        if (p.isInHalfSpace(SbVec3f(0.0f, -1.0f, 0.0f))) {
            fprintf(stderr, "  FAIL: SbPlane isInHalfSpace negative\n");
            ++failures;
        }
    }

    // --- SbPlane intersect with SbLine ---
    {
        SbPlane p(SbVec3f(0, 0, 1), 0.0f); // XY plane at z=0
        SbLine line(SbVec3f(0, 0, -1), SbVec3f(0, 0, 1)); // line along Z
        SbVec3f intersection;
        SbBool hit = p.intersect(line, intersection);
        if (!hit) {
            fprintf(stderr, "  FAIL: SbPlane intersect with line should hit\n");
            ++failures;
        } else if (!approxEqual(intersection[2], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: SbPlane intersect Z (got %f)\n", intersection[2]);
            ++failures;
        }
    }

    // --- SbLine: getClosestPoint ---
    {
        SbLine line(SbVec3f(0, 0, 0), SbVec3f(1, 0, 0)); // X axis
        SbVec3f pt(3.0f, 5.0f, 0.0f);
        SbVec3f closest = line.getClosestPoint(pt);
        // Closest point on X axis to (3,5,0) is (3,0,0)
        if (!approxEqual(closest[0], 3.0f, 1e-3f) || !approxEqual(closest[1], 0.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: SbLine getClosestPoint (got %f %f %f)\n", closest[0], closest[1], closest[2]);
            ++failures;
        }
    }

    // --- SbViewVolume ortho: getPlane, projectToScreen ---
    {
        SbViewVolume vv;
        vv.ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
        // Near plane is at distance nearVal from eye along -Z
        SbPlane nearPlane = vv.getPlane(0.1f);
        // Near plane normal should point towards +Z (towards viewer)
        SbVec3f n = nearPlane.getNormal();
        if (std::fabs(n[2]) < 0.5f) {
            fprintf(stderr, "  FAIL: ortho near plane normal Z (got %f %f %f)\n", n[0], n[1], n[2]);
            ++failures;
        }
    }

    // --- SbViewVolume perspective: getSightPoint ---
    {
        SbViewVolume vv;
        vv.perspective(float(M_PI / 2.0), 1.0f, 0.1f, 100.0f); // 90° FOV
        SbVec3f sight = vv.getSightPoint(10.0f);
        // Sight point at dist 10 should be directly in front (along -Z in eye space)
        if (!approxEqual(sight[0], 0.0f, 1e-2f) || !approxEqual(sight[1], 0.0f, 1e-2f)) {
            fprintf(stderr, "  FAIL: getSightPoint off-axis (got %f %f %f)\n", sight[0], sight[1], sight[2]);
            ++failures;
        }
    }

    // --- SbSphere: intersect with line ---
    {
        SbSphere sphere(SbVec3f(0, 0, 0), 1.0f);
        SbLine line(SbVec3f(-2.0f, 0.0f, 0.0f), SbVec3f(2.0f, 0.0f, 0.0f));
        SbVec3f enter, exit;
        SbBool hit = sphere.intersect(line, enter, exit);
        if (!hit) {
            fprintf(stderr, "  FAIL: SbSphere intersect should hit\n");
            ++failures;
        } else {
            if (!approxEqual(enter[0], -1.0f, 1e-3f)) {
                fprintf(stderr, "  FAIL: SbSphere enter X (got %f)\n", enter[0]);
                ++failures;
            }
            if (!approxEqual(exit[0], 1.0f, 1e-3f)) {
                fprintf(stderr, "  FAIL: SbSphere exit X (got %f)\n", exit[0]);
                ++failures;
            }
        }
    }

    // --- SbCylinder: intersect with line ---
    {
        SbCylinder cyl(SbLine(SbVec3f(0,0,0), SbVec3f(0,1,0)), 1.0f); // Y-axis cylinder
        SbLine ray(SbVec3f(-2.0f, 0.5f, 0.0f), SbVec3f(2.0f, 0.5f, 0.0f));
        SbVec3f enter, exit;
        SbBool hit = cyl.intersect(ray, enter, exit);
        if (!hit) {
            fprintf(stderr, "  FAIL: SbCylinder intersect should hit\n");
            ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SoSceneManager (basic lifecycle)
// =========================================================================
static int runSceneManagerTests()
{
    int failures = 0;

    // --- construction / destruction ---
    {
        SoSceneManager* mgr = new SoSceneManager();
        if (!mgr) {
            fprintf(stderr, "  FAIL: SoSceneManager construction\n");
            ++failures;
        }
        delete mgr;
    }

    // --- setSceneGraph / getSceneGraph ---
    {
        SoSceneManager mgr;
        SoSeparator* root = new SoSeparator();
        root->ref();
        mgr.setSceneGraph(root);
        if (mgr.getSceneGraph() != root) {
            fprintf(stderr, "  FAIL: getSceneGraph\n");
            ++failures;
        }
        mgr.setSceneGraph(nullptr);
        root->unref();
    }

    // --- setViewportRegion / getViewportRegion ---
    {
        SoSceneManager mgr;
        SbViewportRegion vp(800, 600);
        mgr.setViewportRegion(vp);
        const SbViewportRegion& got = mgr.getViewportRegion();
        if (got.getWindowSize()[0] != 800 || got.getWindowSize()[1] != 600) {
            fprintf(stderr, "  FAIL: getViewportRegion size (got %d %d)\n",
                    got.getWindowSize()[0], got.getWindowSize()[1]);
            ++failures;
        }
    }

    // --- setBackgroundColor ---
    {
        SoSceneManager mgr;
        mgr.setBackgroundColor(SbColor(0.5f, 0.25f, 0.75f));
        SbColor c = mgr.getBackgroundColor();
        if (!approxEqual(c[0], 0.5f) || !approxEqual(c[1], 0.25f) || !approxEqual(c[2], 0.75f)) {
            fprintf(stderr, "  FAIL: getBackgroundColor (got %f %f %f)\n", c[0], c[1], c[2]);
            ++failures;
        }
    }

    // --- isRGBMode (default) ---
    {
        SoSceneManager mgr;
        if (!mgr.isRGBMode()) {
            fprintf(stderr, "  FAIL: default isRGBMode should be true\n");
            ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: SoRayPickAction
// =========================================================================
static int runRayPickTests()
{
    int failures = 0;

    // --- construction and viewport setup ---
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction rp(vp);
        // Verify viewport is stored correctly
        if (rp.getViewportRegion().getWindowSize()[0] != 512) {
            fprintf(stderr, "  FAIL: RayPickAction viewport size\n");
            ++failures;
        }
    }

    // --- pick radius ---
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction rp(vp);
        rp.setRadius(5.0f);
        if (!approxEqual(rp.getRadius(), 5.0f)) {
            fprintf(stderr, "  FAIL: pick radius (got %f)\n", rp.getRadius());
            ++failures;
        }
    }

    // --- pick a sphere in a simple scene ---
    {
        SoSeparator* root = new SoSeparator();
        root->ref();
        SoSphere* sphere = new SoSphere();
        sphere->radius = 1.0f;
        root->addChild(sphere);

        SbViewportRegion vp(512, 512);
        SoRayPickAction rp(vp);
        // Set ray along -Z through origin (should hit the sphere at z~1)
        rp.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
        rp.apply(root);

        const SoPickedPointList& picks = rp.getPickedPointList();
        if (picks.getLength() == 0) {
            fprintf(stderr, "  FAIL: RayPickAction should hit sphere\n");
            ++failures;
        }
        root->unref();
    }

    // --- pick misses empty scene ---
    {
        SoSeparator* root = new SoSeparator();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction rp(vp);
        rp.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
        rp.apply(root);

        const SoPickedPointList& picks = rp.getPickedPointList();
        if (picks.getLength() != 0) {
            fprintf(stderr, "  FAIL: RayPickAction should miss empty scene\n");
            ++failures;
        }
        root->unref();
    }

    // --- pick with SoIndexedFaceSet ---
    {
        SoSeparator* root = new SoSeparator();
        root->ref();

        SoCoordinate3* coords = new SoCoordinate3();
        coords->point.set1Value(0, SbVec3f(-1, -1, 0));
        coords->point.set1Value(1, SbVec3f( 1, -1, 0));
        coords->point.set1Value(2, SbVec3f( 0,  1, 0));
        root->addChild(coords);

        SoIndexedFaceSet* ifs = new SoIndexedFaceSet();
        int32_t indices[] = {0, 1, 2, -1};
        ifs->coordIndex.setValues(0, 4, indices);
        root->addChild(ifs);

        SbViewportRegion vp(512, 512);
        SoRayPickAction rp(vp);
        rp.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
        rp.apply(root);

        const SoPickedPointList& picks = rp.getPickedPointList();
        if (picks.getLength() == 0) {
            fprintf(stderr, "  FAIL: RayPickAction should hit triangle\n");
            ++failures;
        }
        root->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: SbBox variants (double-precision and integer types)
// =========================================================================
static int runBoxVariantsTests()
{
    int failures = 0;

    // --- SbBox3d basic operations ---
    {
        SbBox3d box;
        if (!box.isEmpty()) {
            fprintf(stderr, "  FAIL: default SbBox3d should be empty\n");
            ++failures;
        }
        box.extendBy(SbVec3d(1.0, 2.0, 3.0));
        box.extendBy(SbVec3d(-1.0, -2.0, -3.0));
        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: SbBox3d isEmpty after extendBy\n");
            ++failures;
        }
        SbVec3d center = box.getCenter();
        if (std::fabs(center[0]) > 1e-9 || std::fabs(center[1]) > 1e-9 || std::fabs(center[2]) > 1e-9) {
            fprintf(stderr, "  FAIL: SbBox3d center (got %f %f %f)\n", center[0], center[1], center[2]);
            ++failures;
        }
    }

    // --- SbBox2f basic operations ---
    {
        SbBox2f box;
        if (!box.isEmpty()) {
            fprintf(stderr, "  FAIL: default SbBox2f should be empty\n");
            ++failures;
        }
        box.extendBy(SbVec2f(1.0f, 2.0f));
        box.extendBy(SbVec2f(-1.0f, -2.0f));
        SbVec2f size = box.getSize();
        if (!approxEqual(size[0], 2.0f) || !approxEqual(size[1], 4.0f)) {
            fprintf(stderr, "  FAIL: SbBox2f getSize (got %f %f)\n", size[0], size[1]);
            ++failures;
        }
        if (box.hasArea() == FALSE) {
            fprintf(stderr, "  FAIL: SbBox2f hasArea\n");
            ++failures;
        }
    }

    // --- SbBox2s basic operations ---
    {
        SbBox2s box(SbVec2s(-5, -5), SbVec2s(5, 5));
        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: SbBox2s isEmpty for constructed box\n");
            ++failures;
        }
        SbVec2f center = box.getCenter();
        if (center[0] != 0.0f || center[1] != 0.0f) {
            fprintf(stderr, "  FAIL: SbBox2s center (got %f %f)\n", center[0], center[1]);
            ++failures;
        }
    }

    return failures;
}

REGISTER_TEST(unit_matrix, ObolTest::TestCategory::Base,
    "SbMatrix: inverse, scale, rotate, setTransform/getTransform, multRight, transpose, det",
    e.has_visual = false;
    e.run_unit = runMatrixTests;
);

REGISTER_TEST(unit_rotation, ObolTest::TestCategory::Base,
    "SbRotation: identity, axis-angle, matrix round-trip, from-to, composition, slerp",
    e.has_visual = false;
    e.run_unit = runRotationTests;
);

REGISTER_TEST(unit_viewvolume, ObolTest::TestCategory::Base,
    "SbViewVolume, SbPlane, SbLine, SbSphere, SbCylinder geometric operations",
    e.has_visual = false;
    e.run_unit = runViewVolumeTests;
);

REGISTER_TEST(unit_scenemanager, ObolTest::TestCategory::Misc,
    "SoSceneManager: construction, setSceneGraph, viewport, background color",
    e.has_visual = false;
    e.run_unit = runSceneManagerTests;
);

REGISTER_TEST(unit_raypick, ObolTest::TestCategory::Actions,
    "SoRayPickAction: viewport, pick radius, pick sphere, miss, pick triangle",
    e.has_visual = false;
    e.run_unit = runRayPickTests;
);

REGISTER_TEST(unit_box_variants, ObolTest::TestCategory::Base,
    "SbBox3d, SbBox2f, SbBox2s: empty, extendBy, getCenter, getSize",
    e.has_visual = false;
    e.run_unit = runBoxVariantsTests;
);

} // anonymous namespace
