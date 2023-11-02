set(LIBRARY_SOURCES)

list(
    APPEND LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/SurfaceMeshRenderer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SurfaceMeshRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CurtainMeshRenderer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CurtainMeshRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Curve.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Curve.cpp"
)

set(LIBRARY_NAME "Shared")
add_library(${LIBRARY_NAME} ${LIBRARY_SOURCES})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")