#pragma once

extern "C" {
#include <libavutil/frame.h>
}

namespace vvp::media {

// RAII owner of a decoded AVFrame. Move-only so a frame has exactly one
// owner as it travels from Decoder to FrameQueue to consumer.
class VideoFrame {
 public:
  VideoFrame() = default;
  explicit VideoFrame(AVFrame* frame) : frame_(frame) {}
  ~VideoFrame() { Reset(); }

  VideoFrame(const VideoFrame&) = delete;
  VideoFrame& operator=(const VideoFrame&) = delete;

  VideoFrame(VideoFrame&& other) noexcept : frame_(other.frame_) { other.frame_ = nullptr; }
  VideoFrame& operator=(VideoFrame&& other) noexcept {
    if (this != &other) {
      Reset();
      frame_ = other.frame_;
      other.frame_ = nullptr;
    }
    return *this;
  }

  bool Valid() const { return frame_ != nullptr; }
  AVFrame* Get() const { return frame_; }

  int Width() const { return frame_ ? frame_->width : 0; }
  int Height() const { return frame_ ? frame_->height : 0; }
  int Format() const { return frame_ ? frame_->format : -1; }  // AVPixelFormat
  int64_t Pts() const { return frame_ ? frame_->pts : AV_NOPTS_VALUE; }

 private:
  void Reset() {
    if (frame_) {
      av_frame_free(&frame_);
    }
  }

  AVFrame* frame_ = nullptr;
};

}  // namespace vvp::media
