/*
 * obol_bridge.h  –  C interface for the Obol dual-backend bridge
 *
 * This header defines a pure-C API that allows the same viewer binary to
 * host two completely independent Obol instances (one compiled against
 * system OpenGL/GLX and one against OSMesa) without any C++ symbol
 * collisions.
 *
 * Why a C bridge?
 * ───────────────
 * C++ symbols are name-mangled; two shared libraries compiled from the same
 * Obol sources will export identical symbol names (e.g. _ZN5SoDB4initEv).
 * Linking both into one executable would cause duplicate-symbol linker
 * errors, and dynamic loading with RTLD_GLOBAL would cause the second
 * library's symbols to resolve to the first.
 *
 * The solution (mirrors OSMesa's "MGL" mangling for GL functions):
 *
 *   • Each Obol variant (sys-GL, OSMesa) is built as a *static* library
 *     with all C++ symbols hidden via -fvisibility=hidden.
 *
 *   • A thin bridge shared library (libobol_bridge_sys.so /
 *     libobol_bridge_osmesa.so) links that static library with
 *     --whole-archive so all code is embedded, then exports only the
 *     obol_* C functions below via a linker version script.
 *
 *   • The viewer loads both .so files with dlopen(RTLD_LOCAL) so neither
 *     bridge's hidden C++ symbols pollute the other's namespace.
 *
 *   • At runtime the viewer falls back to the OSMesa bridge if the sys-GL
 *     bridge is unavailable (no display, missing libGL, …).
 *
 * Usage in the viewer:
 *   ObolBridgeAPI* gl    = obol_bridge_load("libobol_bridge_sys.so");
 *   ObolBridgeAPI* osmesa= obol_bridge_load("libobol_bridge_osmesa.so");
 *   if (!gl) gl = osmesa;   // graceful fallback
 */

#ifndef OBOL_BRIDGE_H
#define OBOL_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Opaque scene handle
 * --------------------------------------------------------------------- */
typedef struct ObolBridgeScene* ObolScene;

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

/**
 * Initialize the Obol runtime (call once before any other obol_ function).
 * Returns 0 on success, non-zero on error.
 */
int obol_init(void);

/**
 * A human-readable name for this backend, e.g. "OpenGL/GLX" or "OSMesa".
 */
const char* obol_backend_name(void);

/* -----------------------------------------------------------------------
 * Scene catalogue
 * --------------------------------------------------------------------- */

/** Number of built-in test scenes registered in this bridge. */
int obol_scene_count(void);

/** Name of scene at index i (stable identifier, suitable as a key). */
const char* obol_scene_name(int i);

/** Category string (e.g. "Rendering", "Actions"). */
const char* obol_scene_category(int i);

/** One-line human-readable description. */
const char* obol_scene_description(int i);

/** Find scene index by name; returns -1 if not found. */
int obol_scene_find(const char* name);

/* -----------------------------------------------------------------------
 * Scene instance
 * --------------------------------------------------------------------- */

/**
 * Create an interactive scene instance.
 * @param name    One of the names returned by obol_scene_name().
 * @param width   Initial viewport width  (pixels).
 * @param height  Initial viewport height (pixels).
 * @return        Opaque handle, or NULL on failure.
 */
ObolScene obol_scene_create(const char* name, int width, int height);

/** Release all resources for this scene. */
void obol_scene_destroy(ObolScene scene);

/* -----------------------------------------------------------------------
 * Rendering
 * --------------------------------------------------------------------- */

/**
 * Render the scene and write RGBA pixels to @p rgba_buf.
 * @p rgba_buf must point to at least width*height*4 bytes.
 * Returns 0 on success, non-zero on failure.
 * The buffer is bottom-row-first (OpenGL convention) unless the backend
 * description says otherwise.
 */
int obol_scene_render(ObolScene scene, uint8_t* rgba_buf,
                      int width, int height);

/** Notify the scene that the viewport has been resized. */
void obol_scene_resize(ObolScene scene, int width, int height);

/* -----------------------------------------------------------------------
 * Events (coordinates in viewport pixels, origin at top-left)
 * --------------------------------------------------------------------- */

/** Mouse button press.  button: 1=left, 2=middle, 3=right. */
void obol_scene_mouse_press(ObolScene scene, int x, int y, int button);

/** Mouse button release. */
void obol_scene_mouse_release(ObolScene scene, int x, int y, int button);

/** Mouse move (fired during drag and on hover). */
void obol_scene_mouse_move(ObolScene scene, int x, int y);

/** Mouse-wheel scroll.  delta>0 = zoom in, delta<0 = zoom out. */
void obol_scene_scroll(ObolScene scene, float delta);

/** Key press.  key is an ASCII code or OBOL_KEY_* constant. */
void obol_scene_key_press(ObolScene scene, int key);

/** Key release. */
void obol_scene_key_release(ObolScene scene, int key);

/* -----------------------------------------------------------------------
 * Camera state (for syncing two views)
 * --------------------------------------------------------------------- */

/**
 * Read the current camera state.
 * @param pos3    Out: camera position (x,y,z).
 * @param orient4 Out: orientation quaternion (x,y,z,w).
 * @param dist    Out: focal distance.
 */
void obol_scene_get_camera(ObolScene scene,
                            float* pos3, float* orient4, float* dist);

/**
 * Write the camera state (mirrors obol_scene_get_camera output).
 */
void obol_scene_set_camera(ObolScene scene,
                            const float* pos3, const float* orient4,
                            float dist);

/* -----------------------------------------------------------------------
 * Convenience: load a bridge .so and populate a function-pointer table
 * (defined in obol_bridge_loader.h / obol_bridge_loader.cpp for the viewer)
 * --------------------------------------------------------------------- */

#ifdef __cplusplus
} /* extern "C" */
#endif

/* -----------------------------------------------------------------------
 * C++ helper: function-pointer table used by the viewer to call into
 * either bridge without knowing which .so it came from.
 * --------------------------------------------------------------------- */
#ifdef __cplusplus
#include <dlfcn.h>
#include <cstring>

/**
 * ObolBridgeAPI – thin wrapper around a dlopen'd bridge .so.
 *
 * Load with ObolBridgeAPI::load(); destroy with delete.
 * All fields are function pointers resolved from the shared library.
 * The "fn_" prefix avoids name clashes with the C declarations above.
 */
struct ObolBridgeAPI {
    /* Raw dlopen handle – kept open for the lifetime of this object. */
    void* dl_handle = nullptr;

    /* All bridge entry points */
    int          (*fn_init)(void)                                           = nullptr;
    const char*  (*fn_backend_name)(void)                                   = nullptr;
    int          (*fn_scene_count)(void)                                    = nullptr;
    const char*  (*fn_scene_name)(int)                                      = nullptr;
    const char*  (*fn_scene_category)(int)                                  = nullptr;
    const char*  (*fn_scene_description)(int)                               = nullptr;
    int          (*fn_scene_find)(const char*)                              = nullptr;
    ObolScene    (*fn_scene_create)(const char*, int, int)                  = nullptr;
    void         (*fn_scene_destroy)(ObolScene)                             = nullptr;
    int          (*fn_scene_render)(ObolScene, uint8_t*, int, int)         = nullptr;
    void         (*fn_scene_resize)(ObolScene, int, int)                   = nullptr;
    void         (*fn_scene_mouse_press)(ObolScene, int, int, int)         = nullptr;
    void         (*fn_scene_mouse_release)(ObolScene, int, int, int)       = nullptr;
    void         (*fn_scene_mouse_move)(ObolScene, int, int)               = nullptr;
    void         (*fn_scene_scroll)(ObolScene, float)                      = nullptr;
    void         (*fn_scene_key_press)(ObolScene, int)                     = nullptr;
    void         (*fn_scene_key_release)(ObolScene, int)                   = nullptr;
    void         (*fn_scene_get_camera)(ObolScene, float*, float*, float*) = nullptr;
    void         (*fn_scene_set_camera)(ObolScene, const float*,
                                        const float*, float)               = nullptr;

    ~ObolBridgeAPI() {
        if (dl_handle) { dlclose(dl_handle); dl_handle = nullptr; }
    }

    bool valid() const { return dl_handle && fn_init; }

    /**
     * Attempt to open @p so_path and resolve all bridge symbols.
     * Returns a heap-allocated ObolBridgeAPI, or nullptr on failure.
     * The caller must delete the returned object when done.
     */
    static ObolBridgeAPI* load(const char* so_path) {
        void* h = dlopen(so_path, RTLD_LOCAL | RTLD_LAZY);
        if (!h) return nullptr;

        ObolBridgeAPI* api = new ObolBridgeAPI();
        api->dl_handle = h;

#define RESOLVE(sym) \
    api->fn_##sym = reinterpret_cast<decltype(api->fn_##sym)>(dlsym(h, "obol_" #sym)); \
    if (!api->fn_##sym) { delete api; return nullptr; }

        RESOLVE(init)
        RESOLVE(backend_name)
        RESOLVE(scene_count)
        RESOLVE(scene_name)
        RESOLVE(scene_category)
        RESOLVE(scene_description)
        RESOLVE(scene_find)
        RESOLVE(scene_create)
        RESOLVE(scene_destroy)
        RESOLVE(scene_render)
        RESOLVE(scene_resize)
        RESOLVE(scene_mouse_press)
        RESOLVE(scene_mouse_release)
        RESOLVE(scene_mouse_move)
        RESOLVE(scene_scroll)
        RESOLVE(scene_key_press)
        RESOLVE(scene_key_release)
        RESOLVE(scene_get_camera)
        RESOLVE(scene_set_camera)
#undef RESOLVE

        if (api->fn_init() != 0) { delete api; return nullptr; }
        return api;
    }
};
#endif /* __cplusplus */

#endif /* OBOL_BRIDGE_H */
