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
 * @file SoViewport.cpp
 * @brief Implementation of the SoViewport single-viewport manager.
 *
 * Manages one internal SoSeparator root containing (optionally) a camera
 * followed by the scene graph.  Multiple SoViewport instances may share the
 * same scene graph node; Inventor's reference counting handles this cleanly.
 */

#include <Inventor/SoViewport.h>

#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/misc/SoChildList.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/errors/SoDebugError.h>

#include <algorithm>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SoViewport::SoViewport()
    : root_(new SoSeparator)
    , camera_(nullptr)
    , scene_(nullptr)
    , viewport_(SbVec2s(800, 600))
    , bgColor_(0.0f, 0.0f, 0.0f)
    , heAction_(nullptr)
{
    root_->ref();
}

SoViewport::~SoViewport()
{
    if (scene_) {
        if (root_->findChild(scene_) >= 0)
            root_->removeChild(scene_);
        scene_->unref();
        scene_ = nullptr;
    }
    if (camera_) {
        if (root_->findChild(camera_) >= 0)
            root_->removeChild(camera_);
        camera_->unref();
        camera_ = nullptr;
    }
    delete heAction_;
    heAction_ = nullptr;

    root_->unref();
    root_ = nullptr;
}

// ---------------------------------------------------------------------------
// Scene graph
// ---------------------------------------------------------------------------

void SoViewport::setSceneGraph(SoNode * newScene)
{
    if (newScene == scene_)
        return;

    if (scene_) {
        if (root_->findChild(scene_) >= 0)
            root_->removeChild(scene_);
        scene_->unref();
    }

    scene_ = newScene;

    if (scene_) {
        scene_->ref();
        root_->addChild(scene_);
    }
}

SoNode * SoViewport::getSceneGraph() const
{
    return scene_;
}

// ---------------------------------------------------------------------------
// Viewport region
// ---------------------------------------------------------------------------

void SoViewport::setViewportRegion(const SbViewportRegion & region)
{
    viewport_ = region;
    // Invalidate the handle-event action so it is recreated with the new
    // viewport on the next processEvent() call.
    delete heAction_;
    heAction_ = nullptr;
}

const SbViewportRegion & SoViewport::getViewportRegion() const
{
    return viewport_;
}

void SoViewport::setWindowSize(const SbVec2s & size)
{
    // Viewport = full window.
    SbViewportRegion vp(size[0], size[1]);
    setViewportRegion(vp);
}

SbVec2s SoViewport::getWindowSize() const
{
    return viewport_.getWindowSize();
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

void SoViewport::setCamera(SoCamera * camera)
{
    std::unique_ptr<CameraReplacement> replacement =
        this->prepareCameraReplacement(camera);
    if (!replacement) return;
    replacement->commit();
    replacement->notify();
}

class SoViewport::CameraReplacement::Impl {
public:
    Impl(SoViewport &target, SoCamera *camera, int requestedIndex)
        : viewport(target), previous(target.camera_), next(camera)
    {
        std::vector<SoNode *> children;
        children.reserve(static_cast<size_t>(target.root_->getNumChildren()) +
            (camera ? 1u : 0u));
        for (int i = 0; i < target.root_->getNumChildren(); ++i) {
            SoNode *child = target.root_->getChild(i);
            if (child == this->previous || child == this->next)
                continue;
            children.push_back(child);
        }
        if (this->next) {
            const size_t index = std::min(
                static_cast<size_t>(std::max(requestedIndex, 0)),
                children.size());
            children.insert(children.begin() +
                static_cast<std::ptrdiff_t>(index), this->next);
        }
        this->root = target.root_->getChildren()->prepareReplacement(children);
        if (this->next) this->next->ref();
    }

    ~Impl()
    {
        if (this->committed) {
            if (this->previous) this->previous->unref();
        } else if (this->next) {
            this->next->unref();
        }
    }

    void commit()
    {
        if (this->committed) return;
        this->root->commit();
        this->viewport.camera_ = this->next;
        this->committed = true;
    }

    void notify()
    {
        if (!this->committed || this->notified) return;
        this->notified = true;
        this->root->notify();
    }

private:
    SoViewport &viewport;
    SoCamera *previous;
    SoCamera *next;
    std::unique_ptr<SoChildList::Replacement> root;
    bool committed = false;
    bool notified = false;
};

SoViewport::CameraReplacement::CameraReplacement(std::unique_ptr<Impl> state)
    : impl(std::move(state))
{
}

SoViewport::CameraReplacement::~CameraReplacement() = default;

void SoViewport::CameraReplacement::commit()
{
    this->impl->commit();
}

void SoViewport::CameraReplacement::notify()
{
    this->impl->notify();
}

std::unique_ptr<SoViewport::CameraReplacement>
SoViewport::prepareCameraReplacement(SoCamera *camera, int rootIndex)
{
    if (camera == this->camera_) return nullptr;
    auto state = std::make_unique<CameraReplacement::Impl>(
        *this, camera, rootIndex);
    return std::unique_ptr<CameraReplacement>(
        new CameraReplacement(std::move(state)));
}

SoCamera * SoViewport::getCamera() const
{
    return camera_;
}

// ---------------------------------------------------------------------------
// View-all
// ---------------------------------------------------------------------------

void SoViewport::viewAll()
{
    if (!camera_ || !scene_)
        return;
    camera_->viewAll(root_, viewport_);
}

// ---------------------------------------------------------------------------
// Background colour
// ---------------------------------------------------------------------------

void SoViewport::setBackgroundColor(const SbColor & color)
{
    bgColor_ = color;
}

const SbColor & SoViewport::getBackgroundColor() const
{
    return bgColor_;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

SbBool SoViewport::render(SoOffscreenRenderer * renderer)
{
    if (!renderer)
        return FALSE;
    renderer->setViewportRegion(viewport_);
    renderer->setBackgroundColor(bgColor_);
    return renderer->render(root_);
}

// ---------------------------------------------------------------------------
// Event routing
// ---------------------------------------------------------------------------

SbBool SoViewport::processEvent(const SoEvent * event)
{
    if (!event)
        return FALSE;

    if (!heAction_)
        heAction_ = new SoHandleEventAction(viewport_);

    heAction_->setViewportRegion(viewport_);
    heAction_->setEvent(event);
    heAction_->apply(root_);
    return heAction_->isHandled();
}

// ---------------------------------------------------------------------------
// Internal root access
// ---------------------------------------------------------------------------

SoSeparator * SoViewport::getRoot() const
{
    return root_;
}
