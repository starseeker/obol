# =============================================================================
# src/bridge/CMakeLists.txt
#
# Builds one libobol_bridge_<suffix>.so per backend.
#
# Symbol isolation strategy
# ─────────────────────────
# The Obol (Coin) shared library already uses different output names for
# each backend:
#   • libCoin.so          (COIN3D_USE_OSMESA=OFF, system GL)
#   • libCoin-osmesa.so   (COIN3D_USE_OSMESA=ON,  OSMesa;
#                          GL functions mangled to mgl* via USE_MGL_NAMESPACE)
#
# Both bridge .so files are compiled with -fvisibility=hidden + a GNU
# version script (obol_bridge.map) so they export ONLY the obol_* C
# functions and no C++ Coin symbols.
#
# The viewer loads both with dlopen(RTLD_LOCAL).  Because the two Coin
# libraries have different sonames they are always loaded as separate
# objects; RTLD_LOCAL keeps their symbols private so even if both bridges
# export the same obol_* names, each bridge's obol_* functions resolve to
# its own embedded Coin instance.
#
# Usage from the top-level project (after Coin is configured):
#   cmake ... -DCOIN_BUILD_BRIDGE=ON
# The bridge suffix is set automatically from COIN3D_USE_OSMESA.
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# ---- Options ----------------------------------------------------------------

set(COIN_BRIDGE_SUFFIX "sys" CACHE STRING
    "Bridge library suffix (sys | osmesa).  Set by the top-level project.")

set(COIN_BRIDGE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE PATH
    "Directory where the bridge .so will be placed.")

# ---- OSMesa detection -------------------------------------------------------

if(DEFINED COIN3D_USE_OSMESA AND COIN3D_USE_OSMESA)
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

# OSMesa-specific settings
if(_bridge_osmesa)
    target_compile_definitions(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
        COIN3D_OSMESA_BUILD)
    set(_osmesa_inc "${CMAKE_SOURCE_DIR}/external/osmesa/include")
    if(EXISTS "${_osmesa_inc}")
        target_include_directories(obol_bridge_${COIN_BRIDGE_SUFFIX} PRIVATE
            "${_osmesa_inc}")
    endif()
    if(TARGET osmesa_interface)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} osmesa_interface)
    elseif(TARGET osmesa)
        target_link_libraries(obol_bridge_${COIN_BRIDGE_SUFFIX} osmesa)
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
