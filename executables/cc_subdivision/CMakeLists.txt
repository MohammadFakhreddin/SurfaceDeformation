########################################

set(EXECUTABLE "CC_SubdivisonApp")

set(EXECUTABLE_RESOURCES)

list(
    APPEND EXECUTABLE_RESOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/CC_SubdivisionMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CC_SubdivisionApp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CC_SubdivisionApp.hpp"
)

add_executable(${EXECUTABLE} ${EXECUTABLE_RESOURCES})

########################################