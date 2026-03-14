/*
 * glx_mangle.h  –  stub for builds that define USE_MGL_NAMESPACE
 *
 * Mesa's GL/glx.h conditionally includes "glx_mangle.h" when the macro
 * USE_MGL_NAMESPACE is defined.  Obol's OSMesa-only build defines that
 * macro globally to keep OSMesa's mgl* symbols separate from system gl*
 * symbols.  However, we always call GLX functions by their standard names
 * (glXCreateContext, glXMakeCurrent, …); we never want them renamed to the
 * mglX* namespace.
 *
 * On modern Ubuntu (24.04+) the file /usr/include/GL/glx_mangle.h is not
 * shipped by any distribution package, so the conditional include in glx.h
 * fails with a fatal error.  This stub satisfies the include without
 * introducing any symbol renaming, which is the correct behaviour for
 * every build configuration in this project.
 *
 * The file is placed in tests/utils/ which is on the compiler's -I search
 * path for all test targets; GCC/Clang find it after the system directory
 * fails the quoted-include relative-path search.
 */

#ifndef GLX_MANGLE_H
#define GLX_MANGLE_H
/* Intentionally empty: do NOT rename glX* to mglX*. */
#endif /* GLX_MANGLE_H */
