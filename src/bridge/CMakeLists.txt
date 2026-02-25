# =============================================================================
# src/bridge/CMakeLists.txt
#
# Builds one libobol_bridge_<suffix>.so per backend.
#
# Backend variants:
#   dual    – Obol built with COIN3D_BUILD_DUAL_GL (system GL + OSMesa in one
#             library).  The bridge uses OSMesa contexts for headless rendering
#             while the Coin library retains sys GL capability.
#   osmesa  – Obol built with COIN3D_USE_OSMESA=ON (OSMesa only).
#   sys     – Obol built against system OpenGL / GLX only.
#   nanort  – Obol with NanoRT CPU raytracing (no GL context required).
#
# The dual bridge eliminates the previous need for separate sys + osmesa
# bridges: one bridge handles both backends via the dual Coin library.
#
# Usage from the top-level project (after Coin is configured):
#   cmake ... -DCOIN_BUILD_BRIDGE=ON
# The bridge suffix is set automatically from COIN3D_BUILD_DUAL_GL /
# COIN3D_USE_OSMESA (see top-level CMakeLists.txt).
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# ---- Options ----------------------------------------------------------------

set(COIN_BRIDGE_SUFFIX "sys" CACHE STRING
    "Bridge library suffix (dual | sys | osmesa | nanort).  Set by top-level project.")

set(COIN_BRIDGE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE PATH
    "Directory where the bridge .so will be placed.")

# ---- Backend detection -------------------------------------------------------

if(COIN_BRIDGE_SUFFIX STREQUAL "nanort")
    set(_bridge_nanort TRUE)
else()
    set(_bridge_nanort FALSE)
endif()

if(COIN_BRIDGE_SUFFIX STREQUAL "dual")
    set(_bridge_dual TRUE)
else()
    set(_bridge_dual FALSE)
endif()

if(DEFINED COIN3D_USE_OSMESA AND COIN3D_USE_OSMESA AND NOT _bridge_dual)
    set(_bridge_osmesa TRUE)
else()
    set(_bridge_osmesa FALSE)
endif()

# ---- Shared library target --------------------------------------------------

add_library(obol_bridge_${COIN_BRIDGE_SUFFIX} SHARED
    obol_bridge.cpp
)

# All C++ symbols hidden; only obol_* exported via version script.
set_target_properties(obol_bridge_${COIN_BRIDGE_SUFFIX} PROPERTIES
    CXX_VISIBILITY_PRESET    hidden
    VISIBILITY_INLINES_HIDDEN ON
    OUTPUT_NAME              obol_bridge_${COIN_BRIDGE_SUFFIX}
    RUNTIME_OUTPUT_DIRECTORY "${COIN_BRIDGE_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${COIN_BRIDGE_OUTPUT_DIR}"
)

# Include paths
target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}          # obol_bridge.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

# Link against Coin (dynamic - Coin already has different sonames per backend)
target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX}
    Coin ${COIN_TARGET_LINK_LIBRARIES}
)

if(_bridge_nanort)
    # NanoRT: no GL or OSMesa needed; needs nanort.h and nanort_context_manager.h
    target_compile_definitions(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
        COIN3D_NANORT_BUILD)
    target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
        "${PROJECT_SOURCE_DIR}/external/nanort"
        "${PROJECT_SOURCE_DIR}/tests/utils"
    )
elseif(_bridge_dual)
    # Dual GL bridge: compiled against dual Coin library (sys GL + OSMesa).
    # Uses OSMesa context manager for headless rendering; the Coin library
    # also carries sys GL capability for use by the application directly.
    target_compile_definitions(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
        COIN3D_BUILD_DUAL_GL)
    # OSMesa headers – prefer submodule (name-mangled mgl*) for dual builds
    set(_osmesa_submodule_inc "${CMAKE_SOURCE_DIR}/external/osmesa/include")
    if(EXISTS "${_osmesa_submodule_inc}")
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            "${_osmesa_submodule_inc}")
    elseif(COIN_OSMESA_INCLUDE_DIRS)
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            ${COIN_OSMESA_INCLUDE_DIRS})
    endif()
    # Link OSMesa library
    if(COIN_OSMESA_LIBRARIES)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX}
            ${COIN_OSMESA_LIBRARIES})
    elseif(TARGET osmesa_interface)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} osmesa_interface)
    elseif(TARGET osmesa)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} osmesa)
    endif()
    # Also link sys GL / GLX for the sys-GL context manager path
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND)
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            ${X11_INCLUDE_DIR})
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX}
            ${X11_X11_LIB})
    endif()
    if(OpenGL_OpenGL_FOUND AND TARGET OpenGL::GL)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX}
            ${OPENGL_gl_LIBRARY})
    endif()
elseif(_bridge_osmesa)
    target_compile_definitions(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
        COIN3D_OSMESA_BUILD)
    set(_osmesa_inc "${CMAKE_SOURCE_DIR}/external/osmesa/include")
    if(EXISTS "${_osmesa_inc}")
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            "${_osmesa_inc}")
    elseif(COIN_OSMESA_INCLUDE_DIRS)
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            ${COIN_OSMESA_INCLUDE_DIRS})
    endif()
    if(TARGET osmesa_interface)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} osmesa_interface)
    elseif(TARGET osmesa)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} osmesa)
    elseif(OSMesa_LIBRARIES)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} ${OSMesa_LIBRARIES})
    endif()
else()
    # System GL / GLX
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND)
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            ${X11_INCLUDE_DIR})
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX}
            ${X11_X11_LIB})
    endif()
    if(OpenGL_OpenGL_FOUND AND TARGET OpenGL::GL)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX}
            ${OPENGL_gl_LIBRARY})
    endif()
endif()

# Version script (Linux/ELF) to restrict exported symbols to obol_*
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/obol_bridge.map
    )
endif()

if(USE_PTHREAD)
    target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} pthread)
endif()

# ---- Install ----------------------------------------------------------------
install(TARGETS obol_bridge_${COIN_BRIDGE_SUFFIX}
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES obol_bridge.h DESTINATION include)
