/*
 * portablegl_impl.cpp  –  PortableGL single-header implementation TU
 *
 * PortableGL is distributed as a single-header library in the style of the
 * stb libraries.  Exactly ONE translation unit must define
 * PORTABLEGL_IMPLEMENTATION before including portablegl.h in order to emit
 * the function bodies.  All other TUs that need the declarations (type
 * definitions, function prototypes, GL constant values) include portablegl.h
 * WITHOUT the implementation define and get only the header portion.
 *
 * This file is that one TU.  It is compiled only when OBOL_USE_PORTABLEGL is
 * ON (i.e. when the library was configured with the PortableGL software
 * renderer backend).
 *
 * Preprocessor knobs used here:
 *
 *   PGL_PREFIX_TYPES
 *     Renames the GLSL-style builtin types that PortableGL injects into the
 *     global namespace (vec2, vec3, vec4, mat3, mat4, ivec*, uvec*, bvec*,
 *     Color, Line, Plane, …) to pgl_vec2, pgl_vec3, etc.  Without this,
 *     those names clash with local variables in the Obol source files that
 *     happen to use the same identifiers.
 *
 *   PGL_PREFIX_GLSL
 *     Renames PortableGL's implementations of GLSL-style math helpers
 *     (smoothstep → pgl_smoothstep, clamp → pgl_clamp, etc.) to avoid
 *     collisions with the clamp() template in toojpeg.cpp and the clamp()
 *     helper in evaluator.cpp.
 *
 *   PGL_PREFIX_GL
 *     Renames every gl* function to pgl_gl* (e.g. glDrawArrays →
 *     pgl_glDrawArrays) using portablegl_gl_mangle.h.  This prevents symbol
 *     clashes with system OpenGL when both backends are linked into the same
 *     shared library (OBOL_BUILD_DUAL_PORTABLEGL builds).
 *
 *     NOTE: In a pure OBOL_USE_PORTABLEGL (portablegl-only) build this
 *     renaming is unnecessary because there is no system GL.  However it is
 *     always applied so that the same object file can be used for both build
 *     modes without recompilation.
 */

#define PGL_PREFIX_TYPES 1
#define PGL_PREFIX_GLSL  1
#define PGL_PREFIX_GL    1
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl/portablegl.h>
