#include <spdlog/spdlog.h>

#include "app/app.h"

// Usage: imgui-video-player.exe [url]
// An optional url/file starts playing immediately after launch.
int main(int argc, char** argv) {
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

  vvp::Application app;
  if (!app.Init()) {
    spdlog::critical("Application init failed");
    return 1;
  }
  if (argc > 1) {
    app.OpenSource(argv[1]);
  }
  app.Run();
  app.Shutdown();
  return 0;
}
