#include "media/frame_queue.h"

namespace vvp {

FrameQueue::FrameQueue(size_t capacity) : capacity_(capacity > 0 ? capacity : 1) {}

void FrameQueue::Push(VideoFramePtr frame) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      return;
    }
    if (frames_.size() >= capacity_) {
      frames_.pop_front();
      ++dropped_;
    }
    frames_.push_back(std::move(frame));
  }
  cv_.notify_one();
}

VideoFramePtr FrameQueue::PopLatest() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (frames_.empty()) {
    return nullptr;
  }
  VideoFramePtr newest = std::move(frames_.back());
  dropped_ += frames_.size() - 1;  // Older unconsumed frames count as dropped.
  frames_.clear();
  return newest;
}

void FrameQueue::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  dropped_ += frames_.size();
  frames_.clear();
}

void FrameQueue::Close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  cv_.notify_all();
}

bool FrameQueue::IsClosed() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return closed_;
}

size_t FrameQueue::Size() {
  std::lock_guard<std::mutex> lock(mutex_);
  return frames_.size();
}

uint64_t FrameQueue::DroppedCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dropped_;
}

}  // namespace vvp
