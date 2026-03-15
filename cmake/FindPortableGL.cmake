# FindPortableGL.cmake
# Locates the PortableGL single-header software renderer.
#
# PortableGL is a single-header C library that implements a subset of the
# OpenGL 3.x core profile in software without requiring any platform GL
# library or GPU driver.  The header is available from:
#
#   https://github.com/rswinkle/PortableGL
#
# The external/portablegl directory is populated via the git submodule
# declared in .gitmodules, which is automatically initialised at configure
# time by the obol_init_submodule() helper in the root CMakeLists.txt.
#
# Unlike OSMesa, PortableGL does not require name-mangling because its
# functions are either static (file-local) or have non-conflicting names
# such as init_glContext/free_glContext/set_glContext.  No custom symbol
# prefixes or separate compilation units are needed.
#
# This will define the following variables:
#
#   PortableGL_FOUND        - True if the portablegl.h header is available
#   PortableGL_INCLUDE_DIR  - Directory containing portablegl.h
#
# Interface target:
#   PortableGL::PortableGL  - INTERFACE library with include path and defines

set(_pgl_header "${PROJECT_SOURCE_DIR}/external/portablegl/portablegl.h")

if(EXISTS "${_pgl_header}")
    message(STATUS "PortableGL: using external/portablegl submodule")

    set(PortableGL_FOUND       TRUE)
    set(PortableGL_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/external/portablegl")

    if(NOT TARGET PortableGL::PortableGL)
        add_library(PortableGL::PortableGL INTERFACE IMPORTED GLOBAL)
        target_include_directories(PortableGL::PortableGL INTERFACE
            "${PortableGL_INCLUDE_DIR}")
        # Use PGL_PREFIX_TYPES to prevent PortableGL's built-in vec2/vec3/vec4/
        # mat4 types from conflicting with system GL or Obol math headers.
        target_compile_definitions(PortableGL::PortableGL INTERFACE
            PGL_PREFIX_TYPES)
    endif()

else()
    set(PortableGL_FOUND FALSE)
    if(PortableGL_FIND_REQUIRED)
        message(FATAL_ERROR
            "PortableGL header not found at external/portablegl/portablegl.h.\n"
            "Run: git submodule update --init external/portablegl\n"
            "Or clone manually: git clone https://github.com/rswinkle/PortableGL.git external/portablegl")
    else()
        message(STATUS "PortableGL header not found at external/portablegl – PortableGL support disabled")
    endif()
endif()

unset(_pgl_header)
