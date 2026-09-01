#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "media/decoder.h"
#include "media/demuxer.h"
#include "media/video_source.h"

namespace vvp::media {

// IVideoSource backed by a local file (or anything libavformat can open by
// path/URI). Runs a single demux+decode thread per the architecture's
// per-channel threading model; consumers only poll Frames().
class FileSource : public IVideoSource {
 public:
  FileSource() = default;
  ~FileSource() override;

  FileSource(const FileSource&) = delete;
  FileSource& operator=(const FileSource&) = delete;

  bool Open(const std::string& uri) override;
  void Start() override;
  void Stop() override;

  SourceState State() const override { return state_.load(); }
  const VideoSourceInfo& Info() const override { return info_; }
  FrameQueue& Frames() override { return frames_; }

 private:
  void Run();
  void DrainDecoder();

  Demuxer demuxer_;
  Decoder decoder_;
  FrameQueue frames_;
  VideoSourceInfo info_;
  std::atomic<SourceState> state_{SourceState::kIdle};
  std::thread thread_;
  std::atomic<bool> stop_requested_{false};
};

}  // namespace vvp::media
