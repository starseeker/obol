/*
 * obol_viewer.cpp  –  FLTK triple-backend scene viewer for Obol
 *
 * Architecture
 * ────────────
 * The viewer loads up to three bridge shared libraries at startup:
 *
 *   libobol_bridge_sys.so    – Obol built against system OpenGL / GLX
 *   libobol_bridge_osmesa.so – Obol built against OSMesa (headless)
 *   libobol_bridge_nanort.so – Obol with NanoRT CPU raytracing (no GL)
 *
 * Both bridges export the same C API (obol_bridge.h), and each bridge
 * embeds its corresponding Obol static library with -fvisibility=hidden
 * so the C++ symbols are private.  The viewer loads them with
 * dlopen(RTLD_LOCAL) so even the visible obol_* C functions live in
 * separate namespaces and never collide.
 *
 * If libobol_bridge_sys.so is unavailable (no DISPLAY, missing libGL,
 * CI headless runner) the viewer falls back to using the OSMesa bridge
 * for both panels and shows a status message.
 * The NanoRT panel is omitted when libobol_bridge_nanort.so is not found.
 *
 * Layout
 * ──────
 *   ┌─────────┬──────────────────────────────────────────────────┐
 *   │  Test   │  [System GL]  │  [OSMesa]  │  [NanoRT]           │
 *   │ browser │  CoinPanel    │  CoinPanel │  CoinPanel (if avail)│
 *   ├─────────┴───────────────┴────────────┴─────────────────────┤
 *   │ [Reload] [Render…]  [×] Sync views  status text            │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * Interaction (all panels)
 *   Left-drag   → orbit camera
 *   Right-drag  → dolly (zoom along view axis)
 *   Scroll      → zoom
 *   When "Sync views" is checked, moving the camera in any panel
 *   immediately mirrors to the others.
 *
 * Building
 * ────────
 * This file is built as part of tests/obol_superbuild/ (see that
 * directory's CMakeLists.txt).  The superbuild compiles all Obol
 * variants and the bridge libraries, then builds this file linking
 * only FLTK and libdl.
 *
 * The superbuild passes -DOBOL_BRIDGE_DIR=<path> so the viewer knows
 * where to find the bridge .so files.  A fallback search path (directory
 * of the executable) is also tried.
 */

#include "obol_bridge.h"   /* C API + ObolBridgeAPI struct */

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Scroll.H>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>

#ifndef OBOL_BRIDGE_DIR
#  define OBOL_BRIDGE_DIR ""
#endif

/* =========================================================================
 * Bridge discovery helpers
 * ======================================================================= */

/** Try to load a bridge .so from several candidate paths. */
static ObolBridgeAPI* try_load_bridge(const char* basename,
                                      const char* extra_dir = nullptr)
{
    std::vector<std::string> candidates;
    /* 0. environment variable override (highest priority) */
    const char* env_dir = getenv("OBOL_BRIDGE_DIR");
    if (env_dir && env_dir[0]) {
        candidates.push_back(std::string(env_dir) + "/" + basename);
    }
    /* 1. explicit build-time bridge dir */
    if (OBOL_BRIDGE_DIR[0]) {
        candidates.push_back(std::string(OBOL_BRIDGE_DIR) + "/" + basename);
    }
    /* 2. caller-supplied extra dir (e.g. directory of the executable) */
    if (extra_dir && extra_dir[0]) {
        candidates.push_back(std::string(extra_dir) + "/" + basename);
    }
    /* 3. system default (LD_LIBRARY_PATH etc.) */
    candidates.push_back(basename);

    for (const auto& path : candidates) {
        ObolBridgeAPI* api = ObolBridgeAPI::load(path.c_str());
        if (api) {
            fprintf(stdout, "obol_viewer: loaded bridge '%s'\n", path.c_str());
            return api;
        }
        /* Print dlerror for debugging */
        fprintf(stderr, "obol_viewer: could not load '%s': %s\n",
                path.c_str(), dlerror());
    }
    return nullptr;
}

/* =========================================================================
 * CoinPanel  –  one render panel backed by a bridge instance
 * ======================================================================= */

/**
 * CoinPanel is an Fl_Box that:
 *  • holds an ObolBridgeAPI* and an ObolScene handle
 *  • renders the scene via bridge->fn_scene_render() on every draw()
 *  • forwards FLTK mouse / scroll / key events to the bridge
 *  • exposes camera state read/write for the Sync Views feature
 */
class CoinPanel : public Fl_Box {
public:
    ObolBridgeAPI* bridge   = nullptr;
    ObolScene      scene    = nullptr;
    std::string    label_text;
    std::string    status_text;

    /* Raw pixel buffer (RGBA, bottom-up from GL convention) */
    std::vector<uint8_t> pixel_buf;
    /* FLTK image object (top-down RGB) created from pixel_buf */
    Fl_RGB_Image*   fltk_img = nullptr;
    /* Separate display buffer: FLTK wants top-down RGB (3 channels) */
    std::vector<uint8_t> display_buf;

    /* Sync callback: called after local camera changes */
    std::function<void(CoinPanel*)> on_camera_changed;

    explicit CoinPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~CoinPanel() {
        destroyScene();
        delete fltk_img; fltk_img = nullptr;
    }

    /* ------------------------------------------------------------------
     * Scene management
     * ------------------------------------------------------------------ */

    void createScene(const char* name) {
        destroyScene();
        if (!bridge) return;
        scene = bridge->fn_scene_create(name, w(), h());
        if (!scene) {
            status_text = std::string("Failed to create scene '") + name + "'";
        } else {
            status_text = "";
        }
        refreshRender();
    }

    void destroyScene() {
        if (scene && bridge) { bridge->fn_scene_destroy(scene); scene = nullptr; }
        delete fltk_img; fltk_img = nullptr;
    }

    /* Render scene via bridge, update FLTK image, redraw widget. */
    void refreshRender() {
        if (!scene || !bridge) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        pixel_buf.resize((size_t)pw * ph * 4);
        if (bridge->fn_scene_render(scene, pixel_buf.data(), pw, ph) != 0) {
            status_text = "Render failed";
            redraw();
            return;
        }
        /* Convert bottom-up RGBA → top-down RGB for FLTK */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* src = pixel_buf.data() + (size_t)(ph - 1 - row) * pw * 4;
            uint8_t*       dst = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
                src += 4; dst += 3;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        redraw();
    }

    /* ------------------------------------------------------------------
     * Camera state sync helpers
     * ------------------------------------------------------------------ */

    void getCamera(float pos[3], float orient[4], float& dist) const {
        if (scene && bridge)
            bridge->fn_scene_get_camera(scene, pos, orient, &dist);
    }

    void setCamera(const float pos[3], const float orient[4], float dist) {
        if (scene && bridge) {
            bridge->fn_scene_set_camera(scene, pos, orient, dist);
            refreshRender();
        }
    }

    /* ------------------------------------------------------------------
     * FLTK overrides
     * ------------------------------------------------------------------ */

    void draw() override {
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img) {
            fltk_img->draw(x(), y(), w(), h(), 0, 0);
        } else {
            /* No scene loaded: draw a placeholder message */
            fl_color(FL_WHITE);
            fl_font(FL_HELVETICA, 14);
            fl_draw("(no scene loaded)", x() + w()/2 - 60, y() + h()/2);
        }
        /* Backend label top-left */
        if (!label_text.empty()) {
            fl_color(fl_rgb_color(255,255,100));
            fl_font(FL_HELVETICA_BOLD, 13);
            fl_draw(label_text.c_str(), x()+6, y()+16);
        }
        /* Status text bottom-left */
        if (!status_text.empty()) {
            fl_color(fl_rgb_color(255,80,80));
            fl_font(FL_HELVETICA, 12);
            fl_draw(status_text.c_str(), x()+6, y()+h()-6);
        }
    }

    int handle(int event) override {
        if (!scene || !bridge) return Fl_Box::handle(event);

        switch (event) {
        case FL_PUSH:
            take_focus();
            bridge->fn_scene_mouse_press(scene,
                Fl::event_x() - x(), Fl::event_y() - y(),
                Fl::event_button());
            if (Fl::event_button() != FL_MIDDLE_MOUSE)
                return 1; /* consume for drag */
            break;
        case FL_RELEASE:
            bridge->fn_scene_mouse_release(scene,
                Fl::event_x() - x(), Fl::event_y() - y(),
                Fl::event_button());
            notifyCameraChanged();
            refreshRender();
            return 1;
        case FL_DRAG:
            bridge->fn_scene_mouse_move(scene,
                Fl::event_x() - x(), Fl::event_y() - y());
            notifyCameraChanged();
            refreshRender();
            return 1;
        case FL_MOUSEWHEEL:
            bridge->fn_scene_scroll(scene, -(float)Fl::event_dy());
            notifyCameraChanged();
            refreshRender();
            return 1;
        case FL_KEYDOWN:
            bridge->fn_scene_key_press(scene, Fl::event_key());
            refreshRender();
            return 1;
        case FL_KEYUP:
            bridge->fn_scene_key_release(scene, Fl::event_key());
            return 1;
        case FL_FOCUS: case FL_UNFOCUS:
            return 1;
        default: break;
        }
        return Fl_Box::handle(event);
    }

    void resize(int X, int Y, int W, int H) override {
        Fl_Box::resize(X, Y, W, H);
        if (scene && bridge) bridge->fn_scene_resize(scene, W, H);
        refreshRender();
    }

private:
    void notifyCameraChanged() {
        if (on_camera_changed) on_camera_changed(this);
    }
};

/* =========================================================================
 * Main window
 * ======================================================================= */

class ObolViewerWindow : public Fl_Double_Window {
    /* UI widgets */
    Fl_Hold_Browser*  browser_;
    CoinPanel*        left_panel_;    /* system GL  */
    CoinPanel*        right_panel_;   /* OSMesa     */
    CoinPanel*        nanort_panel_;  /* NanoRT     */
    Fl_Check_Button*  sync_btn_;
    Fl_Box*           status_bar_;
    Fl_Button*        reload_btn_;
    Fl_Button*        render_btn_;

    /* Bridges */
    ObolBridgeAPI*    sys_api_    = nullptr;
    ObolBridgeAPI*    osmesa_api_ = nullptr;
    ObolBridgeAPI*    nanort_api_ = nullptr;

    /* Sync guard (prevents recursive sync) */
    bool syncing_ = false;

    static const int BROWSER_W = 220;
    static const int TOOLBAR_H = 32;
    static const int STATUS_H  = 22;

public:
    ObolViewerWindow(int W, int H,
                     ObolBridgeAPI* sys_api, ObolBridgeAPI* osmesa_api,
                     ObolBridgeAPI* nanort_api = nullptr)
        : Fl_Double_Window(W, H, "Obol Scene Viewer"),
          sys_api_(sys_api), osmesa_api_(osmesa_api), nanort_api_(nanort_api)
    {
        begin();
        buildUI(W, H);
        end();

        /* Populate the scene browser from the OSMesa bridge (always present) */
        ObolBridgeAPI* cat_api = osmesa_api_ ? osmesa_api_ : sys_api_;
        if (cat_api) {
            int n = cat_api->fn_scene_count();
            for (int i = 0; i < n; ++i) {
                std::string entry =
                    std::string(cat_api->fn_scene_category(i)) + "/" +
                    cat_api->fn_scene_name(i);
                browser_->add(entry.c_str());
            }
        }

        /* Wire up camera-sync callbacks */
        left_panel_->on_camera_changed = [this](CoinPanel* src) {
            if (!syncing_ && sync_btn_->value()) syncCamerasFrom(src);
        };
        right_panel_->on_camera_changed = [this](CoinPanel* src) {
            if (!syncing_ && sync_btn_->value()) syncCamerasFrom(src);
        };
        if (nanort_panel_) {
            nanort_panel_->on_camera_changed = [this](CoinPanel* src) {
                if (!syncing_ && sync_btn_->value()) syncCamerasFrom(src);
            };
        }

        updateStatusBar();
        resizable(this);
    }

    /* ------------------------------------------------------------------
     * Public: load a named scene into all available panels
     * ------------------------------------------------------------------ */
    void loadScene(const char* name) {
        left_panel_->createScene(name);
        right_panel_->createScene(name);
        if (nanort_panel_) nanort_panel_->createScene(name);
        updateStatusBar();
    }

private:
    /* ------------------------------------------------------------------
     * UI construction
     * ------------------------------------------------------------------ */
    void buildUI(int W, int H) {
        int content_h = H - TOOLBAR_H - STATUS_H;

        /* ---- Left browser ---- */
        browser_ = new Fl_Hold_Browser(0, 0, BROWSER_W, content_h);
        browser_->textsize(12);
        browser_->callback(browserCB, this);

        /* ---- Render area (Fl_Tile for resizable split) ---- */
        Fl_Tile* tile = new Fl_Tile(BROWSER_W, 0,
                                    W - BROWSER_W, content_h);
        {
            int num_panels  = nanort_api_ ? 3 : 2;
            int panel_w     = (W - BROWSER_W) / num_panels;
            int x           = BROWSER_W;

            /* Left panel: system GL */
            left_panel_ = new CoinPanel(x, 0, panel_w, content_h,
                                        sys_api_
                                            ? sys_api_->fn_backend_name()
                                            : "System GL (unavailable)");
            left_panel_->bridge = sys_api_;
            x += panel_w;

            /* Middle panel: OSMesa */
            int osmesa_w = nanort_api_ ? panel_w : W - BROWSER_W - panel_w;
            right_panel_ = new CoinPanel(x, 0, osmesa_w, content_h,
                                         osmesa_api_
                                             ? osmesa_api_->fn_backend_name()
                                             : "OSMesa (unavailable)");
            right_panel_->bridge = osmesa_api_;
            x += osmesa_w;

            /* Right panel: NanoRT (optional) */
            if (nanort_api_) {
                nanort_panel_ = new CoinPanel(x, 0,
                                              W - BROWSER_W - 2 * panel_w,
                                              content_h,
                                              nanort_api_->fn_backend_name());
                nanort_panel_->bridge = nanort_api_;
            } else {
                nanort_panel_ = nullptr;
            }
        }
        tile->end();

        /* ---- Toolbar ---- */
        Fl_Group* toolbar = new Fl_Group(0, content_h, W, TOOLBAR_H);
        toolbar->box(FL_UP_BOX);
        {
            int bx = 6, by = content_h + 4, bh = TOOLBAR_H - 8;

            reload_btn_ = new Fl_Button(bx, by, 70, bh, "Reload");
            reload_btn_->callback(reloadCB, this);
            bx += 76;

            render_btn_ = new Fl_Button(bx, by, 80, bh, "Render…");
            render_btn_->callback(renderCB, this);
            bx += 86;

            sync_btn_ = new Fl_Check_Button(bx, by, 100, bh, "Sync views");
            sync_btn_->value(1);   /* default: synced */
            bx += 106;

            /* status label fills remaining space */
            status_bar_ = new Fl_Box(bx, by, W - bx - 6, bh, "");
            status_bar_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            status_bar_->labelsize(12);
        }
        toolbar->end();

        /* ---- Status bar ---- */
        Fl_Box* statusbox = new Fl_Box(0, content_h + TOOLBAR_H,
                                       W, STATUS_H, "");
        statusbox->box(FL_ENGRAVED_BOX);
        statusbox->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        statusbox->labelsize(11);
    }

    /* ------------------------------------------------------------------
     * Camera synchronisation
     * ------------------------------------------------------------------ */
    void syncCamerasFrom(CoinPanel* src) {
        float pos[3], orient[4], dist = 1.0f;
        src->getCamera(pos, orient, dist);
        syncing_ = true;
        for (CoinPanel* dst : {left_panel_, right_panel_, nanort_panel_}) {
            if (dst && dst != src && dst->scene && dst->bridge)
                dst->setCamera(pos, orient, dist);
        }
        syncing_ = false;
    }

    /* ------------------------------------------------------------------
     * Status bar
     * ------------------------------------------------------------------ */
    void updateStatusBar() {
        std::string msg;
        if (sys_api_)
            msg += std::string("Left: ") + sys_api_->fn_backend_name();
        else
            msg += "Left: System GL (NOT available – using OSMesa fallback)";
        msg += "   |   ";
        if (osmesa_api_)
            msg += std::string("Mid: ") + osmesa_api_->fn_backend_name();
        else
            msg += "Mid: OSMesa (NOT available)";
        if (nanort_api_) {
            msg += "   |   ";
            msg += std::string("Right: ") + nanort_api_->fn_backend_name();
        }
        status_bar_->copy_label(msg.c_str());
    }

    /* ------------------------------------------------------------------
     * Callbacks
     * ------------------------------------------------------------------ */
    static void browserCB(Fl_Widget*, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        int sel = self->browser_->value();
        if (sel < 1) return;
        const char* entry = self->browser_->text(sel);
        /* entry format: "Category/scene_name" */
        const char* slash = strrchr(entry, '/');
        const char* name  = slash ? slash + 1 : entry;
        self->loadScene(name);
    }

    static void reloadCB(Fl_Widget*, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        int sel = self->browser_->value();
        if (sel < 1) return;
        const char* entry = self->browser_->text(sel);
        const char* slash = strrchr(entry, '/');
        const char* name  = slash ? slash + 1 : entry;
        self->loadScene(name);
    }

    static void renderCB(Fl_Widget*, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        /* Ask which backend to render */
        int choice = fl_choice(
            "Render to PNG from which backend?",
            "Cancel", "System GL", "OSMesa");
        if (choice == 0) return;  /* Cancel */
        const char* path = fl_file_chooser(
            "Save rendered image", "*.rgb", "output.rgb");
        if (!path) return;

        int sel = self->browser_->value();
        if (sel < 1) { fl_message("Please select a scene first."); return; }
        const char* entry = self->browser_->text(sel);
        const char* slash = strrchr(entry, '/');
        const char* name  = slash ? slash + 1 : entry;

        ObolBridgeAPI* api = (choice == 1) ? self->sys_api_
                                            : self->osmesa_api_;
        if (!api) { fl_message("Selected backend is not available."); return; }

        int pw = 800, ph = 600;
        std::vector<uint8_t> buf((size_t)pw * ph * 4);
        ObolScene sc = api->fn_scene_create(name, pw, ph);
        if (!sc) { fl_message("Failed to create scene."); return; }
        int rc = api->fn_scene_render(sc, buf.data(), pw, ph);
        api->fn_scene_destroy(sc);
        if (rc != 0) { fl_message("Render failed."); return; }

        /* Write raw RGB (drop alpha) – convert bottom-up to top-down */
        FILE* f = fopen(path, "wb");
        if (!f) { fl_message("Cannot open output file."); return; }
        for (int row = ph - 1; row >= 0; --row) {
            const uint8_t* src = buf.data() + (size_t)row * pw * 4;
            for (int col = 0; col < pw; ++col) {
                fputc(src[0], f); fputc(src[1], f); fputc(src[2], f);
                src += 4;
            }
        }
        fclose(f);
        fl_message("Saved %d×%d RGB image to:\n%s", pw, ph, path);
    }
};

/* =========================================================================
 * main()
 * ======================================================================= */

int main(int argc, char** argv)
{
    /* ---- Load bridge libraries ---- */

    /* Determine a fallback search directory: same dir as the executable */
    std::string exe_dir;
    if (argc > 0 && argv[0]) {
        std::string exe(argv[0]);
        auto sep = exe.rfind('/');
        if (sep != std::string::npos) exe_dir = exe.substr(0, sep);
    }
    const char* extra = exe_dir.empty() ? nullptr : exe_dir.c_str();

    ObolBridgeAPI* sys_api    = try_load_bridge("libobol_bridge_sys.so",    extra);
    ObolBridgeAPI* osmesa_api = try_load_bridge("libobol_bridge_osmesa.so", extra);
    ObolBridgeAPI* nanort_api = try_load_bridge("libobol_bridge_nanort.so", extra);

    if (nanort_api)
        fprintf(stdout, "obol_viewer: NanoRT bridge loaded\n");
    else
        fprintf(stdout, "obol_viewer: NanoRT bridge not found – NanoRT panel disabled\n");

    /* Graceful fallback: if system GL bridge is unavailable, use OSMesa for
     * both panels so the viewer is still functional. */
    bool sys_fallback = false;
    if (!sys_api && osmesa_api) {
        fprintf(stderr,
            "obol_viewer: system GL bridge unavailable – using OSMesa for both panels\n");
        sys_api = osmesa_api;
        sys_fallback = true;
    }

    if (!sys_api && !osmesa_api) {
        fprintf(stderr,
            "obol_viewer: ERROR: could not load either bridge library.\n"
            "  Run from the superbuild output directory, or set LD_LIBRARY_PATH.\n");
        return 1;
    }

    /* ---- Build and show the window ---- */
    Fl::scheme("gtk+");
    Fl::visual(FL_RGB | FL_DOUBLE);

    ObolViewerWindow* win = new ObolViewerWindow(
        1280, 720, sys_api, osmesa_api, nanort_api);
    win->show(argc, argv);

    if (sys_fallback) {
        fl_message("System GL bridge (libobol_bridge_sys.so) was not found.\n"
                   "Both panels are using the OSMesa backend.\n\n"
                   "To enable side-by-side comparison, build the viewer\n"
                   "using the superbuild in tests/obol_superbuild/.");
    }

    /* Load the first scene automatically */
    ObolBridgeAPI* cat_api = osmesa_api ? osmesa_api : sys_api;
    if (cat_api && cat_api->fn_scene_count() > 0) {
        win->loadScene(cat_api->fn_scene_name(0));
    }

    int ret = Fl::run();

    /* Cleanup */
    delete win;
    if (nanort_api) delete nanort_api;
    if (osmesa_api && osmesa_api != sys_api) delete osmesa_api;
    if (sys_api) delete sys_api;

    return ret;
}
