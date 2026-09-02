#include "media/demuxer.h"

#include <spdlog/spdlog.h>

namespace vvp {

void Demuxer::ApplyProtocolOptions(AVDictionary** dict) const {
  const SourceKind kind = DetectSourceKind(url_);
  if (kind == SourceKind::kRtsp) {
    av_dict_set(dict, "rtsp_transport", options_.rtsp_transport.c_str(), 0);
    if (options_.read_timeout_ms > 0) {
      // Microseconds on older FFmpeg, milliseconds tolerated via listen/timeout
      // naming differences; "timeout" accepts us for RTSP/UDP, keep conservative.
      av_dict_set(dict, "timeout", std::to_string(static_cast<long long>(options_.read_timeout_ms) * 1000).c_str(), 0);
    }
    if (options_.minimize_buffering) {
      av_dict_set(dict, "buffer_size", "65536", 0);
      av_dict_set(dict, "max_delay", "0", 0);
    }
  }
}

bool Demuxer::Open(const std::string& url, const SourceOptions& options) {
  Close();
  url_ = url;
  options_ = options;
  eof_ = false;

  AVDictionary* dict = nullptr;
  ApplyProtocolOptions(&dict);
  AVFormatContext* ctx = nullptr;
  int err = avformat_open_input(&ctx, url.c_str(), nullptr, &dict);
  const bool options_consumed = av_dict_count(dict) == 0;
  av_dict_free(&dict);
  if (err < 0) {
    spdlog::error("Demuxer: avformat_open_input('{}') failed: {}", url, AvErrorString(err));
    return false;
  }
  if (!options_consumed) {
    spdlog::warn("Demuxer: some protocol options were not consumed");
  }
  format_.reset(ctx);

  err = avformat_find_stream_info(format_.get(), nullptr);
  if (err < 0) {
    spdlog::error("Demuxer: avformat_find_stream_info failed: {}", AvErrorString(err));
    Close();
    return false;
  }

  video_stream_ = av_find_best_stream(format_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (video_stream_ < 0) {
    spdlog::error("Demuxer: no video stream found in '{}'", url);
    Close();
    return false;
  }

  const AVStream* stream = format_->streams[video_stream_];
  time_base_ = stream->time_base;
  duration_ = format_->duration > 0 ? format_->duration * av_q2d(AV_TIME_BASE_Q) : 0.0;

  spdlog::info("Demuxer: '{}' opened (video stream #{}, {}x{}, duration {:.2f}s)", url, video_stream_,
               stream->codecpar->width, stream->codecpar->height, duration_);
  return true;
}

void Demuxer::Close() {
  format_.reset();
  video_stream_ = -1;
  eof_ = false;
}

bool Demuxer::ReadNext(AVPacket* packet) {
  if (format_ == nullptr) {
    return false;
  }
  while (true) {
    int err = av_read_frame(format_.get(), packet);
    if (err < 0) {
      if (err == AVERROR_EOF || avio_feof(format_->pb)) {
        eof_ = true;
      } else {
        spdlog::warn("Demuxer: av_read_frame failed: {}", AvErrorString(err));
      }
      return false;
    }
    if (packet->stream_index == video_stream_) {
      return true;
    }
    av_packet_unref(packet);  // Skip audio/subtitle packets.
  }
}

bool Demuxer::Seek(double seconds) {
  if (format_ == nullptr) {
    return false;
  }
  eof_ = false;
  const int64_t ts = static_cast<int64_t>(seconds / av_q2d(time_base_));
  // Backwards-capable seek; any keyframe within range is accepted.
  int err = avformat_seek_file(format_.get(), video_stream_, INT64_MIN, ts, ts, 0);
  if (err < 0) {
    spdlog::warn("Demuxer: seek to {:.2f}s failed: {}", seconds, AvErrorString(err));
    return false;
  }
  return true;
}

const AVCodecParameters* Demuxer::video_codec_parameters() const {
  if (video_stream_ < 0) {
    return nullptr;
  }
  return format_->streams[video_stream_]->codecpar;
}

}  // namespace vvp
