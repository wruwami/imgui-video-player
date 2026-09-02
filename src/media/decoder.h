#pragma once

#include <string>

#include "media/ffmpeg_utils.h"
#include "media/video_source.h"

namespace vvp {

struct DecoderInfo {
  std::string codec_name;  // e.g. "h264"
  std::string hw_backend;  // "d3d11va" when hardware, empty when software.
  bool hardware = false;
  int width = 0;
  int height = 0;
};

// Wraps one AVCodecContext for the video stream. Tries hardware decoding
// (D3D11VA on Windows) according to the policy and falls back to software
// automatically. Decoded frames may be hardware surfaces (AV_PIX_FMT_D3D11).
class Decoder {
 public:
  ~Decoder();

  bool Open(const AVCodecParameters* params, const AVRational& time_base, const SourceOptions& options);
  void Close();

  void Flush();  // avcodec_flush_buffers (after seeks).

  // Sends a packet; an empty AVPacket (no data) flushes the decoder at EOF.
  bool Send(const AVPacket* packet);

  enum class Status {
    kFrame,
    kAgain,  // Needs more input; call Send() first.
    kError,
  };
  Status Receive(FramePtr& frame_out);

  const DecoderInfo& info() const { return info_; }

 private:
  AVPixelFormat PickPixelFormat(AVCodecContext* ctx, const AVPixelFormat* formats);

  static enum AVPixelFormat GetFormatTrampoline(AVCodecContext* ctx, const AVPixelFormat* formats) {
    auto* self = static_cast<Decoder*>(ctx->opaque);
    return self->PickPixelFormat(ctx, formats);
  }

  bool OpenHwDevice();

  CodecContextPtr codec_;
  AVBufferRef* hw_device_ = nullptr;
  DecoderInfo info_;
  SourceOptions options_;
  // Set by the get_format callback; the final path is confirmed on the first
  // decoded frame (get_format is deferred past avcodec_open2).
  bool info_pix_fmt_is_hw_ = false;
  bool info_finalized_ = false;
};

}  // namespace vvp
