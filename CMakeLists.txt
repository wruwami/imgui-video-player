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
# FFmpeg (via vcpkg; no official CMake config, resolve explicitly)
# ---------------------------------------------------------------------------
find_path(FFMPEG_INCLUDE_DIR NAMES libavformat/avformat.h)
foreach(_ffmpeg_lib avcodec avformat avutil swscale swresample)
  find_library(FFMPEG_${_ffmpeg_lib}_LIBRARY NAMES ${_ffmpeg_lib})
  if(NOT FFMPEG_${_ffmpeg_lib}_LIBRARY)
    message(FATAL_ERROR "FFmpeg library not found: ${_ffmpeg_lib}")
  endif()
  list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${_ffmpeg_lib}_LIBRARY})
endforeach()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(imgui-video-player
  src/main.cpp
  src/platform/window.cpp
  src/app/channel_manager.cpp
  src/app/channel_manager.cpp
  src/media/ffmpeg_utils.cpp
  src/media/frame_queue.cpp
  src/media/video_source.cpp
  src/media/demuxer.cpp
  src/media/decoder.cpp
  src/media/channel.cpp
  src/render/d3d11_renderer.cpp
  src/render/color_conversion.cpp
  src/render/video_texture.cpp
  src/app/app.cpp
  src/ui/ui.cpp
  src/ui/video_view.cpp)

target_include_directories(imgui-video-player PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${FFMPEG_INCLUDE_DIR})

target_link_libraries(imgui-video-player PRIVATE
  imgui
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  ${FFMPEG_LIBRARIES})

if(MSVC)
  target_compile_options(imgui-video-player PRIVATE /W4 /utf-8 /MP)
  target_compile_definitions(imgui-video-player PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ---------------------------------------------------------------------------
# Unit tests (GoogleTest). Logic only — no GPU, no media file fixtures.
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)  # gtest_discover_tests

  add_executable(unit_tests
    tests/test_frame_queue.cpp
    tests/test_source_kind.cpp
    tests/test_color_conversion.cpp
    tests/test_channel_manager.cpp
    src/app/channel_manager.cpp
    src/media/channel.cpp
    src/media/decoder.cpp
    src/media/demuxer.cpp
    src/media/ffmpeg_utils.cpp
    src/media/frame_queue.cpp
    src/media/video_source.cpp
    src/render/color_conversion.cpp)

  target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIR})
  target_link_libraries(unit_tests PRIVATE GTest::gtest_main spdlog::spdlog ${FFMPEG_LIBRARIES})

  if(MSVC)
    target_compile_options(unit_tests PRIVATE /W4 /utf-8 /MP)
    target_compile_definitions(unit_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()

  gtest_discover_tests(unit_tests)
endif()
