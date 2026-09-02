#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

#include "media/frame.h"

namespace vvp {

// Bounded frame queue with a drop-oldest policy: a realtime viewer must never
// build a backlog, so when the consumer lags, the oldest frames are dropped.
class FrameQueue {
 public:
  explicit FrameQueue(size_t capacity);

  void Push(VideoFramePtr frame);  // Drops the oldest frame when full.
  VideoFramePtr PopLatest();       // Drains the queue, returns the newest frame.
  void Clear();
  void Close();  // Unblocks waiting consumers.

  bool IsClosed() const;
  size_t Size();
  uint64_t DroppedCount() const;

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<VideoFramePtr> frames_;
  size_t capacity_;
  bool closed_ = false;
  uint64_t dropped_ = 0;
};

}  // namespace vvp
