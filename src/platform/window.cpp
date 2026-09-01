#include "platform/window.h"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif

namespace vvp {

Window::~Window() { Shutdown(); }

bool Window::Init(const WindowConfig& config) {
  glfwSetErrorCallback([](int code, const char* description) {
    spdlog::error("GLFW error {}: {}", code, description != nullptr ? description : "");
  });

  if (glfwInit() == GLFW_FALSE) {
    spdlog::critical("glfwInit() failed");
    return false;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);
  if (window_ == nullptr) {
    spdlog::critical("glfwCreateWindow() failed");
    glfwTerminate();
    return false;
  }

  spdlog::info("Window created: {}x{} '{}'", config.width, config.height, config.title);
  return true;
}

void Window::Shutdown() {
  if (window_ != nullptr) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

bool Window::ShouldClose() const { return glfwWindowShouldClose(window_) != 0; }

void Window::PollEvents() { glfwPollEvents(); }

}  // namespace vvp
