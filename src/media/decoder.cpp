#include "media/decoder.h"

#include <spdlog/spdlog.h>

namespace vvp::media {

Decoder::~Decoder() { Close(); }

bool Decoder::Open(const AVCodecParameters* params) {
  Close();

  if (!params) {
    spdlog::error("Decoder::Open: null codec parameters");
    return false;
  }

  const AVCodec* codec = avcodec_find_decoder(params->codec_id);
  if (!codec) {
    spdlog::error("no decoder found for codec id {}", static_cast<int>(params->codec_id));
    return false;
  }

  codec_ctx_ = avcodec_alloc_context3(codec);
  if (!codec_ctx_) {
    spdlog::error("avcodec_alloc_context3 failed");
    return false;
  }

  if (avcodec_parameters_to_context(codec_ctx_, params) < 0) {
    spdlog::error("avcodec_parameters_to_context failed");
    Close();
    return false;
  }

  if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
    spdlog::error("avcodec_open2 failed for {}", codec->name);
    Close();
    return false;
  }

  return true;
}

void Decoder::Close() { avcodec_free_context(&codec_ctx_); }

bool Decoder::SendPacket(const AVPacket* packet) {
  if (!codec_ctx_) {
    return false;
  }
  int err = avcodec_send_packet(codec_ctx_, packet);
  if (err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF) {
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, err_buf, sizeof(err_buf));
    spdlog::warn("avcodec_send_packet failed: {}", err_buf);
    return false;
  }
  return true;
}

bool Decoder::ReceiveFrame(VideoFrame* out_frame) {
  if (!codec_ctx_) {
    return false;
  }
  AVFrame* frame = av_frame_alloc();
  int err = avcodec_receive_frame(codec_ctx_, frame);
  if (err < 0) {
    av_frame_free(&frame);
    return false;  // EAGAIN (need more input) or EOF.
  }
  *out_frame = VideoFrame(frame);
  return true;
}

const char* Decoder::CodecName() const {
  return (codec_ctx_ && codec_ctx_->codec) ? codec_ctx_->codec->name : "unknown";
}

}  // namespace vvp::media
