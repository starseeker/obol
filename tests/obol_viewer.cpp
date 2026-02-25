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
 * @file obol_viewer.cpp
 * @brief FLTK GUI viewer for Obol test scenes.
 *
 * Layout:
 *   +--250px--+------- remaining width -------+
 *   | Test    | "System GL"  | "OSMesa"        |
 *   | browser |  CoinGLWidget|  CoinOSMesaWidget|
 *   |         |  (left half) |  (right half)   |
 *   +---------+--------------+-----------------+
 *   | [Run] [Render] [Help]   [x] Sync Views   |
 *   +---------------------------------------------+
 *
 * System GL widget: renders via SoGLRenderAction in an Fl_Gl_Window.
 * OSMesa widget   : renders via SoOffscreenRenderer (OSMesa context),
 *                   then displays the result as an Fl_RGB_Image inside
 *                   an Fl_Box.
 *
 * Both widgets share the same SoSeparator scene root produced by the
 * registry's create_scene factory, and each manages its own
 * SoPerspectiveCamera so the two views can be compared independently
 * (or kept in sync when "Sync Views" is checked).
 *
 * Camera interaction (both widgets):
 *   Left-drag   : orbit (trackball)
 *   Right-drag  : dolly (zoom in/out along view axis)
 *   Scroll      : zoom
 */

#include "testlib/test_registry.h"
#include "utils/headless_utils.h"

// FLTK
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

// OpenGL (system GL for CoinGLWidget)
#ifdef COIN3D_OSMESA_BUILD
#  include <OSMesa/gl.h>
#else
#  ifdef __unix__
#    include <GL/gl.h>
#  else
#    include <OpenGL/gl.h>
#  endif
#endif

// Coin
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>

// OSMesa for the offscreen widget
#ifdef COIN3D_OSMESA_BUILD
#  include <OSMesa/osmesa.h>
#else
// When building without OSMesa as the primary backend, we still attempt to
// use the system OSMesa if available.  The widget degrades gracefully if
// OSMesa is not present at link time.
#  ifdef HAVE_OSMESA
#    include <GL/osmesa.h>
#  endif
#endif

#include <cstdio>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// =========================================================================
// Forward declarations
// =========================================================================
class CoinGLWidget;
class CoinOSMesaWidget;
class ObolViewerWindow;

// =========================================================================
// Camera orbit helper (shared by both widgets)
// =========================================================================
namespace {

void orbitCamera(SoPerspectiveCamera* cam, float dAzimuth, float dElevation)
{
    if (!cam) return;
    const SbVec3f center(0.0f, 0.0f, 0.0f);
    SbVec3f offset = cam->position.getValue() - center;

    // Azimuth: rotate around world Y
    SbRotation azRot(SbVec3f(0,1,0), dAzimuth);
    azRot.multVec(offset, offset);

    // Elevation: rotate around the camera's right axis
    SbVec3f viewDir = -offset; viewDir.normalize();
    SbVec3f worldUp(0,1,0);
    SbVec3f rightVec = worldUp.cross(viewDir);
    float rLen = rightVec.length();
    if (rLen > 1e-4f) {
        rightVec *= (1.0f / rLen);
        SbRotation elRot(rightVec, dElevation);
        elRot.multVec(offset, offset);
    }

    cam->position.setValue(center + offset);
    cam->pointAt(center, SbVec3f(0,1,0));
}

void dollyCamera(SoPerspectiveCamera* cam, float delta)
{
    if (!cam) return;
    SbVec3f pos = cam->position.getValue();
    float dist = pos.length();
    dist *= (1.0f + delta);
    if (dist < 0.01f) dist = 0.01f;
    pos.normalize();
    cam->position.setValue(pos * dist);
}

} // anonymous namespace

// =========================================================================
// CoinGLWidget – renders scene into an Fl_Gl_Window via SoGLRenderAction
// =========================================================================
class CoinGLWidget : public Fl_Gl_Window {
public:
    SoSeparator*         scene_root_ = nullptr;
    SoPerspectiveCamera* camera_     = nullptr;
    // Callback invoked after each draw (used by "Sync Views")
    std::function<void(SoPerspectiveCamera*)> on_camera_changed_;

    CoinGLWidget(int x, int y, int w, int h, const char* label = nullptr)
        : Fl_Gl_Window(x, y, w, h, label)
    {
        mode(FL_RGB | FL_DOUBLE | FL_DEPTH);
    }

    void setScene(SoSeparator* root, SoPerspectiveCamera* cam) {
        scene_root_ = root;
        camera_     = cam;
        valid(0);
        redraw();
    }

    void draw() override {
        if (!valid()) {
            glViewport(0, 0, pixel_w(), pixel_h());
            valid(1);
        }
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (scene_root_) {
            SbViewportRegion vp(pixel_w(), pixel_h());
            SoGLRenderAction action(vp);
            action.setTransparencyType(SoGLRenderAction::BLEND);
            action.apply(scene_root_);
        }
    }

    int handle(int event) override {
        static int  last_x = 0, last_y = 0;
        static bool left_down = false, right_down = false;

        switch (event) {
        case FL_PUSH:
            last_x = Fl::event_x();
            last_y = Fl::event_y();
            if (Fl::event_button() == FL_LEFT_MOUSE)  left_down  = true;
            if (Fl::event_button() == FL_RIGHT_MOUSE) right_down = true;
            return 1;

        case FL_RELEASE:
            if (Fl::event_button() == FL_LEFT_MOUSE)  left_down  = false;
            if (Fl::event_button() == FL_RIGHT_MOUSE) right_down = false;
            return 1;

        case FL_DRAG:
            if (camera_) {
                int dx = Fl::event_x() - last_x;
                int dy = Fl::event_y() - last_y;
                if (left_down) {
                    orbitCamera(camera_,
                                -dx * 0.01f,
                                -dy * 0.01f);
                } else if (right_down) {
                    dollyCamera(camera_, dy * 0.01f);
                }
                if (on_camera_changed_) on_camera_changed_(camera_);
                redraw();
            }
            last_x = Fl::event_x();
            last_y = Fl::event_y();
            return 1;

        case FL_MOUSEWHEEL:
            if (camera_) {
                dollyCamera(camera_, Fl::event_dy() * 0.1f);
                if (on_camera_changed_) on_camera_changed_(camera_);
                redraw();
            }
            return 1;

        default:
            return Fl_Gl_Window::handle(event);
        }
    }
};

// =========================================================================
// CoinOSMesaWidget – renders via OSMesa and displays as Fl_RGB_Image
// =========================================================================
class CoinOSMesaWidget : public Fl_Box {
public:
    SoSeparator*         scene_root_ = nullptr;
    SoPerspectiveCamera* camera_     = nullptr;
    std::function<void(SoPerspectiveCamera*)> on_camera_changed_;

    CoinOSMesaWidget(int x, int y, int wi, int hi, const char* label = nullptr)
        : Fl_Box(x, y, wi, hi, label), buf_w_(0), buf_h_(0)
    {
        box(FL_FLAT_BOX);
        color(fl_rgb_color(30, 30, 30));
#if defined(COIN3D_OSMESA_BUILD) || defined(HAVE_OSMESA)
        mesa_ctx_ = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, nullptr);
#else
        mesa_ctx_ = nullptr;
#endif
    }

    ~CoinOSMesaWidget() {
#if defined(COIN3D_OSMESA_BUILD) || defined(HAVE_OSMESA)
        if (mesa_ctx_) OSMesaDestroyContext(mesa_ctx_);
#endif
        delete[] pixel_buf_;
        delete   display_image_;
    }

    void setScene(SoSeparator* root, SoPerspectiveCamera* cam) {
        scene_root_ = root;
        camera_     = cam;
        refresh();
    }

    /** Re-render the scene and update the displayed image. */
    void refresh() {
        if (!scene_root_) { redraw(); return; }

        int rw = w() > 0 ? w() : 800;
        int rh = h() > 0 ? h() : 600;

        ensureBuffer(rw, rh);

#if defined(COIN3D_OSMESA_BUILD) || defined(HAVE_OSMESA)
        if (!mesa_ctx_) { redraw(); return; }

        if (!OSMesaMakeCurrent(mesa_ctx_, pixel_buf_,
                               GL_UNSIGNED_BYTE, rw, rh)) {
            fprintf(stderr, "CoinOSMesaWidget: OSMesaMakeCurrent failed\n");
            redraw(); return;
        }

        glViewport(0, 0, rw, rh);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            SbViewportRegion vp(rw, rh);
            SoGLRenderAction action(vp);
            action.setTransparencyType(SoGLRenderAction::BLEND);
            action.apply(scene_root_);
        }

        glFlush();
        OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);

        // Build Fl_RGB_Image from RGBA buffer (strip alpha, flip vertically)
        int stride = rw * 4;
        std::unique_ptr<unsigned char[]> rgb(new unsigned char[rw * rh * 3]);
        for (int row = 0; row < rh; ++row) {
            // OSMesa origin is bottom-left; FLTK origin is top-left
            const unsigned char* src = pixel_buf_ + (rh - 1 - row) * stride;
            unsigned char*       dst = rgb.get()  + row * rw * 3;
            for (int col = 0; col < rw; ++col) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                src += 4; dst += 3;
            }
        }

        delete display_image_;
        display_image_ = new Fl_RGB_Image(rgb.release(), rw, rh, 3);
        display_image_->alloc_array = 1;
        image(display_image_);
#else
        // No OSMesa: show placeholder text
        label("OSMesa not available");
#endif
        redraw();
    }

    int handle(int event) override {
        static int  last_x = 0, last_y = 0;
        static bool left_down = false, right_down = false;

        switch (event) {
        case FL_PUSH:
            last_x = Fl::event_x();
            last_y = Fl::event_y();
            if (Fl::event_button() == FL_LEFT_MOUSE)  left_down  = true;
            if (Fl::event_button() == FL_RIGHT_MOUSE) right_down = true;
            return 1;

        case FL_RELEASE:
            if (Fl::event_button() == FL_LEFT_MOUSE)  left_down  = false;
            if (Fl::event_button() == FL_RIGHT_MOUSE) right_down = false;
            return 1;

        case FL_DRAG:
            if (camera_) {
                int dx = Fl::event_x() - last_x;
                int dy = Fl::event_y() - last_y;
                if (left_down) {
                    orbitCamera(camera_, -dx * 0.01f, -dy * 0.01f);
                } else if (right_down) {
                    dollyCamera(camera_, dy * 0.01f);
                }
                if (on_camera_changed_) on_camera_changed_(camera_);
                refresh();
            }
            last_x = Fl::event_x();
            last_y = Fl::event_y();
            return 1;

        case FL_MOUSEWHEEL:
            if (camera_) {
                dollyCamera(camera_, Fl::event_dy() * 0.1f);
                if (on_camera_changed_) on_camera_changed_(camera_);
                refresh();
            }
            return 1;

        default:
            return Fl_Box::handle(event);
        }
    }

private:
    void ensureBuffer(int bw, int bh) {
        if (bw == buf_w_ && bh == buf_h_) return;
        delete[] pixel_buf_;
        pixel_buf_ = new unsigned char[bw * bh * 4]();
        buf_w_ = bw; buf_h_ = bh;
    }

#if defined(COIN3D_OSMESA_BUILD) || defined(HAVE_OSMESA)
    OSMesaContext mesa_ctx_    = nullptr;
#else
    void*         mesa_ctx_    = nullptr;
#endif
    unsigned char* pixel_buf_  = nullptr;
    int            buf_w_ = 0, buf_h_ = 0;
    Fl_RGB_Image*  display_image_ = nullptr;
};

// =========================================================================
// ObolViewerWindow – top-level window wiring everything together
// =========================================================================
class ObolViewerWindow : public Fl_Window {
public:
    ObolViewerWindow(int w, int h)
        : Fl_Window(w, h, "Obol Test Viewer")
    {
        begin();

        const int BROWSER_W  = 250;
        const int TOOLBAR_H  = 40;
        const int view_w     = w - BROWSER_W;
        const int view_h     = h - TOOLBAR_H;
        const int half_view  = view_w / 2;

        // ---- Test browser (left) ----------------------------------------
        browser_ = new Fl_Hold_Browser(0, 0, BROWSER_W, view_h, nullptr);
        browser_->textsize(12);
        browser_->callback(browserSelectCB, this);
        populateBrowser();

        // ---- Label bar (above GL views) ---------------------------------
        Fl_Box* gl_label = new Fl_Box(BROWSER_W, 0,
                                       half_view, 20, "System GL");
        gl_label->labelsize(11); gl_label->align(FL_ALIGN_CENTER);

        Fl_Box* os_label = new Fl_Box(BROWSER_W + half_view, 0,
                                       view_w - half_view, 20, "OSMesa");
        os_label->labelsize(11); os_label->align(FL_ALIGN_CENTER);

        // ---- GL widget (left half of view area) -------------------------
        gl_widget_ = new CoinGLWidget(BROWSER_W, 20,
                                       half_view, view_h - 20, nullptr);
        gl_widget_->on_camera_changed_ = [this](SoPerspectiveCamera* cam) {
            if (sync_views_ && osmesa_widget_->camera_) {
                // Copy camera state to OSMesa widget's camera
                osmesa_widget_->camera_->position.setValue(cam->position.getValue());
                osmesa_widget_->camera_->orientation.setValue(cam->orientation.getValue());
                osmesa_widget_->refresh();
            }
        };

        // ---- OSMesa widget (right half of view area) --------------------
        osmesa_widget_ = new CoinOSMesaWidget(BROWSER_W + half_view, 20,
                                               view_w - half_view, view_h - 20,
                                               nullptr);
        osmesa_widget_->on_camera_changed_ = [this](SoPerspectiveCamera* cam) {
            if (sync_views_ && gl_widget_->camera_) {
                gl_widget_->camera_->position.setValue(cam->position.getValue());
                gl_widget_->camera_->orientation.setValue(cam->orientation.getValue());
                gl_widget_->redraw();
            }
        };

        // ---- Toolbar (bottom) -------------------------------------------
        Fl_Group* toolbar = new Fl_Group(0, view_h, w, TOOLBAR_H);
        toolbar->box(FL_UP_BOX);

        Fl_Button* btn_run = new Fl_Button(5, view_h + 6, 70, 28, "Run");
        btn_run->callback(runCB, this);

        Fl_Button* btn_render = new Fl_Button(80, view_h + 6, 90, 28, "Render...");
        btn_render->callback(renderCB, this);

        Fl_Button* btn_help = new Fl_Button(175, view_h + 6, 60, 28, "Help");
        btn_help->callback(helpCB, this);

        sync_check_ = new Fl_Check_Button(250, view_h + 6, 120, 28, "Sync Views");
        sync_check_->value(0);
        sync_check_->callback(syncCB, this);

        toolbar->end();
        end();
        resizable(this);
    }

    ~ObolViewerWindow() {
        clearScene();
    }

private:
    Fl_Hold_Browser*     browser_      = nullptr;
    CoinGLWidget*        gl_widget_    = nullptr;
    CoinOSMesaWidget*    osmesa_widget_= nullptr;
    Fl_Check_Button*     sync_check_  = nullptr;

    SoSeparator*         scene_root_  = nullptr;
    SoPerspectiveCamera* cam_gl_      = nullptr;
    SoPerspectiveCamera* cam_osmesa_  = nullptr;
    bool                 sync_views_  = false;

    std::vector<std::string> test_names_;  // parallel to browser lines

    // ---- Populate browser from registry ---------------------------------
    void populateBrowser() {
        const auto& tests = ObolTest::TestRegistry::instance().allTests();
        for (const auto& e : tests) {
            if (!e.has_visual && !e.create_scene) continue;
            std::string line = e.name + "  (" +
                ObolTest::categoryToString(e.category) + ")";
            browser_->add(line.c_str());
            test_names_.push_back(e.name);
        }
    }

    // ---- Load the selected test into both widgets -----------------------
    void loadSelectedTest() {
        int idx = browser_->value();
        if (idx < 1 || idx > (int)test_names_.size()) return;

        const std::string& name = test_names_[idx - 1];
        const ObolTest::TestEntry* entry =
            ObolTest::TestRegistry::instance().findTest(name);
        if (!entry || !entry->create_scene) return;

        clearScene();

        // Create scene for GL widget
        scene_root_ = entry->create_scene(gl_widget_->w(), gl_widget_->h());

        // Build independent cameras for each view
        cam_gl_ = new SoPerspectiveCamera;
        cam_gl_->ref();
        {
            SbViewportRegion vp(gl_widget_->w(), gl_widget_->h());
            cam_gl_->viewAll(scene_root_, vp);
        }

        cam_osmesa_ = new SoPerspectiveCamera;
        cam_osmesa_->ref();
        cam_osmesa_->position.setValue(cam_gl_->position.getValue());
        cam_osmesa_->orientation.setValue(cam_gl_->orientation.getValue());
        cam_osmesa_->nearDistance.setValue(cam_gl_->nearDistance.getValue());
        cam_osmesa_->farDistance.setValue(cam_gl_->farDistance.getValue());
        cam_osmesa_->focalDistance.setValue(cam_gl_->focalDistance.getValue());

        gl_widget_->setScene(scene_root_, cam_gl_);
        osmesa_widget_->setScene(scene_root_, cam_osmesa_);
    }

    void clearScene() {
        gl_widget_->setScene(nullptr, nullptr);
        osmesa_widget_->setScene(nullptr, nullptr);
        if (cam_gl_)     { cam_gl_->unref();     cam_gl_     = nullptr; }
        if (cam_osmesa_) { cam_osmesa_->unref();  cam_osmesa_ = nullptr; }
        if (scene_root_) { scene_root_->unref();  scene_root_ = nullptr; }
    }

    // ---- Render selected test to file -----------------------------------
    void renderSelected() {
        int idx = browser_->value();
        if (idx < 1 || idx > (int)test_names_.size()) {
            fl_alert("Please select a test first.");
            return;
        }
        const std::string& name = test_names_[idx - 1];
        std::string outpath = name + "_viewer_output.rgb";
        bool ok = ObolTest::TestRegistry::instance().renderTestToFile(
            name, outpath, 800, 600);
        if (ok)
            fl_message("Rendered to: %s", outpath.c_str());
        else
            fl_alert("Render failed for test: %s", name.c_str());
    }

    // ---- Run unit test for selected entry --------------------------------
    void runSelected() {
        int idx = browser_->value();
        if (idx < 1 || idx > (int)test_names_.size()) {
            fl_alert("Please select a test first.");
            return;
        }
        const std::string& name = test_names_[idx - 1];
        const ObolTest::TestEntry* entry =
            ObolTest::TestRegistry::instance().findTest(name);
        if (!entry) return;

        if (entry->run_unit) {
            int rc = entry->run_unit();
            if (rc == 0)
                fl_message("PASS: %s", name.c_str());
            else
                fl_alert("FAIL: %s (rc=%d)", name.c_str(), rc);
        } else {
            // Visual-only: just reload the scene
            loadSelectedTest();
        }
    }

    // ---- Static FLTK callbacks ------------------------------------------
    static void browserSelectCB(Fl_Widget*, void* data) {
        static_cast<ObolViewerWindow*>(data)->loadSelectedTest();
    }
    static void runCB(Fl_Widget*, void* data) {
        static_cast<ObolViewerWindow*>(data)->runSelected();
    }
    static void renderCB(Fl_Widget*, void* data) {
        static_cast<ObolViewerWindow*>(data)->renderSelected();
    }
    static void helpCB(Fl_Widget*, void*) {
        fl_message(
            "Obol Test Viewer\n\n"
            "  Left-drag  : orbit camera\n"
            "  Right-drag : dolly (zoom)\n"
            "  Scroll     : zoom\n\n"
            "  Run    : execute the selected test's unit test\n"
            "  Render : render scene to an .rgb file\n"
            "  Sync   : keep both cameras synchronised");
    }
    static void syncCB(Fl_Widget* w, void* data) {
        ObolViewerWindow* self = static_cast<ObolViewerWindow*>(data);
        self->sync_views_ = (static_cast<Fl_Check_Button*>(w)->value() != 0);
    }
};

// =========================================================================
// main
// =========================================================================
int main(int argc, char** argv)
{
    initCoinHeadless();

    ObolViewerWindow* win = new ObolViewerWindow(1100, 700);
    win->show(argc, argv);
    return Fl::run();
}
