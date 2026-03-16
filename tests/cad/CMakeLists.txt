# =============================================================================
# tests/cad/CMakeLists.txt
#
# Unit tests for the CAD assembly subsystem:
#   - test_cad_ids        : ID generation and hashing
#   - test_segment_lod    : SegmentPopLod POP-quantisation
#   - test_triangle_lod   : TrianglePopLod POP-quantisation
#   - test_cad_picking    : CPU BVH ray-picking
#
# All tests are standalone (no GL context, no BRL-CAD dependency).
# =============================================================================

# Common include paths for CAD tests
set(CAD_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/tests                       # test_utils.h
    ${PROJECT_SOURCE_DIR}/include                   # Inventor headers
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    # Internal CAD headers needed directly by tests
    ${PROJECT_SOURCE_DIR}/src/cad/lod
    ${PROJECT_SOURCE_DIR}/src/cad/picking
    ${PROJECT_SOURCE_DIR}/src/cad
)

# ---------------------------------------------------------------------------
# test_cad_ids
# ---------------------------------------------------------------------------
add_executable(test_cad_ids test_cad_ids.cpp)
target_link_libraries(test_cad_ids
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_cad_ids PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_cad_ids COMMAND test_cad_ids)

# ---------------------------------------------------------------------------
# test_segment_lod
# ---------------------------------------------------------------------------
add_executable(test_segment_lod test_segment_lod.cpp)
target_link_libraries(test_segment_lod
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_segment_lod PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_segment_lod COMMAND test_segment_lod)

# ---------------------------------------------------------------------------
# test_triangle_lod
# ---------------------------------------------------------------------------
add_executable(test_triangle_lod test_triangle_lod.cpp)
target_link_libraries(test_triangle_lod
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_triangle_lod PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_triangle_lod COMMAND test_triangle_lod)

# ---------------------------------------------------------------------------
# test_cad_picking
# ---------------------------------------------------------------------------
add_executable(test_cad_picking test_cad_picking.cpp)
target_link_libraries(test_cad_picking
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_cad_picking PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_cad_picking COMMAND test_cad_picking)
