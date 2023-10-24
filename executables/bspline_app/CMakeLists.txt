########################################

set(EXECUTABLE "bSpline")

set(EXECUTABLE_RESOURCES)

list(
    APPEND EXECUTABLE_RESOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/BSplineMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BSplineApp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BSplineApp.hpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/BSplineCurve.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/BSplineCurve.hpp"
)

add_executable(${EXECUTABLE} ${EXECUTABLE_RESOURCES})

########################################