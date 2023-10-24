set(LIBRARY_SOURCES)

list(
    APPEND LIBRARY_SOURCES

    "${CMAKE_CURRENT_SOURCE_DIR}/BSplineCurve.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BSplineCurve.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ControlPoint.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ControlPoint.cpp"
)

set(LIBRARY_NAME "Shared")
add_library(${LIBRARY_NAME} ${LIBRARY_SOURCES})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")