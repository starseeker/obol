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
 * @file test_manips.cpp
 * @brief Tests for Coin3D manipulator classes.
 *
 * Exercises the manipulator infrastructure without requiring a display:
 *   - SoTrackballManip: instantiation, type, getDragger, getChildren, fields
 *   - SoJackManip:       instantiation, type, getDragger
 *   - SoTransformManip base API:
 *       translation / rotation / scaleFactor fields
 *       replaceNode (attach) / replaceManip (detach) lifecycle
 *       SoSearchAction traversal
 *
 * Subsystems improved: manips (Tier 3, COVERAGE_PLAN.md item 22)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/draggers/SoTrackballDragger.h>
#include <Inventor/draggers/SoJackDragger.h>
#include <Inventor/manips/SoTransformManip.h>
#include <Inventor/manips/SoTrackballManip.h>
#include <Inventor/manips/SoJackManip.h>
#include <Inventor/misc/SoChildList.h>

#include <cmath>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner  runner;

    // -----------------------------------------------------------------------
    // SoTrackballManip: instantiation and type
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: instantiation and type check");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: getDragger returns non-null
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: getDragger returns non-null");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip getDragger returned null");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: getDragger is an SoTrackballDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: getDragger is SoTrackballDragger");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoTrackballDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip getDragger is not SoTrackballDragger");
    }

    // -----------------------------------------------------------------------
    // SoJackManip: instantiation and type
    // -----------------------------------------------------------------------
    runner.startTest("SoJackManip: instantiation and type check");
    {
        SoJackManip *m = new SoJackManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoJackManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoJackManip: getDragger returns non-null SoJackDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoJackManip: getDragger returns SoJackDragger");
    {
        SoJackManip *m = new SoJackManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoJackDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoJackManip getDragger is not SoJackDragger");
    }

    // -----------------------------------------------------------------------
    // SoTransformManip: getChildren returns non-null child list
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: getChildren returns non-null child list");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SoChildList *cl = m->getChildren();
        bool pass = (cl != nullptr) && (cl->getLength() > 0);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip getChildren null or empty");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: translation field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: translation field set/get");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        m->translation.setValue(1.0f, 2.0f, 3.0f);
        SbVec3f t = m->translation.getValue();
        bool pass = (fabsf(t[0] - 1.0f) < 1e-5f) &&
                    (fabsf(t[1] - 2.0f) < 1e-5f) &&
                    (fabsf(t[2] - 3.0f) < 1e-5f);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip translation set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: scaleFactor field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: scaleFactor field set/get");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        m->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
        SbVec3f sf = m->scaleFactor.getValue();
        bool pass = (fabsf(sf[0] - 2.0f) < 1e-5f) &&
                    (fabsf(sf[1] - 2.0f) < 1e-5f) &&
                    (fabsf(sf[2] - 2.0f) < 1e-5f);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip scaleFactor set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: rotation field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: rotation field set/get");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SbRotation rot(SbVec3f(0.0f, 1.0f, 0.0f),
                       static_cast<float>(M_PI) / 4.0f);
        m->rotation.setValue(rot);
        SbRotation got = m->rotation.getValue();
        SbVec3f  ga; float ga_angle;
        SbVec3f  ra; float ra_angle;
        got.getValue(ga, ga_angle);
        rot.getValue(ra, ra_angle);
        bool pass = fabsf(ga_angle - ra_angle) < 1e-4f;
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip rotation set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoSearchAction finds a manip in a scene graph
    // -----------------------------------------------------------------------
    runner.startTest("SoSearchAction finds SoTrackballManip in scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoTrackballManip *m = new SoTrackballManip;
        root->addChild(m);

        SoSearchAction sa;
        sa.setType(SoTrackballManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);

        bool pass = (sa.getPath() != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find SoTrackballManip");
    }

    // -----------------------------------------------------------------------
    // replaceNode: attach SoTrackballManip in place of SoTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: replaceNode attaches manip to scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        // Start with a plain SoTransform
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(5.0f, 0.0f, 0.0f);
        root->addChild(xf);

        // Find the path to the SoTransform
        SoSearchAction sa;
        sa.setType(SoTransform::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *path = sa.getPath();

        bool pass = false;
        if (path != nullptr) {
            SoTrackballManip *manip = new SoTrackballManip;
            manip->ref();
            SbBool ok = manip->replaceNode(path);
            if (ok) {
                // Verify the translation was transferred to the manip
                SbVec3f t = manip->translation.getValue();
                pass = (fabsf(t[0] - 5.0f) < 1e-4f);
            }
            manip->unref();
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip replaceNode failed or field not transferred");
    }

    // -----------------------------------------------------------------------
    // replaceManip: detach manip and restore plain SoTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: replaceManip restores SoTransform");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        // Insert manip directly into scene graph
        SoTrackballManip *manip = new SoTrackballManip;
        manip->translation.setValue(7.0f, 0.0f, 0.0f);
        root->addChild(manip);

        // Find the path to the manip
        SoSearchAction sa;
        sa.setType(SoTrackballManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *manipPath = sa.getPath();

        bool pass = false;
        if (manipPath != nullptr) {
            // Replace the manip with a new plain SoTransform
            SoTransform *newXf = new SoTransform;
            SbBool ok = manip->replaceManip(manipPath, newXf);
            if (ok) {
                // The new SoTransform should have the translation value
                SbVec3f t = newXf->translation.getValue();
                pass = (fabsf(t[0] - 7.0f) < 1e-4f);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip replaceManip failed or field not transferred");
    }

    // -----------------------------------------------------------------------
    // replaceManip with null: replaceManip creates a default SoTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoJackManip: replaceManip(null) creates default SoTransform");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoJackManip *manip = new SoJackManip;
        manip->translation.setValue(3.0f, 0.0f, 0.0f);
        root->addChild(manip);

        SoSearchAction sa;
        sa.setType(SoJackManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *manipPath = sa.getPath();

        bool pass = false;
        if (manipPath != nullptr) {
            SbBool ok = manip->replaceManip(manipPath, nullptr);
            if (ok) {
                // Scene graph should now have a SoTransform instead of the manip
                SoSearchAction sa2;
                sa2.setType(SoTransform::getClassTypeId());
                sa2.setInterest(SoSearchAction::FIRST);
                sa2.apply(root);
                pass = (sa2.getPath() != nullptr);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoJackManip replaceManip(null) failed or no SoTransform found after detach");
    }

    return runner.getSummary();
}
