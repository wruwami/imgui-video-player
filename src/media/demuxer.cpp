#include "media/demuxer.h"

#include <spdlog/spdlog.h>

namespace vvp::media {

Demuxer::~Demuxer() { Close(); }

bool Demuxer::Open(const std::string& uri) {
  Close();

  AVFormatContext* ctx = nullptr;
  if (int err = avformat_open_input(&ctx, uri.c_str(), nullptr, nullptr); err < 0) {
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, err_buf, sizeof(err_buf));
    spdlog::error("avformat_open_input('{}') failed: {}", uri, err_buf);
    return false;
  }
  format_ctx_ = ctx;

  if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
    spdlog::error("avformat_find_stream_info('{}') failed", uri);
    Close();
    return false;
  }

  video_stream_index_ = av_find_best_stream(format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (video_stream_index_ < 0) {
    spdlog::error("no video stream found in '{}'", uri);
    Close();
    return false;
  }

  return true;
}

void Demuxer::Close() {
  if (format_ctx_) {
    avformat_close_input(&format_ctx_);
  }
  video_stream_index_ = -1;
}

bool Demuxer::ReadVideoPacket(AVPacket* packet) {
  if (!format_ctx_) {
    return false;
  }
  while (true) {
    int err = av_read_frame(format_ctx_, packet);
    if (err < 0) {
      return false;  // EOF or read error.
    }
    if (packet->stream_index == video_stream_index_) {
      return true;
    }
    av_packet_unref(packet);  // Not the video stream (e.g. audio) — skip.
  }
}

const AVCodecParameters* Demuxer::VideoCodecParams() const {
  if (!format_ctx_ || video_stream_index_ < 0) {
    return nullptr;
  }
  return format_ctx_->streams[video_stream_index_]->codecpar;
}

AVRational Demuxer::VideoTimeBase() const {
  if (!format_ctx_ || video_stream_index_ < 0) {
    return AVRational{0, 1};
  }
  return format_ctx_->streams[video_stream_index_]->time_base;
}

}  // namespace vvp::media
