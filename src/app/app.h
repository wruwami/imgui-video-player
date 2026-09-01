#pragma once

#include "platform/window.h"
#include "render/d3d11_renderer.h"
#include "ui/ui.h"

namespace vvp {

// Owns the platform window, renderer and Dear ImGui context; runs the main
// loop with docking + multi-viewport enabled.
class Application {
 public:
  Application() = default;
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Init();
  void Run();
  void Shutdown();

 private:
  bool InitImGui();
  void DrawFrame();

  Window window_;
  D3D11Renderer renderer_;
  Ui ui_;
  bool imgui_initialized_ = false;
};

}  // namespace vvp
