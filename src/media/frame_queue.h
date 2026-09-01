#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

#include "media/video_frame.h"

namespace vvp::media {

// Thread-safe queue that keeps only the most recent frames. Pushing past
// capacity drops the oldest queued frame instead of blocking the producer —
// the real-time (CCTV) policy: never let latency build up when the consumer
// falls behind.
class FrameQueue {
 public:
  explicit FrameQueue(std::size_t capacity = 2) : capacity_(capacity) {}

  void Push(VideoFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frames_.size() >= capacity_) {
      frames_.pop_front();
      ++dropped_;
    }
    frames_.push_back(std::move(frame));
  }

  // Non-blocking pop; returns an invalid (default-constructed) frame if the
  // queue is empty.
  VideoFrame TryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frames_.empty()) {
      return VideoFrame();
    }
    VideoFrame frame = std::move(frames_.front());
    frames_.pop_front();
    return frame;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
  }

  std::size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_.size();
  }

  std::uint64_t DroppedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
  }

 private:
  mutable std::mutex mutex_;
  std::deque<VideoFrame> frames_;
  std::size_t capacity_;
  std::uint64_t dropped_ = 0;
};

}  // namespace vvp::media
