# FindPortableGL.cmake
# Locates the PortableGL single-header software renderer.
#
# PortableGL is a software implementation of OpenGL 3.x core in a single C/C++
# header file.  Unlike OSMesa it does NOT support GLSL shader compilation;
# shaders must be supplied as C/C++ function pointers via pglCreateProgram().
# This is the primary obstacle to a full Obol migration (see obstacles below).
#
# The project embeds a copy of portablegl.h in external/portablegl/.
#
# Known obstacles for full Obol/Coin migration
# --------------------------------------------
# 1. GLSL incompatibility (BLOCKER):
#    Obol generates and compiles GLSL programs at runtime (SoShaderObject,
#    SoShadowGroup, etc.).  PortableGL has no GLSL compiler; glShaderSource(),
#    glCompileShader() and glLinkProgram() are compile-time stubs that silently
#    succeed but produce no working shader.  Every GLSL-based render path will
#    produce incorrect or blank output.
#
# 2. Uniform stubs:
#    All glUniform*() and glUniformMatrix*() functions are empty stubs.  Because
#    Obol relies on uniforms for transformation matrices, lighting parameters,
#    and material properties, none of those values will reach any PortableGL
#    C-function shader even if one is provided.
#
# 3. FBO stubs:
#    glGenFramebuffers(), glBindFramebuffer(), glCheckFramebufferStatus() etc.
#    are all empty stubs.  SoSceneTexture2 (render-to-texture), shadow maps
#    and any node that creates its own FBO will silently fail.
#
# 4. Missing glReadPixels():
#    PortableGL does not implement glReadPixels().  CoinOffscreenGLCanvas uses
#    glReadPixels() to extract rendered pixels.  A workaround is provided in
#    SoDBPortableGL.cpp via pglGetBackBuffer(), but it only works for the
#    default framebuffer and requires pixel-format conversion.
#
# 5. No dual-GL support:
#    OSMesa uses MGL name mangling (gl* -> mgl*) to coexist with system OpenGL
#    in the same process.  PortableGL defines the standard gl* names directly.
#    Without a code-generation step analogous to OSMesa's name-mangling build,
#    OBOL_USE_PORTABLEGL is mutually exclusive with system-OpenGL and OSMesa.
#
# 6. Type name conflicts:
#    PortableGL defines vec2/vec3/vec4/mat4 etc. in the global namespace.
#    PGL_PREFIX_TYPES is activated when including portablegl.h via gl-headers.h
#    to rename them to pgl_vec2 etc. and avoid conflicts with Obol source code
#    that uses vec2/vec3 as variable names.
#
# 7. Performance:
#    PortableGL is a CPU software renderer with no parallel rasterisation.
#    For production workloads it will be significantly slower than even OSMesa
#    with llvmpipe.
#
# This will define the following variables:
#
#   PortableGL_FOUND        - True if PortableGL header is available
#   PortableGL_INCLUDE_DIRS - Directory containing portablegl.h
#
# Interface target:
#   portablegl_interface    - INTERFACE library for include path propagation

if(EXISTS "${PROJECT_SOURCE_DIR}/external/portablegl/portablegl.h")
    message(STATUS "Found PortableGL single-header library in external/portablegl/")

    if(NOT TARGET portablegl_interface)
        add_library(portablegl_interface INTERFACE)
        target_include_directories(portablegl_interface INTERFACE
            "${PROJECT_SOURCE_DIR}/external/portablegl")
    endif()

    set(PortableGL_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/external/portablegl")
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
