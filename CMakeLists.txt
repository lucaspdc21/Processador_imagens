cmake_minimum_required(VERSION 3.15)
project(Processador LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(core
    src/Image.cpp
    src/ColorImage.cpp
    src/ColorSpace.cpp
    src/ImageColor.cpp
    src/ImageLoad.cpp
    src/ImageWarp.cpp
    src/ImageScale.cpp
    src/ImageRotate.cpp
    src/FrequencyTransform.cpp
    src/FrequencyFilters.cpp
    src/FrequencyMask.cpp
    src/FrequencySession.cpp
    src/ImageQuantization.cpp
    src/ImageResample.cpp
    src/ImageStats.cpp
    src/ImageMetrics.cpp
    src/ImageIntensity.cpp
    src/ImageHistogram.cpp
    src/ImageConvolution.cpp
    src/ImageSpatialFilters.cpp
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

# --- Threads (DFT/FFT em paralelo e worker da GUI) --------------------------
find_package(Threads REQUIRED)
target_link_libraries(core PUBLIC Threads::Threads)

# --- FFT da biblioteca (pocketfft, header-only, BSD) -------------------------
# Usada somente no modo "FFT da biblioteca" e como oraculo nos testes; a DFT
# ingenua e a FFT radix-2 sao codigo proprio. Se o download falhar (sem
# internet), o programa compila normalmente e esse modo fica desabilitado.
# Para usar uma copia local: -DPOCKETFFT_INCLUDE_DIR=/caminho/da/pasta
option(PROCESSADOR_USE_POCKETFFT "Habilita o modo 'FFT da biblioteca' (pocketfft)" ON)
if(PROCESSADOR_USE_POCKETFFT)
    include(FetchContent)
    if(NOT POCKETFFT_INCLUDE_DIR)
        FetchContent_Declare(
            pocketfft
            GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
            GIT_TAG        cpp
            GIT_SHALLOW    TRUE
        )
        # So baixa (nao ha nada a compilar: e header-only).
        FetchContent_GetProperties(pocketfft)
        if(NOT pocketfft_POPULATED)
            FetchContent_Populate(pocketfft)
        endif()
        set(POCKETFFT_INCLUDE_DIR ${pocketfft_SOURCE_DIR})
    endif()
    if(EXISTS ${POCKETFFT_INCLUDE_DIR}/pocketfft_hdronly.hpp)
        target_include_directories(core PUBLIC ${POCKETFFT_INCLUDE_DIR})
        target_compile_definitions(core PUBLIC HAVE_POCKETFFT POCKETFFT_NO_MULTITHREADING)
        message(STATUS "FFT da biblioteca: pocketfft em ${POCKETFFT_INCLUDE_DIR}")
    else()
        message(WARNING "pocketfft_hdronly.hpp nao encontrado: o modo 'FFT da biblioteca' ficara desabilitado.")
    endif()
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
#set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
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
    ${IMGUI_DIR}/imgui_demo.cpp
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
    ui/SpatialWindow.cpp
    ui/FrequencyWindow.cpp
    ui/RgbaTexture.cpp
    ui/AppState.cpp
    ui/ImageTexture.cpp
    ui/LeftPanel.cpp
    ui/CenterPanel.cpp
    ui/RightPanel.cpp
    ui/CompareWindow.cpp
    ui/ConvolutionWindow.cpp
)
target_include_directories(processador_gui PRIVATE ${TINYFD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/ui)
target_link_libraries(processador_gui PRIVATE core imgui tinyfiledialogs)

# Testes do nucleo de cor (RGB <-> HSI, cinza, chroma-key...): ./build/color_tests
add_executable(color_tests tests/color_tests.cpp)
target_link_libraries(color_tests PRIVATE core)

# ---------------------------------------------------------------------------
# Testes automatizados (sem dependencias externas):
#   cmake -S . -B build && cmake --build build --target run_tests
# ---------------------------------------------------------------------------
enable_testing()
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp)
set(TEST_TARGETS "")
foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} EXCLUDE_FROM_ALL ${test_src})
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(${test_name} PRIVATE core)
    add_test(NAME ${test_name} COMMAND ${test_name})
    list(APPEND TEST_TARGETS ${test_name})
endforeach()
add_custom_target(run_tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure DEPENDS ${TEST_TARGETS})
