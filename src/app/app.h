#pragma once

#include <string>
#include <vector>

#include "media/channel.h"
#include "platform/window.h"
#include "render/d3d11_renderer.h"
#include "render/video_texture.h"
#include "ui/ui.h"

namespace vvp {

// Owns the platform window, renderer, media channels and Dear ImGui context;
// runs the main loop with docking + multi-viewport enabled.
class Application {
 public:
  Application() = default;
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Init();
  void Run();
  void Shutdown();

  // Starts playing a url/file immediately (e.g. from the command line).
  void OpenSource(const std::string& url) { channel_.Open(url, MakeSourceOptions()); }

 private:
  bool InitImGui();
  void DrawFrame();
  SourceOptions MakeSourceOptions() const;

  Window window_;
  D3D11Renderer renderer_;
  VideoTexture video_texture_;
  Channel channel_;
  Ui ui_;
  std::vector<std::string> dropped_paths_;
  bool imgui_initialized_ = false;
};

}  // namespace vvp
