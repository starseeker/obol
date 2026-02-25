# =============================================================================
# tests/obol_superbuild/viewer/CMakeLists.txt
#
# Builds the obol_viewer FLTK executable.
# Called as an ExternalProject from the superbuild; all configuration is
# passed via -D options so this file requires no hard-coded paths.
#
# Variables injected by the superbuild:
#   OBOL_BRIDGE_DIR     – directory containing libobol_bridge_*.so files
#   OBOL_BRIDGE_HEADER  – path to obol_bridge.h
#   OBOL_VIEWER_SOURCE  – path to obol_viewer.cpp
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(ObolViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- FLTK -------------------------------------------------------------------
find_package(FLTK REQUIRED)

# ---- Sources / includes -----------------------------------------------------

get_filename_component(_bridge_hdr_dir "${OBOL_BRIDGE_HEADER}" DIRECTORY)

add_executable(obol_viewer "${OBOL_VIEWER_SOURCE}")

target_include_directories(obol_viewer PRIVATE
    "${_bridge_hdr_dir}"     # obol_bridge.h
    ${FLTK_INCLUDE_DIR}
)

# The viewer loads bridges at runtime via dlopen; only FLTK and libdl
# are linked at build time.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(obol_viewer ${FLTK_LIBRARIES} dl)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(obol_viewer ${FLTK_LIBRARIES})
else()
    target_link_libraries(obol_viewer ${FLTK_LIBRARIES})
endif()

# Embed the bridge directory so the viewer can find the .so files at startup
# (the superbuild also copies them next to the binary as a fallback).
target_compile_definitions(obol_viewer PRIVATE
    OBOL_BRIDGE_DIR="${OBOL_BRIDGE_DIR}"
)

# Set the output directory to the current binary dir (same level as where
# the superbuild will copy the bridge .so files).
set_target_properties(obol_viewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

message(STATUS "obol_viewer will load bridges from: ${OBOL_BRIDGE_DIR}")
