#pragma once

#include <memory>

#include "media/ffmpeg_utils.h"

namespace vvp {

// Immutable decoded video frame wrapping an owning AVFrame.
// May reference a hardware surface: AV_PIX_FMT_D3D11 frames carry
// data[0] = ID3D11Texture2D* (texture array) and data[1] = array slice index.
class VideoFrame {
 public:
  VideoFrame(FramePtr frame, double pts_seconds) : frame_(std::move(frame)), pts_seconds_(pts_seconds) {}

  const AVFrame* av_frame() const { return frame_.get(); }
  double pts_seconds() const { return pts_seconds_; }
  int width() const { return frame_ ? frame_->width : 0; }
  int height() const { return frame_ ? frame_->height : 0; }
  AVPixelFormat pixel_format() const { return frame_ ? static_cast<AVPixelFormat>(frame_->format) : AV_PIX_FMT_NONE; }
  bool IsHardware() const { return pixel_format() == AV_PIX_FMT_D3D11; }

 private:
  FramePtr frame_;
  double pts_seconds_ = 0.0;
};

using VideoFramePtr = std::shared_ptr<const VideoFrame>;

}  // namespace vvp
