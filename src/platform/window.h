#pragma once

struct GLFWwindow;

namespace vvp {

struct WindowConfig {
  int width = 1600;
  int height = 900;
  const char* title = "imgui-video-player";
};

// RAII wrapper around a GLFW window. Uses GLFW_NO_API so the rendering
// backend owns the graphics context (D3D11 on Windows).
class Window {
 public:
  Window() = default;
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool Init(const WindowConfig& config);
  void Shutdown();

  bool ShouldClose() const;
  GLFWwindow* Handle() const { return window_; }

  static void PollEvents();

 private:
  GLFWwindow* window_ = nullptr;
};

}  // namespace vvp
