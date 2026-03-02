#ifndef OBOL_SOQUADVIEWPORT_H
#define OBOL_SOQUADVIEWPORT_H

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
 * @file SoQuadViewport.h
 * @brief Four-quadrant viewport manager for split-view 3D display.
 *
 * SoQuadViewport partitions a window into a 2×2 grid of independent camera
 * views that all render the same shared scene graph.  Each quadrant has its
 * own SoCamera, SbViewportRegion, and background colour.  One quadrant is
 * designated as "active" at any time for event routing and interaction.
 *
 * Level-of-Detail (SoLOD / SoLevelOfDetail) nodes in the shared scene are
 * automatically resolved per-quadrant based on each quadrant camera's
 * position, so zoomed-in views get higher-detail geometry while zoomed-out
 * views use coarser representations.
 *
 * Typical usage:
 * @code
 *   SoQuadViewport qv;
 *   qv.setWindowSize(SbVec2s(800, 600));
 *   qv.setSceneGraph(myGeometryScene);      // scene without cameras
 *
 *   qv.setCamera(SoQuadViewport::TOP_LEFT,  frontCam);
 *   qv.setCamera(SoQuadViewport::TOP_RIGHT, sideCam);
 *   qv.setCamera(SoQuadViewport::BOTTOM_LEFT,  topCam);
 *   qv.setCamera(SoQuadViewport::BOTTOM_RIGHT, perspCam);
 *   qv.viewAllQuadrants();
 *
 *   // Render quadrant 0 into a half-size offscreen renderer:
 *   SbVec2s qs = qv.getQuadrantSize();
 *   SoOffscreenRenderer ren(SbViewportRegion(qs[0], qs[1]));
 *   qv.renderQuadrant(SoQuadViewport::TOP_LEFT, &ren);
 * @endcode
 */

#include <Inventor/SbVec2s.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbViewportRegion.h>

class SoCamera;
class SoNode;
class SoSeparator;
class SoOffscreenRenderer;
class SoEvent;
class SoHandleEventAction;

class OBOL_DLL_API SoQuadViewport {
public:
    /** Total number of quadrants managed by this class. */
    static const int NUM_QUADS = 4;

    /**
     * Symbolic indices for the four quadrants.
     *
     * Layout (OpenGL origin at bottom-left):
     * @verbatim
     *  ┌──────────┬──────────┐
     *  │ TOP_LEFT │TOP_RIGHT │
     *  ├──────────┼──────────┤
     *  │BOT_LEFT  │BOT_RIGHT │
     *  └──────────┴──────────┘
     * @endverbatim
     */
    enum QuadIndex {
        TOP_LEFT     = 0,
        TOP_RIGHT    = 1,
        BOTTOM_LEFT  = 2,
        BOTTOM_RIGHT = 3
    };

    SoQuadViewport();
    ~SoQuadViewport();

    // ---- Scene graph -------------------------------------------------------

    /**
     * Set the shared scene graph rendered by all quadrants.
     * The scene should NOT contain cameras; cameras are supplied per-quadrant
     * via setCamera().  If the scene contains directional lights or other
     * camera-relative nodes, they apply independently in each quadrant.
     * Passing NULL removes the current scene from all quadrants.
     */
    void    setSceneGraph(SoNode * scene);
    SoNode* getSceneGraph() const;

    // ---- Window / viewport -------------------------------------------------

    /**
     * Set the full window size.  The four quadrant viewports are recomputed
     * as equal W/2 × H/2 tiles.
     */
    void          setWindowSize(const SbVec2s & size);
    SbVec2s       getWindowSize() const;

    /** Returns the pixel size of one quadrant (floor(W/2) × floor(H/2)). */
    SbVec2s       getQuadrantSize() const;

    /**
     * Returns the SbViewportRegion for quadrant @a quad.
     * The viewport region has window-size equal to getWindowSize() and a
     * sub-viewport corresponding to the quadrant's tile within the window.
     * This can be passed directly to SoGLRenderAction or SoOffscreenRenderer.
     */
    const SbViewportRegion& getViewportRegion(int quad) const;

    // ---- Cameras -----------------------------------------------------------

    /**
     * Set the camera for quadrant @a quad.
     * The previous camera (if any) is removed from the quadrant's scene root
     * and unreffed.  The new camera is ref'd and inserted as the first child
     * of the quadrant's internal scene root so it is applied before the
     * shared geometry scene.
     * Passing NULL removes the current camera without replacing it.
     */
    void       setCamera(int quad, SoCamera * camera);
    SoCamera * getCamera(int quad) const;

    // ---- Active quadrant ---------------------------------------------------

    /**
     * Select the active quadrant (0–3).  Events routed through processEvent()
     * are dispatched to this quadrant's SoHandleEventAction.
     */
    void setActiveQuadrant(int quad);
    int  getActiveQuadrant() const;

    // ---- View-all ----------------------------------------------------------

    /**
     * Adjust camera @a quad so the entire shared scene fits in the view.
     * Does nothing if no camera or no scene has been set for this quadrant.
     */
    void viewAll(int quad);

    /** Call viewAll() on every quadrant. */
    void viewAllQuadrants();

    // ---- Background colours ------------------------------------------------

    void           setBackgroundColor(int quad, const SbColor & color);
    const SbColor& getBackgroundColor(int quad) const;

    // ---- Rendering ---------------------------------------------------------

    /**
     * Render quadrant @a quad using the supplied SoOffscreenRenderer.
     * The renderer's viewport region is updated to the quadrant size before
     * rendering.  Background colour is applied to the renderer.
     *
     * @note  The caller is responsible for creating @a renderer with an
     *        appropriate context manager.  A renderer of size getQuadrantSize()
     *        is sufficient.
     *
     * @return TRUE if the render succeeded, FALSE otherwise.
     */
    SbBool renderQuadrant(int quad, SoOffscreenRenderer * renderer);

    // ---- Event routing -----------------------------------------------------

    /**
     * Dispatch @a event to the active quadrant's SoHandleEventAction.
     * @return TRUE if the event was handled by some node in the scene.
     */
    SbBool processEvent(const SoEvent * event);

private:
    SoQuadViewport(const SoQuadViewport &);            // not copyable
    SoQuadViewport & operator=(const SoQuadViewport &);

    void updateViewports();

    struct QuadState {
        SoSeparator *     root;       // internal root: [camera] [scene]
        SoCamera *        camera;     // owned (ref'd), may be NULL
        SbViewportRegion  viewport;   // sub-viewport within the full window
        SbColor           bgColor;
        SoHandleEventAction * heAction;
    };

    QuadState    quads_[NUM_QUADS];
    SoNode *     scene_;             // shared geometry (ref'd), may be NULL
    SbVec2s      windowSize_;
    int          activeQuad_;
};

#endif // !OBOL_SOQUADVIEWPORT_H
