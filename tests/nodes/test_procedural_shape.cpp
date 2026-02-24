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
 * @file test_procedural_shape.cpp
 * @brief Unit tests for SoProceduralShape — application-defined custom geometry.
 *
 * Tests:
 *   1. Shape-type registration (success and duplicate rejection)
 *   2. isRegistered() / getSchemaJSON() registry queries
 *   3. setShapeType() — field population and schema-default extraction
 *   4. params field get/set round-trip
 *   5. computeBBox via SoGetBoundingBoxAction
 *   6. Bbox scaling with changed params
 *   7. SoGetPrimitiveCountAction triangle count
 *   8. Unregistered shape type graceful handling
 *   9. Scene-graph ref/unref lifecycle
 *  10. Node type-id checks
 */

#include "../test_utils.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>

#include <cmath>
#include <cstring>

using namespace SimpleTest;

// ============================================================================
// Helper
// ============================================================================

static bool floatNear(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) < eps;
}

// ============================================================================
// "UnitBox" shape type used by the tests
//
// Parameters: [0] halfSize  (default 1.0)
// Generates an axis-aligned cube with 12 triangles (2 per face).
// ============================================================================

static const char* kBoxSchema = R"({
  "type"  : "UnitBox",
  "label" : "Unit Box",
  "params": [
    { "name": "halfSize", "type": "float", "default": 1.0,
      "min": 0.001, "max": 100.0, "label": "Half Size" }
  ]
})";

static void unitBox_bbox(const float* params, int numParams,
                         SbVec3f& minPt, SbVec3f& maxPt,
                         void* /*userdata*/)
{
    float h = (numParams > 0) ? params[0] : 1.0f;
    minPt.setValue(-h, -h, -h);
    maxPt.setValue( h,  h,  h);
}

static void unitBox_geom(const float* params, int numParams,
                         SoProceduralTriangles* tris,
                         SoProceduralWireframe* wire,
                         void* /*userdata*/)
{
    float h = (numParams > 0) ? params[0] : 1.0f;

    if (tris) {
        tris->vertices = {
            SbVec3f(-h,-h,-h), SbVec3f( h,-h,-h),
            SbVec3f( h, h,-h), SbVec3f(-h, h,-h),
            SbVec3f(-h,-h, h), SbVec3f( h,-h, h),
            SbVec3f( h, h, h), SbVec3f(-h, h, h)
        };
        // 12 triangles (2 per face × 6 faces)
        tris->indices = {
            0,1,2, 0,2,3,   // -Z
            4,6,5, 4,7,6,   // +Z
            0,3,7, 0,7,4,   // -X
            1,5,6, 1,6,2,   // +X
            0,4,5, 0,5,1,   // -Y
            3,2,6, 3,6,7    // +Y
        };
    }

    if (wire) {
        wire->vertices = {
            SbVec3f(-h,-h,-h), SbVec3f( h,-h,-h),
            SbVec3f( h, h,-h), SbVec3f(-h, h,-h),
            SbVec3f(-h,-h, h), SbVec3f( h,-h, h),
            SbVec3f( h, h, h), SbVec3f(-h, h, h)
        };
        wire->segments = {
            0,1, 1,2, 2,3, 3,0,
            4,5, 5,6, 6,7, 7,4,
            0,4, 1,5, 2,6, 3,7
        };
    }
}

// Dummy callbacks for duplicate-registration test
static void dummy_bbox(const float*, int, SbVec3f& mn, SbVec3f& mx, void*)
{ mn.setValue(-1,-1,-1); mx.setValue(1,1,1); }
static void dummy_geom(const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*) {}

// ============================================================================
// main
// ============================================================================

int main()
{
    TestFixture fixture;
    TestRunner  runner;

    // ------------------------------------------------------------------
    // Test 1: First registration succeeds
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType succeeds for new type");
    {
        SbBool ok = SoProceduralShape::registerShapeType(
                        "UnitBox_test", kBoxSchema, unitBox_bbox, unitBox_geom);
        runner.endTest(ok == TRUE,
                       ok ? "" : "Expected TRUE for first registration");
    }

    // ------------------------------------------------------------------
    // Test 2: Duplicate registration is rejected
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType rejects duplicate name");
    {
        SbBool ok = SoProceduralShape::registerShapeType(
                        "UnitBox_test", "{}", dummy_bbox, dummy_geom);
        runner.endTest(ok == FALSE,
                       ok ? "Expected FALSE for duplicate registration" : "");
    }

    // ------------------------------------------------------------------
    // Test 3: isRegistered()
    // ------------------------------------------------------------------
    runner.startTest("isRegistered returns correct results");
    {
        bool pass = (SoProceduralShape::isRegistered("UnitBox_test") == TRUE)
                 && (SoProceduralShape::isRegistered("NoSuchShape")  == FALSE);
        runner.endTest(pass, pass ? "" : "isRegistered returned wrong value");
    }

    // ------------------------------------------------------------------
    // Test 4: getSchemaJSON() returns the registered JSON
    // ------------------------------------------------------------------
    runner.startTest("getSchemaJSON returns registered schema");
    {
        const char* schema = SoProceduralShape::getSchemaJSON("UnitBox_test");
        bool pass = (schema != nullptr)
                 && (strstr(schema, "UnitBox") != nullptr)
                 && (strstr(schema, "halfSize") != nullptr)
                 && (SoProceduralShape::getSchemaJSON("NoSuchShape") == nullptr);
        runner.endTest(pass, pass ? "" : "getSchemaJSON returned unexpected value");
    }

    // ------------------------------------------------------------------
    // Test 5: setShapeType() populates fields from schema defaults
    // ------------------------------------------------------------------
    runner.startTest("setShapeType populates shapeType, schemaJSON, and params");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();

        shape->setShapeType("UnitBox_test");

        bool pass = (strcmp(shape->shapeType.getValue().getString(),
                            "UnitBox_test") == 0)
                 && (shape->schemaJSON.getValue().getLength() > 0)
                 && (shape->params.getNum() == 1)
                 && floatNear(shape->params[0], 1.0f);

        shape->unref();
        runner.endTest(pass, pass ? "" : "setShapeType field population failed");
    }

    // ------------------------------------------------------------------
    // Test 6: params field get/set round-trip
    // ------------------------------------------------------------------
    runner.startTest("params field set/get round-trip");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();

        float vals[] = { 0.5f, 2.0f, 3.14f };
        shape->params.setValues(0, 3, vals);

        bool pass = (shape->params.getNum() == 3)
                 && floatNear(shape->params[0], 0.5f)
                 && floatNear(shape->params[1], 2.0f)
                 && floatNear(shape->params[2], 3.14f, 1e-3f);

        shape->unref();
        runner.endTest(pass, pass ? "" : "params field round-trip failed");
    }

    // ------------------------------------------------------------------
    // Test 7: SoGetBoundingBoxAction invokes the bbox callback
    // ------------------------------------------------------------------
    runner.startTest("SoGetBoundingBoxAction calls bbox callback");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("UnitBox_test"); // halfSize=1 → [-1,-1,-1]..[1,1,1]
        root->addChild(shape);

        SbViewportRegion vp(128, 128);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);

        SbBox3f box = bba.getBoundingBox();
        SbVec3f mn  = box.getMin();
        SbVec3f mx  = box.getMax();

        bool pass = !box.isEmpty()
                 && floatNear(mn[0], -1.0f) && floatNear(mn[1], -1.0f)
                 && floatNear(mn[2], -1.0f)
                 && floatNear(mx[0],  1.0f) && floatNear(mx[1],  1.0f)
                 && floatNear(mx[2],  1.0f);

        root->unref();
        runner.endTest(pass, pass ? "" : "Bounding box values incorrect");
    }

    // ------------------------------------------------------------------
    // Test 8: Bbox scales with params
    // ------------------------------------------------------------------
    runner.startTest("Bbox scales correctly when params change");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->shapeType.setValue("UnitBox_test");
        float pv = 3.0f;
        shape->params.setValues(0, 1, &pv); // halfSize = 3
        root->addChild(shape);

        SbViewportRegion vp(128, 128);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);

        SbBox3f box = bba.getBoundingBox();
        SbVec3f mn  = box.getMin();
        SbVec3f mx  = box.getMax();

        bool pass = floatNear(mn[0], -3.0f) && floatNear(mx[0], 3.0f);
        root->unref();
        runner.endTest(pass, pass ? "" : "Bbox did not scale with params");
    }

    // ------------------------------------------------------------------
    // Test 9: SoGetPrimitiveCountAction counts triangles
    // ------------------------------------------------------------------
    runner.startTest("SoGetPrimitiveCountAction reports correct triangle count");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("UnitBox_test"); // unitBox_geom → 12 triangles
        root->addChild(shape);

        SoGetPrimitiveCountAction pca;
        pca.apply(root);

        bool pass = (pca.getTriangleCount() == 12);
        root->unref();
        runner.endTest(pass,
                       pass ? "" : "Triangle count did not match expected 12");
    }

    // ------------------------------------------------------------------
    // Test 10: Unregistered type — graceful fallback (unit bbox, no crash)
    // ------------------------------------------------------------------
    runner.startTest("Unregistered shapeType does not crash, returns fallback bbox");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->shapeType.setValue("DoesNotExist");
        root->addChild(shape);

        SbViewportRegion vp(128, 128);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root); // must not crash

        bool pass = !bba.getBoundingBox().isEmpty();
        root->unref();
        runner.endTest(pass, pass ? "" : "Fallback bbox should not be empty");
    }

    // ------------------------------------------------------------------
    // Test 11: Scene-graph ref/unref lifecycle
    // ------------------------------------------------------------------
    runner.startTest("Scene-graph ref/unref lifecycle is correct");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("UnitBox_test");
        root->addChild(shape);

        bool pass = (root->getNumChildren() == 1)
                 && (root->getChild(0) == shape);

        root->unref(); // should cleanly destroy everything
        runner.endTest(pass, pass ? "" : "Scene-graph child check failed");
    }

    // ------------------------------------------------------------------
    // Test 12: Node type-id checks
    // ------------------------------------------------------------------
    runner.startTest("SoProceduralShape type-id checks");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();

        bool pass = (shape->getTypeId() != SoType::badType())
                 && shape->isOfType(SoShape::getClassTypeId())
                 && shape->isOfType(SoNode::getClassTypeId());

        shape->unref();
        runner.endTest(pass, pass ? "" : "Type-id check failed");
    }

    return runner.getSummary();
}

