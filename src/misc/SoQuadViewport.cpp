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
 * @file SoQuadViewport.cpp
 * @brief Implementation of the SoQuadViewport four-quadrant viewport manager.
 *
 * Each quadrant owns an internal SoSeparator root that holds (optionally) a
 * camera followed by the shared scene graph.  Multiple parents of the shared
 * scene graph are handled transparently by Inventor's reference counting.
 */

#include <Inventor/SoQuadViewport.h>

#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/errors/SoDebugError.h>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SoQuadViewport::SoQuadViewport()
    : scene_(nullptr)
    , windowSize_(800, 600)
    , activeQuad_(0)
{
    for (int i = 0; i < NUM_QUADS; ++i) {
        quads_[i].root     = new SoSeparator;
        quads_[i].root->ref();
        quads_[i].camera   = nullptr;
        quads_[i].bgColor  = SbColor(0.0f, 0.0f, 0.0f);
        quads_[i].heAction = nullptr;
    }
    updateViewports();
}

SoQuadViewport::~SoQuadViewport()
{
    // Detach the shared scene from every quad root before releasing refs.
    if (scene_) {
        for (int i = 0; i < NUM_QUADS; ++i) {
            if (quads_[i].root->findChild(scene_) >= 0)
                quads_[i].root->removeChild(scene_);
        }
        scene_->unref();
        scene_ = nullptr;
    }

    for (int i = 0; i < NUM_QUADS; ++i) {
        if (quads_[i].camera) {
            quads_[i].camera->unref();
            quads_[i].camera = nullptr;
        }
        delete quads_[i].heAction;
        quads_[i].heAction = nullptr;

        quads_[i].root->unref();
        quads_[i].root = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Scene graph
// ---------------------------------------------------------------------------

void SoQuadViewport::setSceneGraph(SoNode * newScene)
{
    if (newScene == scene_)
        return;

    // Remove old scene from all quad roots.
    if (scene_) {
        for (int i = 0; i < NUM_QUADS; ++i) {
            if (quads_[i].root->findChild(scene_) >= 0)
                quads_[i].root->removeChild(scene_);
        }
        scene_->unref();
    }

    scene_ = newScene;

    if (scene_) {
        scene_->ref();
        // Append shared scene after any camera already in each quad root.
        for (int i = 0; i < NUM_QUADS; ++i)
            quads_[i].root->addChild(scene_);
    }
}

SoNode * SoQuadViewport::getSceneGraph() const
{
    return scene_;
}

// ---------------------------------------------------------------------------
// Window / viewport
// ---------------------------------------------------------------------------

void SoQuadViewport::setWindowSize(const SbVec2s & size)
{
    windowSize_ = size;
    updateViewports();
    // Rebuild handle-event actions with updated viewport
    for (int i = 0; i < NUM_QUADS; ++i) {
        delete quads_[i].heAction;
        quads_[i].heAction = nullptr;
    }
}

SbVec2s SoQuadViewport::getWindowSize() const
{
    return windowSize_;
}

SbVec2s SoQuadViewport::getQuadrantSize() const
{
    return SbVec2s(windowSize_[0] / 2, windowSize_[1] / 2);
}

const SbViewportRegion & SoQuadViewport::getViewportRegion(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::getViewportRegion",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return quads_[0].viewport;
    }
    return quads_[quad].viewport;
}

// Internal: recompute all four viewport sub-regions.
// Layout (2x2 grid, OpenGL origin bottom-left):
//   TOP_LEFT  (0): origin (0,      H/2),   size (W/2, H/2)
//   TOP_RIGHT (1): origin (W/2,    H/2),   size (W/2, H/2)
//   BOT_LEFT  (2): origin (0,      0),     size (W/2, H/2)
//   BOT_RIGHT (3): origin (W/2,    0),     size (W/2, H/2)
void SoQuadViewport::updateViewports()
{
    short W  = windowSize_[0];
    short H  = windowSize_[1];
    short qW = W / 2;
    short qH = H / 2;

    // TOP_LEFT
    quads_[TOP_LEFT].viewport.setWindowSize(W, H);
    quads_[TOP_LEFT].viewport.setViewportPixels(SbVec2s(0, qH), SbVec2s(qW, qH));

    // TOP_RIGHT
    quads_[TOP_RIGHT].viewport.setWindowSize(W, H);
    quads_[TOP_RIGHT].viewport.setViewportPixels(SbVec2s(qW, qH), SbVec2s(qW, qH));

    // BOTTOM_LEFT
    quads_[BOTTOM_LEFT].viewport.setWindowSize(W, H);
    quads_[BOTTOM_LEFT].viewport.setViewportPixels(SbVec2s(0, 0), SbVec2s(qW, qH));

    // BOTTOM_RIGHT
    quads_[BOTTOM_RIGHT].viewport.setWindowSize(W, H);
    quads_[BOTTOM_RIGHT].viewport.setViewportPixels(SbVec2s(qW, 0), SbVec2s(qW, qH));
}

// ---------------------------------------------------------------------------
// Cameras
// ---------------------------------------------------------------------------

void SoQuadViewport::setCamera(int quad, SoCamera * camera)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::setCamera",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return;
    }

    SoSeparator * root = quads_[quad].root;

    // Remove the previous camera from the root.
    if (quads_[quad].camera) {
        if (root->findChild(quads_[quad].camera) >= 0)
            root->removeChild(quads_[quad].camera);
        quads_[quad].camera->unref();
        quads_[quad].camera = nullptr;
    }

    quads_[quad].camera = camera;

    if (camera) {
        camera->ref();
        // Insert camera as the first child so it precedes the shared scene.
        root->insertChild(camera, 0);
    }
}

SoCamera * SoQuadViewport::getCamera(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS)
        return nullptr;
    return quads_[quad].camera;
}

// ---------------------------------------------------------------------------
// Active quadrant
// ---------------------------------------------------------------------------

void SoQuadViewport::setActiveQuadrant(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::setActiveQuadrant",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return;
    }
    activeQuad_ = quad;
}

int SoQuadViewport::getActiveQuadrant() const
{
    return activeQuad_;
}

// ---------------------------------------------------------------------------
// View-all
// ---------------------------------------------------------------------------

void SoQuadViewport::viewAll(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS)
        return;
    SoCamera * cam = quads_[quad].camera;
    if (!cam || !scene_)
        return;
    // Use the quadrant-sized viewport for the view-all computation.
    SbVec2s qs = getQuadrantSize();
    SbViewportRegion vp(qs[0], qs[1]);
    cam->viewAll(quads_[quad].root, vp);
}

void SoQuadViewport::viewAllQuadrants()
{
    for (int i = 0; i < NUM_QUADS; ++i)
        viewAll(i);
}

// ---------------------------------------------------------------------------
// Background colours
// ---------------------------------------------------------------------------

void SoQuadViewport::setBackgroundColor(int quad, const SbColor & color)
{
    if (quad < 0 || quad >= NUM_QUADS)
        return;
    quads_[quad].bgColor = color;
}

const SbColor & SoQuadViewport::getBackgroundColor(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS)
        return quads_[0].bgColor;
    return quads_[quad].bgColor;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::renderQuadrant(int quad, SoOffscreenRenderer * renderer)
{
    if (quad < 0 || quad >= NUM_QUADS || !renderer)
        return FALSE;

    SbVec2s qs = getQuadrantSize();
    SbViewportRegion qvp(qs[0], qs[1]);
    renderer->setViewportRegion(qvp);
    renderer->setBackgroundColor(quads_[quad].bgColor);
    return renderer->render(quads_[quad].root);
}

// ---------------------------------------------------------------------------
// Event routing
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::processEvent(const SoEvent * event)
{
    if (!event || activeQuad_ < 0 || activeQuad_ >= NUM_QUADS)
        return FALSE;

    // Lazily create handle-event action for the active quadrant.
    if (!quads_[activeQuad_].heAction) {
        quads_[activeQuad_].heAction =
            new SoHandleEventAction(quads_[activeQuad_].viewport);
    }

    SoHandleEventAction * hea = quads_[activeQuad_].heAction;
    hea->setViewportRegion(quads_[activeQuad_].viewport);
    hea->setEvent(event);
    hea->apply(quads_[activeQuad_].root);
    return hea->isHandled();
}
