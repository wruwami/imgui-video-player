#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "media/video_frame.h"

namespace vvp::media {

// Thin RAII wrapper around libavcodec. Software decoding only for now —
// D3D11VA hardware acceleration lands in a follow-up (#5).
class Decoder {
 public:
  Decoder() = default;
  ~Decoder();

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

  bool Open(const AVCodecParameters* params);
  void Close();

  // Feeds one packet to the decoder. `packet` may be null to flush at EOF.
  bool SendPacket(const AVPacket* packet);

  // Drains one decoded frame if available. Returns false when the decoder
  // needs more input (EAGAIN) or has been fully flushed (EOF).
  bool ReceiveFrame(VideoFrame* out_frame);

  const char* CodecName() const;

 private:
  AVCodecContext* codec_ctx_ = nullptr;
};

}  // namespace vvp::media
