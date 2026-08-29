cmake_minimum_required(VERSION 3.14)
project(VP9Converter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    cmake_policy(SET CMP0169 OLD)
endif()

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw)
endif()

find_package(OpenGL REQUIRED)

add_executable(VP9Converter
    src/main.cpp
    src/gui/MainWindow.cpp
    src/model/TaskManager.cpp
    src/util/FileDialog.cpp
    src/util/WinConv.cpp
    src/core/FFmpegDetector.cpp
    src/core/FFmpegBuilder.cpp
    src/core/ProcessRunner.cpp
    src/core/ProgressParser.cpp
)
target_link_libraries(VP9Converter PRIVATE imgui OpenGL::GL)
target_include_directories(VP9Converter PRIVATE src)

if(MSVC)
    target_compile_definitions(VP9Converter PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    set_target_properties(VP9Converter PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()
