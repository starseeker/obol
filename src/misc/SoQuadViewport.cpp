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
 * SoQuadViewport composes four SoViewport instances arranged in a 2×2 grid.
 * All non-trivial logic lives in SoViewport; this class only manages the
 * tile layout and shared-scene distribution.
 */

#include <Inventor/SoQuadViewport.h>

#include <Inventor/SoViewport.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/errors/SoDebugError.h>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SoQuadViewport::SoQuadViewport()
    : scene_(nullptr)
    , windowSize_(800, 600)
    , activeQuad_(0)
{
    updateViewports();
}

SoQuadViewport::~SoQuadViewport()
{
    // Remove the shared scene from all tiles before their destructors run,
    // so the scene's ref count is brought back down correctly.
    if (scene_) {
        for (int i = 0; i < NUM_QUADS; ++i)
            tiles_[i].setSceneGraph(nullptr);
        scene_->unref();
        scene_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Scene graph
// ---------------------------------------------------------------------------

void SoQuadViewport::setSceneGraph(SoNode * newScene)
{
    if (newScene == scene_)
        return;

    // Distribute the new scene to every tile.  SoViewport::setSceneGraph()
    // manages its own ref/unref, so we only hold an extra ref here to keep
    // the scene alive between setSceneGraph() calls even if all tiles are
    // temporarily set to nullptr.
    for (int i = 0; i < NUM_QUADS; ++i)
        tiles_[i].setSceneGraph(newScene);

    if (scene_)  scene_->unref();
    scene_ = newScene;
    if (scene_)  scene_->ref();
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
        return tiles_[0].getViewportRegion();
    }
    return tiles_[quad].getViewportRegion();
}

// Internal: assign sub-viewport regions to each tile.
//
// Layout (OpenGL origin at bottom-left, window W×H):
//   TOP_LEFT  (0): origin (0,   qH),  size (qW, qH)
//   TOP_RIGHT (1): origin (qW,  qH),  size (qW, qH)
//   BOT_LEFT  (2): origin (0,   0),   size (qW, qH)
//   BOT_RIGHT (3): origin (qW,  0),   size (qW, qH)
void SoQuadViewport::updateViewports()
{
    short W  = windowSize_[0];
    short H  = windowSize_[1];
    short qW = W / 2;
    short qH = H / 2;

    struct { short ox, oy; } origins[NUM_QUADS] = {
        { 0,  qH },   // TOP_LEFT
        { qW, qH },   // TOP_RIGHT
        { 0,  0  },   // BOTTOM_LEFT
        { qW, 0  }    // BOTTOM_RIGHT
    };

    for (int i = 0; i < NUM_QUADS; ++i) {
        SbViewportRegion vp;
        vp.setWindowSize(W, H);
        vp.setViewportPixels(SbVec2s(origins[i].ox, origins[i].oy),
                             SbVec2s(qW, qH));
        tiles_[i].setViewportRegion(vp);
    }
}

// ---------------------------------------------------------------------------
// Per-quadrant SoViewport access
// ---------------------------------------------------------------------------

SoViewport * SoQuadViewport::getViewport(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::getViewport",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return nullptr;
    }
    return &tiles_[quad];
}

const SoViewport * SoQuadViewport::getViewport(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::getViewport",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return nullptr;
    }
    return &tiles_[quad];
}

// ---------------------------------------------------------------------------
// Camera convenience wrappers
// ---------------------------------------------------------------------------

void SoQuadViewport::setCamera(int quad, SoCamera * camera)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::setCamera",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return;
    }
    tiles_[quad].setCamera(camera);
}

SoCamera * SoQuadViewport::getCamera(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS)
        return nullptr;
    return tiles_[quad].getCamera();
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
// View-all convenience wrappers
// ---------------------------------------------------------------------------

void SoQuadViewport::viewAll(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS)
        return;
    tiles_[quad].viewAll();
}

void SoQuadViewport::viewAllQuadrants()
{
    for (int i = 0; i < NUM_QUADS; ++i)
        tiles_[i].viewAll();
}

// ---------------------------------------------------------------------------
// Background colour convenience wrappers
// ---------------------------------------------------------------------------

void SoQuadViewport::setBackgroundColor(int quad, const SbColor & color)
{
    if (quad < 0 || quad >= NUM_QUADS)
        return;
    tiles_[quad].setBackgroundColor(color);
}

const SbColor & SoQuadViewport::getBackgroundColor(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS)
        return tiles_[0].getBackgroundColor();
    return tiles_[quad].getBackgroundColor();
}

// ---------------------------------------------------------------------------
// Rendering convenience wrapper
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::renderQuadrant(int quad, SoOffscreenRenderer * renderer)
{
    if (quad < 0 || quad >= NUM_QUADS || !renderer)
        return FALSE;
    return tiles_[quad].render(renderer);
}

// ---------------------------------------------------------------------------
// Event routing
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::processEvent(const SoEvent * event)
{
    if (!event)
        return FALSE;
    return tiles_[activeQuad_].processEvent(event);
}
