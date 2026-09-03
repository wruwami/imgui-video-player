#pragma once

#include <string>
#include <vector>

#include "app/channel_manager.h"
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
  void OpenSource(const std::string& url);

 private:
  bool InitImGui();
  void DrawFrame();
  SourceOptions MakeSourceOptions() const;

  // The UI currently drives channel #0 (multi-channel UI arrives with the
  // grid work, issue #9/#10).
  Channel& channel() { return *channels_.Get(0); }

  Window window_;
  D3D11Renderer renderer_;
  VideoTexture video_texture_;
  ChannelManager channels_;
  Ui ui_;
  std::vector<std::string> dropped_paths_;
  bool imgui_initialized_ = false;
};

}  // namespace vvp
