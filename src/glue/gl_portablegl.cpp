/*
 * gl_portablegl.cpp  –  PortableGL backend variant of the Obol GL glue layer.
 *
 * Architecture
 * ------------
 * This file implements OBOL_BUILD_DUAL_PORTABLEGL support, which allows both
 * the system-OpenGL and the PortableGL software backends to be compiled into
 * a single shared library so that applications can switch between them at
 * runtime — exactly as the dual-GL system-GL + OSMesa build does.
 *
 * The approach mirrors BRL-CAD's technique for embedding a private zlib
 * alongside a system zlib:
 *
 *   1. gl.cpp is the canonical implementation of the GL glue layer.
 *      When compiled normally it produces SoGLContext_instance(),
 *      SoGLContext_glGenTextures(), etc., all calling system GL symbols
 *      (glGetString, glGenTextures, …).
 *
 *   2. This file (gl_portablegl.cpp) recompiles the same source with:
 *        a. OBOL_GLHEADERS_PORTABLEGL_OVERRIDE  – makes gl-headers.h pull in
 *           portablegl.h (and portablegl_gl_mangle.h) instead of the system
 *           <GL/gl.h>.  With PGL_PREFIX_GL active, every gl* symbol is
 *           renamed to pgl_gl*.
 *        b. SOGL_PREFIX_SET / SOGL_PREFIX_STR=portablegl_  – every
 *           SoGLContext_* function defined in gl.cpp gets the portablegl_
 *           prefix in this compilation unit, e.g.
 *           portablegl_SoGLContext_instance().
 *
 * The result: the object file from this TU exports portablegl_SoGLContext_*
 * symbols that call pgl_gl* (PortableGL-private) functions, while the object
 * file from gl.cpp exports the plain SoGLContext_* symbols that call the real
 * system GL.  Both coexist in the same .so without linker symbol collisions.
 *
 * Dispatch
 * --------
 * A thin dispatch layer in gl.cpp (compiled with OBOL_BUILD_DUAL_PORTABLEGL
 * defined) keeps the stable SoGLContext_* API working at runtime: it checks a
 * per-context backend flag set at context-creation time and forwards to either
 * portablegl_SoGLContext_* or the system-GL implementation.
 *
 * PortableGL GL symbol renaming
 * -----------------------------
 * PortableGL uses gl* names for its public API.  To avoid linker collisions
 * with system OpenGL (which uses the same names), portablegl_gl_mangle.h
 * provides a flat set of #define macros (e.g. glDrawArrays → pgl_glDrawArrays)
 * that rename every gl* symbol PortableGL defines.
 *
 * The mechanism mirrors OSMesa's <OSMesa/gl_mangle.h> / USE_MGL_NAMESPACE
 * approach.  portablegl_impl.cpp compiles PortableGL with PGL_PREFIX_GL=1
 * so its exported symbols are pgl_gl*; this TU sets
 * OBOL_GLHEADERS_PORTABLEGL_OVERRIDE so that the gl-headers.h selector
 * includes portablegl.h with the same mangle active, making every gl* call
 * in the re-compiled gl.cpp resolve to a pgl_gl* symbol.
 *
 * Proposed upstream contribution to PortableGL
 * ---------------------------------------------
 * The PGL_PREFIX_GL mechanism described above (a check inside portablegl.h
 * for PGL_PREFIX_GL that includes portablegl_gl_mangle.h) is proposed as an
 * upstream addition to PortableGL.  Until that is merged, the include is
 * placed in gl-headers.h.cmake.in under the OBOL_PORTABLEGL_BUILD / dual
 * guard.  See cmake/FindPortableGL.cmake for the full list of proposed
 * upstream contributions.
 */

/* -----------------------------------------------------------------------
 * Step 1 – override the GL header selector so that Inventor/system/gl.h
 *           (and therefore every source that pulls it in via glp.h) uses
 *           PortableGL headers rather than the system <GL/gl.h>.
 *           This define is tested in include/Inventor/system/gl-headers.h.
 * --------------------------------------------------------------------- */
#define OBOL_GLHEADERS_PORTABLEGL_OVERRIDE 1

/* -----------------------------------------------------------------------
 * Step 2 – activate the pgl_gl* name mangling.
 *           With OBOL_GLHEADERS_PORTABLEGL_OVERRIDE set, gl-headers.h will
 *           include portablegl_gl_mangle.h before portablegl.h so that every
 *           gl* call in this TU resolves to a pgl_gl* symbol.
 * --------------------------------------------------------------------- */
#ifndef PGL_PREFIX_GL
#define PGL_PREFIX_GL 1
#endif

/* -----------------------------------------------------------------------
 * Step 3 – activate the SOGL function-name prefix so that every
 *           SoGLContext_* function defined in gl.cpp gets the portablegl_
 *           prefix in this compilation unit.
 * --------------------------------------------------------------------- */
#define SOGL_PREFIX_SET 1
#define SOGL_PREFIX_STR portablegl_

/* -----------------------------------------------------------------------
 * Step 4 – compile the primary GL glue implementation.
 *           The #defines above take effect inside the included file.
 * --------------------------------------------------------------------- */
#include "gl.cpp"
