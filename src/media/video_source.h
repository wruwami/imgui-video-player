#pragma once

#include <string>

#include "media/frame_queue.h"

namespace vvp::media {

struct VideoSourceInfo {
  int width = 0;
  int height = 0;
  double fps = 0.0;
  std::string codec_name;
  bool hardware_accelerated = false;  // Always false until D3D11VA lands (#5).
};

enum class SourceState {
  kIdle,     // Opened, not yet started.
  kRunning,  // Demux+decode thread active.
  kStopped,  // Reached EOF or was stopped.
  kError,    // Failed to open or decode.
};

// Abstraction over a video source (file / rtsp / dash — see subclasses like
// FileSource). Implementations own a per-channel demux+decode thread and
// publish decoded frames to a FrameQueue that the render thread polls.
class IVideoSource {
 public:
  virtual ~IVideoSource() = default;

  virtual bool Open(const std::string& uri) = 0;
  virtual void Start() = 0;
  virtual void Stop() = 0;

  virtual SourceState State() const = 0;
  virtual const VideoSourceInfo& Info() const = 0;
  virtual FrameQueue& Frames() = 0;
};

}  // namespace vvp::media
