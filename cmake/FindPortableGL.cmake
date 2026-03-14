# FindPortableGL.cmake
# Locates the PortableGL single-header software renderer.
#
# PortableGL is a software implementation of OpenGL 3.x core in a single C/C++
# header file.  Unlike OSMesa it does NOT support GLSL shader compilation;
# shaders must be supplied as C/C++ function pointers via pglCreateProgram().
#
# The project embeds a copy of portablegl.h in external/portablegl/.
#
# Obstacles, status, and proposed PortableGL upstream contributions
# -----------------------------------------------------------------
#
# 1. GLSL incompatibility (RESOLVED via shader adapter):
#    Obol generates GLSL 1.10 compatibility-profile shaders at runtime.
#    The adapter in portablegl_shader_registry.cpp:
#      a) intercepts glShaderSourceARB / glCompileShaderARB / glLinkProgramARB,
#      b) inspects the GLSL source for known keywords (gl_LightSource,
#         fragmentNormal, texture2D, shadowMap, …) to classify the shader,
#      c) selects the matching pre-written C-function shader pair from
#         portablegl_obol_shaders.h (Gouraud, Phong, Textured, Depth, Flat),
#      d) creates the PortableGL program via pglCreateProgram(), and
#      e) at glUseProgramObjectARB time calls pglSetUniform() to wire the
#         ObolPGLCompatState as the uniform data block for C shaders.
#    PROPOSED UPSTREAM: An optional GLSL-to-C transpiler stub or a named-
#    shader registry (glNamedShaderARB / pglRegisterCShader) would allow
#    apps to associate C shaders with GLSL source names, eliminating the
#    need for keyword-based classification heuristics.
#
# 2. Uniforms (RESOLVED via ObolPGLCompatState):
#    All fixed-function uniform state (gl_ModelViewMatrix, gl_LightSource[],
#    gl_FrontMaterial, …) is tracked in ObolPGLCompatState (see
#    portablegl_compat_state.h) and passed to C shaders via pglSetUniform().
#    Named coin_* uniforms are handled via magic location integers returned
#    from the glGetUniformLocationARB interceptor.
#    PROPOSED UPSTREAM: A per-program key→value uniform store (e.g. an array
#    of float/int values indexed by glGetUniformLocation) would allow portable
#    C shaders to read application uniforms without a bespoke compat-state
#    struct.
#
# 3. FBO (PARTIALLY RESOLVED via pglSetTexBackBuffer):
#    FBO calls are intercepted by pgl_igl_GenFramebuffers / BindFramebuffer /
#    FramebufferTexture2D in portablegl_compat_funcs.cpp.  Colour attachment
#    render-to-texture is implemented via pglSetTexBackBuffer(); depth-only
#    attachments and multi-attachment FBOs are not yet supported.
#    Shadow maps require a depth-only pass; the OBOL_PGL_SHADER_DEPTH shader
#    writes gl_FragDepth but the FBO infrastructure to capture it still needs
#    work.
#    PROPOSED UPSTREAM: A proper FBO table with attachment objects would fully
#    replace the pglSetTexBackBuffer() workaround and enable depth textures,
#    renderbuffers, and MRT.
#
# 4. glReadPixels (RESOLVED via pglGetBackBuffer):
#    pgl_igl_ReadPixels() reads from the PortableGL back buffer directly and
#    performs the GL bottom-to-top / PGL top-to-bottom row flip.  Only
#    GL_RGBA / GL_UNSIGNED_BYTE is supported.
#    PROPOSED UPSTREAM: A proper glReadPixels implementation in portablegl.h
#    that handles all standard format/type combinations.
#
# 5. Fixed-function state (RESOLVED via interceptors):
#    glMatrixMode / glLoadMatrixf / glPushMatrix / glPopMatrix / glOrtho /
#    glFrustum / glLightfv / glMaterialfv / glColorMaterial / glBegin /
#    glEnd etc. are all intercepted and routed through ObolPGLCompatState.
#    PROPOSED UPSTREAM: Add a compat_state field to glContext and implement
#    these functions natively in portablegl.h.  Shaders would access it via
#    a pglGetCompatState() helper (or a pointer stored in pgl_uniforms).
#
# 6. No dual-GL support (REMAINS):
#    PortableGL defines standard gl* names directly.  Without name-mangling
#    analogous to OSMesa's mgl* symbols, OBOL_USE_PORTABLEGL is mutually
#    exclusive with system-OpenGL and OSMesa builds.
#    PROPOSED UPSTREAM: Add a PGL_PREFIX_GL option that renames all public
#    gl* symbols to pgl* (similar to how PGL_PREFIX_TYPES renames vec4, etc.),
#    enabling a dual-GL build with system OpenGL.
#
# 7. Performance (REMAINS):
#    PortableGL is a single-threaded CPU software renderer.  It will be
#    significantly slower than OSMesa+llvmpipe for complex scenes.
#    PROPOSED UPSTREAM: Multi-threaded tile rasterisation would substantially
#    improve throughput for Obol's typical workload (large meshes, Phong
#    lighting, shadow maps).
#
# This will define the following variables:
#
#   PortableGL_FOUND        - True if PortableGL header is available
#   PortableGL_INCLUDE_DIRS - Parent directory of portablegl/ (for
#                             #include <portablegl/portablegl.h>)
#
# Interface target:
#   portablegl_interface    - INTERFACE library for include path propagation

if(EXISTS "${PROJECT_SOURCE_DIR}/external/portablegl/portablegl.h")
    message(STATUS "Found PortableGL single-header library in external/portablegl/")

    if(NOT TARGET portablegl_interface)
        add_library(portablegl_interface INTERFACE)
        # The include path is the PARENT of the portablegl/ directory so that
        # #include <portablegl/portablegl.h> resolves correctly.
        target_include_directories(portablegl_interface INTERFACE
            "${PROJECT_SOURCE_DIR}/external")
    endif()

    # PortableGL_INCLUDE_DIRS = parent dir so <portablegl/portablegl.h> works
    set(PortableGL_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/external")
    set(PortableGL_FOUND        TRUE)
    set(PortableGL_INCLUDE_DIRS "${PortableGL_INCLUDE_DIR}")

else()
    set(PortableGL_FOUND FALSE)
    if(PortableGL_FIND_REQUIRED)
        message(FATAL_ERROR
            "PortableGL header not found at external/portablegl/portablegl.h.\n"
            "Please place portablegl.h in that directory or download it from:\n"
            "  https://raw.githubusercontent.com/rswinkle/PortableGL/master/portablegl.h")
    else()
        message(STATUS
            "PortableGL header not found at external/portablegl/ – PortableGL support disabled")
    endif()
endif()
