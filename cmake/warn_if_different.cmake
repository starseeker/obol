# warn_if_different.cmake
# Compares a generated file with a reference (source-tree) copy and prints a
# CMake WARNING (non-fatal) when they differ.
#
# Usage (from a custom command):
#   cmake -DGENERATED=<path> -DREFERENCE=<path> -P warn_if_different.cmake

if(NOT EXISTS "${GENERATED}")
  message(WARNING "Generated file does not exist: ${GENERATED}")
  return()
endif()

if(NOT EXISTS "${REFERENCE}")
  message(WARNING "Reference (source-tree) file does not exist: ${REFERENCE}")
  return()
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${GENERATED}" "${REFERENCE}"
  RESULT_VARIABLE _diff_result
)

if(_diff_result)
  message(WARNING
    "Generated file differs from source-tree copy:\n"
    "  Generated : ${GENERATED}\n"
    "  Reference : ${REFERENCE}\n"
    "Consider updating the source-tree copy if the grammar has changed."
  )
endif()
