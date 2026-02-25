# =============================================================================
# tests/obol_superbuild/CMakeLists.txt
#
# Superbuild that compiles all Obol backends (system GL, OSMesa, NanoRT) and
# their C bridge shared libraries, then builds the obol_viewer FLTK application
# that loads them at runtime via dlopen.
#
# This is the ONLY CMakeLists you need to drive the full triple-backend viewer
# build.  You do NOT need to configure the top-level Obol project first.
#
# Quick start:
#   mkdir build_viewer && cd build_viewer
#   cmake ../tests/obol_superbuild -DCMAKE_BUILD_TYPE=Release
#   cmake --build . -j$(nproc)
#   ./viewer/obol_viewer
#
# What this builds
# ────────────────
#   obol_sys_build/    ← Obol configured with COIN3D_USE_OSMESA=OFF
#                        (system OpenGL / GLX)
#     → libobol_bridge_sys.so    (C bridge, all C++ symbols hidden)
#
#   obol_osmesa_build/ ← Obol configured with COIN3D_USE_OSMESA=ON
#                        (OSMesa; GL functions mangled to mgl* via
#                         USE_MGL_NAMESPACE, same trick as Mesa itself)
#     → libobol_bridge_osmesa.so (C bridge, all C++ symbols hidden)
#
#   obol_nanort_build/ ← Obol with NanoRT CPU raytracing backend
#                        (no OpenGL context required; uses COIN3D_NANORT_BUILD)
#     → libobol_bridge_nanort.so (C bridge, all C++ symbols hidden)
#
#   viewer/
#     → obol_viewer              (FLTK app, links only FLTK + dl)
#
# Symbol isolation
# ────────────────
# All bridge .so files are compiled with -fvisibility=hidden so all C++
# Obol symbols (SoDB, SoSeparator, …) remain library-private.  Only the
# twenty obol_* C functions listed in src/bridge/obol_bridge.map are
# exported.  The viewer loads all bridges with dlopen(RTLD_LOCAL) so even
# the exported names live in separate scopes and never collide.
#
# Runtime fallback
# ────────────────
# If libobol_bridge_sys.so cannot be opened (no DISPLAY, missing libGL,
# headless CI runner), the viewer automatically falls back to
# libobol_bridge_osmesa.so for both GL panels.  A status line in the UI
# reports which backend is active.
# The NanoRT panel is shown only when libobol_bridge_nanort.so is available.
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(ObolViewerSuperbuild LANGUAGES C CXX)

include(ExternalProject)

# -----------------------------------------------------------------------
# User-visible options
# -----------------------------------------------------------------------

set(OBOL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    CACHE PATH "Root of the Obol source tree")

option(SUPERBUILD_PARALLEL "Build sub-projects in parallel where possible" ON)
set(_jobs "")
if(SUPERBUILD_PARALLEL)
    cmake_host_system_information(RESULT _ncpu QUERY NUMBER_OF_LOGICAL_CORES)
    set(_jobs -- -j${_ncpu})
endif()

# Propagate the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# -----------------------------------------------------------------------
# Bridge output directory (shared between both Obol builds so the viewer
# can find both .so files in one location)
# -----------------------------------------------------------------------
set(BRIDGE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bridges")
file(MAKE_DIRECTORY "${BRIDGE_OUTPUT_DIR}")

# -----------------------------------------------------------------------
# Common CMake arguments forwarded to every sub-project
# -----------------------------------------------------------------------
set(_common_cmake_args
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DBUILD_SHARED_LIBS=ON           # Coin built as shared library
    -DCOIN_BUILD_TESTS=OFF
    -DCOIN_BUILD_EXAMPLES=OFF
    -DCOIN_BUILD_BRIDGE=ON
    -DCOIN_BRIDGE_OUTPUT_DIR=${BRIDGE_OUTPUT_DIR}
)

# -----------------------------------------------------------------------
# Sub-project 1: system OpenGL / GLX backend
# -----------------------------------------------------------------------
ExternalProject_Add(obol_sys
    SOURCE_DIR        "${OBOL_SOURCE_DIR}"
    BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/obol_sys_build"
    INSTALL_DIR       "${CMAKE_CURRENT_BINARY_DIR}/obol_sys_install"
    CMAKE_ARGS
        ${_common_cmake_args}
        -DCOIN3D_USE_OSMESA=OFF
        -DCOIN_BRIDGE_SUFFIX=sys
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR>
                          --target obol_bridge_sys ${_jobs}
    INSTALL_COMMAND   ""
    LOG_CONFIGURE ON  LOG_BUILD ON
    COMMENT           "Building Obol (system GL) + bridge"
)

# -----------------------------------------------------------------------
# Sub-project 2: OSMesa backend
# -----------------------------------------------------------------------
ExternalProject_Add(obol_osmesa
    SOURCE_DIR        "${OBOL_SOURCE_DIR}"
    BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/obol_osmesa_build"
    INSTALL_DIR       "${CMAKE_CURRENT_BINARY_DIR}/obol_osmesa_install"
    CMAKE_ARGS
        ${_common_cmake_args}
        -DCOIN3D_USE_OSMESA=ON
        -DCOIN_BRIDGE_SUFFIX=osmesa
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR>
                          --target obol_bridge_osmesa ${_jobs}
    INSTALL_COMMAND   ""
    LOG_CONFIGURE ON  LOG_BUILD ON
    COMMENT           "Building Obol (OSMesa) + bridge"
)

# -----------------------------------------------------------------------
# Sub-project 3: NanoRT CPU raytracing backend
# This backend uses the same OSMesa-built Coin (no system GL needed) but
# configures the bridge to use SoNanoRTContextManager instead of OSMesa.
# -----------------------------------------------------------------------
ExternalProject_Add(obol_nanort
    SOURCE_DIR        "${OBOL_SOURCE_DIR}"
    BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/obol_nanort_build"
    INSTALL_DIR       "${CMAKE_CURRENT_BINARY_DIR}/obol_nanort_install"
    CMAKE_ARGS
        ${_common_cmake_args}
        -DCOIN3D_USE_OSMESA=ON    # Build Coin headless (avoids GL dependency)
        -DCOIN_BRIDGE_SUFFIX=nanort
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR>
                          --target obol_bridge_nanort ${_jobs}
    INSTALL_COMMAND   ""
    LOG_CONFIGURE ON  LOG_BUILD ON
    COMMENT           "Building Obol (NanoRT) + bridge"
)

# -----------------------------------------------------------------------
# Sub-project 4: obol_viewer FLTK application
#
# The viewer sub-project is a thin wrapper so we can declare it as an
# ExternalProject and express the dependency on the bridge builds.
# -----------------------------------------------------------------------
ExternalProject_Add(obol_viewer_build
    SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/viewer"
    BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/viewer"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DOBOL_BRIDGE_DIR=${BRIDGE_OUTPUT_DIR}
        -DOBOL_BRIDGE_HEADER=${OBOL_SOURCE_DIR}/src/bridge/obol_bridge.h
        -DOBOL_VIEWER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../obol_viewer.cpp
    INSTALL_COMMAND   ""
    DEPENDS           obol_sys obol_osmesa obol_nanort
    LOG_CONFIGURE ON  LOG_BUILD ON
    COMMENT           "Building obol_viewer (FLTK)"
)

# -----------------------------------------------------------------------
# Convenience: copy all bridge .so files next to the viewer binary after
# the viewer build completes so it can find them with a relative path.
# -----------------------------------------------------------------------
ExternalProject_Get_Property(obol_viewer_build BINARY_DIR)
set(_viewer_bin_dir "${BINARY_DIR}")

add_custom_target(copy_bridges ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${BRIDGE_OUTPUT_DIR}/libobol_bridge_sys.so"
        "${_viewer_bin_dir}/libobol_bridge_sys.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${BRIDGE_OUTPUT_DIR}/libobol_bridge_osmesa.so"
        "${_viewer_bin_dir}/libobol_bridge_osmesa.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${BRIDGE_OUTPUT_DIR}/libobol_bridge_nanort.so"
        "${_viewer_bin_dir}/libobol_bridge_nanort.so"
    DEPENDS obol_viewer_build
    COMMENT "Copying bridge libraries next to obol_viewer"
)

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------
message(STATUS "Obol viewer superbuild configured:")
message(STATUS "  Source root : ${OBOL_SOURCE_DIR}")
message(STATUS "  Bridges dir : ${BRIDGE_OUTPUT_DIR}")
message(STATUS "  Viewer dir  : ${CMAKE_CURRENT_BINARY_DIR}/viewer")
message(STATUS "")
message(STATUS "Build with:  cmake --build ${CMAKE_CURRENT_BINARY_DIR} -j<N>")
message(STATUS "Then run:    ${CMAKE_CURRENT_BINARY_DIR}/viewer/obol_viewer")
