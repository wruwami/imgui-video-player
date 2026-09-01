cmake_minimum_required(VERSION 3.24)

project(imgui-video-player
  VERSION 0.1.0
  DESCRIPTION "Multi-channel hardware-accelerated video player"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Dear ImGui — docking branch, pinned (docking + multi-viewport are required
# and only exist on this branch; vcpkg ships master only).
# ---------------------------------------------------------------------------
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.92.9b-docking
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# vcpkg's glfw3 port exports the un-namespaced `glfw` target.
if(NOT TARGET glfw)
  message(FATAL_ERROR "glfw3 target not found")
endif()

# ---------------------------------------------------------------------------
# FFmpeg — vcpkg's ffmpeg port has no CMake config; use the pkg-config
# modules it installs instead (the vcpkg toolchain wires up PKG_CONFIG_PATH).
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)
if(WIN32)
  target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler)
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(imgui-video-player
  src/main.cpp
  src/platform/window.cpp
  src/render/d3d11_renderer.cpp
  src/app/app.cpp
  src/ui/ui.cpp
  src/media/demuxer.cpp
  src/media/decoder.cpp
  src/media/file_source.cpp)

target_include_directories(imgui-video-player PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(imgui-video-player PRIVATE
  imgui
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  PkgConfig::AVCODEC
  PkgConfig::AVFORMAT
  PkgConfig::AVUTIL)

if(MSVC)
  target_compile_options(imgui-video-player PRIVATE /W4 /utf-8 /MP)
endif()
