cmake_minimum_required(VERSION 3.21...4.1.2)

project(motionblur)

set(CMAKE_CXX_STANDARD 26)

set(CPM_SOURCE_CACHE ${PROJECT_SOURCE_DIR}/cpm-cache CACHE PATH "")
include(cmake/CPM.cmake)

add_library(motionblur SHARED
    src/gamewindow.hpp
    src/main.cpp
    src/menu.hpp
    src/motionblur.cpp
    src/motionblur.hpp
)

target_compile_options(motionblur PRIVATE
    -Wall
    -Wextra
    -Wconversion
    -Wpedantic
)

CPMAddPackage("gh:stephenberry/glaze@7.5.0")
target_link_libraries(motionblur PRIVATE
    EGL
    GLESv2
    log
    glaze::glaze
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(motionblur PRIVATE
        -flto
    )
    target_link_options(motionblur PRIVATE
        -flto
        -Wl,--version-script,${PROJECT_SOURCE_DIR}/map.txt
        -Wl,--no-undefined-version
    )
    set_target_properties(motionblur PROPERTIES
        LINK_DEPENDS ${PROJECT_SOURCE_DIR}/map.txt
    )
endif()

install(TARGETS motionblur LIBRARY DESTINATION mods)
