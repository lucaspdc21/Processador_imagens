cmake_minimum_required(VERSION 3.15)
project(Processador LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(core
    src/Image.cpp
    src/ImageLoad.cpp
    src/ImageQuantization.cpp
    src/ImageResample.cpp
    src/ImageStats.cpp
    src/ImageMetrics.cpp
    src/ImageIntensity.cpp
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

option(BUILD_GUI "Compila a interface grafica (precisa de GLFW, OpenGL e internet na primeira vez)" ON)
option(BUILD_TESTS "Compila e registra os testes do nucleo (CTest)" ON)

if(BUILD_TESTS)
    enable_testing()
    add_executable(core_tests tests/test_core.cpp)
    target_link_libraries(core_tests PRIVATE core)
    add_test(NAME core_tests COMMAND core_tests)
endif()

if(NOT BUILD_GUI)
    return()
endif()

find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(glfw)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
set(TINYFD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyfiledialogs)
add_library(tinyfiledialogs STATIC
    ${TINYFD_DIR}/tinyfiledialogs.c
)
target_include_directories(tinyfiledialogs PUBLIC ${TINYFD_DIR})

add_executable(processador_gui
    ui/main.cpp
    ui/AppState.cpp
    ui/ImageTexture.cpp
    ui/LeftPanel.cpp
    ui/CenterPanel.cpp
    ui/RightPanel.cpp
    ui/CompareWindow.cpp
)
target_include_directories(processador_gui PRIVATE
    ${TINYFD_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include
)
target_link_libraries(processador_gui PRIVATE core imgui tinyfiledialogs)