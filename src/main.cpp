#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "app/app.h"
#include "media/file_source.h"

namespace {

// Smoke-test entry point for the media pipeline (#4): opens `uri`, runs the
// demux+decode thread to completion, and reports how many frames were
// consumed. No UI involved — VideoView wiring is a separate issue (#7).
int RunMediaPipelineSmokeTest(const std::string& uri) {
  vvp::media::FileSource source;
  if (!source.Open(uri)) {
    spdlog::critical("failed to open '{}'", uri);
    return 1;
  }

  source.Start();

  std::uint64_t consumed = 0;
  while (true) {
    if (auto frame = source.Frames().TryPop(); frame.Valid()) {
      ++consumed;
      continue;
    }
    if (source.State() != vvp::media::SourceState::kRunning) {
      break;  // Decoder finished and the queue is drained.
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  source.Stop();

  spdlog::info("pipeline smoke test: {} frames consumed, {} dropped", consumed, source.Frames().DroppedCount());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

  if (argc > 1) {
    return RunMediaPipelineSmokeTest(argv[1]);
  }

  vvp::Application app;
  if (!app.Init()) {
    spdlog::critical("Application init failed");
    return 1;
  }
  app.Run();
  app.Shutdown();
  return 0;
}
