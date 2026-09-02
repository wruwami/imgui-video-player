#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "media/decoder.h"
#include "media/demuxer.h"
#include "media/frame.h"
#include "media/frame_queue.h"

namespace vvp {

enum class ChannelState {
  kIdle,     // Not opened.
  kOpening,  // Worker is probing/opening the source.
  kPlaying,
  kPaused,
  kEnded,  // File reached EOF.
  kError,
};

struct ChannelStats {
  DecoderInfo decoder;
  std::string error;
  int width = 0;
  int height = 0;
  double duration = 0.0;     // 0 for live sources.
  double decoded_fps = 0.0;  // Smoothed decode rate.
  uint64_t frames_decoded = 0;
};

// One playback channel: source -> decoder -> frame queue, running on its own
// worker thread. Frames are paced to realtime (respecting speed), so the
// consumer can always render "the latest frame" — the viewer never backlogs.
class Channel {
 public:
  Channel();
  ~Channel();

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  bool Open(const std::string& url, const SourceOptions& options = {});
  void Close();

  void Play();
  void Pause();
  void TogglePlay();
  void Seek(double seconds);
  void SetSpeed(double speed);
  double speed() const { return speed_.load(std::memory_order_relaxed); }

  ChannelState state() const { return state_.load(std::memory_order_relaxed); }
  double position() const { return position_.load(std::memory_order_relaxed); }
  double duration() const;

  // Snapshot for the UI (thread-safe).
  ChannelStats stats() const;

  // Render thread: returns the newest frame and drains the rest (drop policy).
  VideoFramePtr LatestFrame() { return queue_.PopLatest(); }

 private:
  void WorkerLoop();

  void SetState(ChannelState s) { state_.store(s, std::memory_order_relaxed); }
  void SetError(const std::string& message);

  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<ChannelState> state_{ChannelState::kIdle};
  std::atomic<bool> paused_{false};
  std::atomic<bool> resume_clock_reset_{false};
  std::atomic<double> speed_{1.0};
  std::atomic<double> pending_seek_{-1.0};
  std::atomic<double> position_{0.0};

  // Decode counters (hot path, lock-free).
  std::atomic<uint64_t> frames_decoded_{0};
  std::atomic<double> decoded_fps_{0.0};

  FrameQueue queue_{4};

  // Immutable after open; guarded for snapshot reads during reopen.
  mutable std::mutex info_mutex_;
  DecoderInfo decoder_info_;
  std::string error_;
  int width_ = 0;
  int height_ = 0;
  double duration_ = 0.0;

  std::string url_;
  SourceOptions options_;
};

}  // namespace vvp
