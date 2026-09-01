#include <spdlog/spdlog.h>

#include "app/app.h"

int main() {
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

  vvp::Application app;
  if (!app.Init()) {
    spdlog::critical("Application init failed");
    return 1;
  }
  app.Run();
  app.Shutdown();
  return 0;
}
