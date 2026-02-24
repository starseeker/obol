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
 *  13. registerShapeType with handle callbacks
 *  14. buildHandleDraggers() returns non-null for handle-capable type
 *  15. buildHandleDraggers() returns nullptr for type without handles
 *  16. buildHandleDraggers() produces correct child count
 *  17. JSON topology: setShapeType extracts defaults from full-topology schema
 *  18. buildHandleDraggers() from topology (no handlesCB) — correct child count
 *  19. DRAG_NO_INTERSECT is a distinct DragType value
 *  20. registerShapeType with validateCB
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

    // ------------------------------------------------------------------
    // Handle/dragger API tests
    // ------------------------------------------------------------------

    // Minimal shape type with handle support for the dragger tests.
    // 2 params: [0]=x, [1]=y — one handle at (x, y, 0)
    static const auto twoParamBbox = [](const float* p, int n, SbVec3f& mn, SbVec3f& mx, void*){
        (void)p;(void)n; mn.setValue(-1,-1,-1); mx.setValue(1,1,1);
    };
    static const auto twoParamGeom = [](const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*){};
    static const auto twoParamHandles = [](const float* p, int n,
                                            std::vector<SoProceduralHandle>& handles, void*) {
        float x = (n>0) ? p[0] : 0.f;
        float y = (n>1) ? p[1] : 0.f;
        SoProceduralHandle h;
        h.position  = SbVec3f(x, y, 0.f);
        h.dragType  = SoProceduralHandle::DRAG_POINT;
        handles.push_back(h);
    };
    static const auto twoParamDrag = [](float* params, int n, int idx,
                                         const SbVec3f& oldP, const SbVec3f& newP, void*) {
        if(idx!=0||n<2) return;
        SbVec3f d=newP-oldP;
        params[0]+=d[0]; params[1]+=d[1];
    };

    // Use non-capturing lambda cast — wrap via plain function pointer
    // (lambdas without captures are implicitly convertible to function pointers)
    SbBool hdReg = SoProceduralShape::registerShapeType(
        "TwoParam_test", "",
        static_cast<SoProceduralBBoxCB>(twoParamBbox),
        static_cast<SoProceduralGeomCB>(twoParamGeom),
        static_cast<SoProceduralHandlesCB>(twoParamHandles),
        static_cast<SoProceduralHandleDragCB>(twoParamDrag));
    (void)hdReg;

    // ------------------------------------------------------------------
    // Test 13: registerShapeType with handle callbacks succeeds
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType with handle callbacks");
    {
        bool pass = (SoProceduralShape::isRegistered("TwoParam_test") == TRUE);
        runner.endTest(pass, pass ? "" : "TwoParam_test should be registered");
    }

    // ------------------------------------------------------------------
    // Test 14: buildHandleDraggers() returns non-null for type with handles
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers returns non-null for registered handle type");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->shapeType.setValue("TwoParam_test");
        float pv[] = { 0.5f, 0.5f };
        shape->params.setValues(0, 2, pv);

        SoSeparator* handles = shape->buildHandleDraggers();
        bool pass = (handles != nullptr);
        if (handles) handles->ref(); // adopt ownership

        if (handles) handles->unref();
        shape->unref();
        runner.endTest(pass, pass ? "" : "buildHandleDraggers returned nullptr");
    }

    // ------------------------------------------------------------------
    // Test 15: buildHandleDraggers() returns nullptr for type without handles
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers returns nullptr when no handle callbacks");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("UnitBox_test"); // registered WITHOUT handle callbacks

        SoSeparator* handles = shape->buildHandleDraggers();
        bool pass = (handles == nullptr);

        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected nullptr for type without handles");
    }

    // ------------------------------------------------------------------
    // Test 16: buildHandleDraggers() produces correct number of child nodes
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers produces one sub-separator per handle");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->shapeType.setValue("TwoParam_test");
        float pv[] = { 1.f, 2.f };
        shape->params.setValues(0, 2, pv);

        SoSeparator* handles = shape->buildHandleDraggers();
        bool pass = handles && (handles->getNumChildren() == 1); // 1 handle

        if (handles) { handles->ref(); handles->unref(); }
        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected 1 child separator for 1 handle");
    }

    // ------------------------------------------------------------------
    // JSON topology parsing tests — use a minimal cube schema
    // ------------------------------------------------------------------

    // Minimal 4-vertex tetrahedron schema with full topology and one handle
    static const char* kTetraSchema = R"({
      "type": "Tetra_test",
      "params": [
        {"name":"v0x","type":"float","default":-1.0},
        {"name":"v0y","type":"float","default":-1.0},
        {"name":"v0z","type":"float","default":-1.0},
        {"name":"v1x","type":"float","default": 1.0},
        {"name":"v1y","type":"float","default":-1.0},
        {"name":"v1z","type":"float","default":-1.0},
        {"name":"v2x","type":"float","default": 0.0},
        {"name":"v2y","type":"float","default": 1.0},
        {"name":"v2z","type":"float","default":-1.0},
        {"name":"v3x","type":"float","default": 0.0},
        {"name":"v3y","type":"float","default": 0.0},
        {"name":"v3z","type":"float","default": 1.0}
      ],
      "vertices": [
        {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},
        {"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
        {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},
        {"name":"v3","x":"v3x","y":"v3y","z":"v3z"}
      ],
      "faces": [
        {"name":"base","verts":["v0","v2","v1"]},
        {"name":"front","verts":["v0","v1","v3"]},
        {"name":"right","verts":["v1","v2","v3"]},
        {"name":"left","verts":["v2","v0","v3"]}
      ],
      "handles": [
        {"name":"v0_h","vertex":"v0","dragType":"DRAG_NO_INTERSECT"},
        {"name":"e01_h","edge":["v0","v1"],"dragType":"DRAG_ON_PLANE"},
        {"name":"f_base_h","face":"base","dragType":"DRAG_NO_INTERSECT"}
      ]
    })";

    static const auto tetraBbox = [](const float*, int, SbVec3f& mn, SbVec3f& mx, void*){
        mn.setValue(-2,-2,-2); mx.setValue(2,2,2);
    };
    static const auto tetraGeom = [](const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*){};

    SoProceduralShape::registerShapeType(
        "Tetra_test", kTetraSchema,
        static_cast<SoProceduralBBoxCB>(tetraBbox),
        static_cast<SoProceduralGeomCB>(tetraGeom));

    // ------------------------------------------------------------------
    // Test 17: JSON topology parsing — params extracted from schema
    // ------------------------------------------------------------------
    runner.startTest("JSON topology: setShapeType extracts 12 defaults for Tetra_test");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");
        bool pass = (shape->params.getNum() == 12);
        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected 12 params from JSON topology");
    }

    // ------------------------------------------------------------------
    // Test 18: buildHandleDraggers() from topology — correct child count
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers from topology: correct child count");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");

        SoSeparator* handles = shape->buildHandleDraggers();
        // Schema defines 3 handles
        bool pass = handles && (handles->getNumChildren() == 3);
        if (handles) { handles->ref(); handles->unref(); }
        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected 3 children from topology handles");
    }

    // ------------------------------------------------------------------
    // Test 19: DRAG_NO_INTERSECT enum value is distinct from other types
    // ------------------------------------------------------------------
    runner.startTest("DRAG_NO_INTERSECT is a distinct DragType value");
    {
        bool pass = (SoProceduralHandle::DRAG_NO_INTERSECT != SoProceduralHandle::DRAG_POINT)
                 && (SoProceduralHandle::DRAG_NO_INTERSECT != SoProceduralHandle::DRAG_ALONG_AXIS)
                 && (SoProceduralHandle::DRAG_NO_INTERSECT != SoProceduralHandle::DRAG_ON_PLANE);
        runner.endTest(pass, pass ? "" : "DRAG_NO_INTERSECT not distinct");
    }

    // ------------------------------------------------------------------
    // Test 20: registerShapeType with validateCB succeeds
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType with validateCB");
    {
        static const auto vCB = [](const float*, int, int, const SbVec3f&,
                                    const SbVec3f& proposed, SbVec3f& accepted,
                                    void*) -> SbBool {
            accepted = proposed; // always accept
            return TRUE;
        };
        SbBool ok = SoProceduralShape::registerShapeType(
            "ValidateTest", "",
            static_cast<SoProceduralBBoxCB>(tetraBbox),
            static_cast<SoProceduralGeomCB>(tetraGeom),
            nullptr, nullptr,
            static_cast<SoProceduralHandleValidateCB>(vCB));
        bool pass = (ok == TRUE) && (SoProceduralShape::isRegistered("ValidateTest") == TRUE);
        runner.endTest(pass, pass ? "" : "validateCB registration failed");
    }

    return runner.getSummary();


}
