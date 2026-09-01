#pragma once

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace vvp::media {

// Thin RAII wrapper around libavformat: opens a source and reads packets
// from its best video stream, skipping other streams (audio, etc).
class Demuxer {
 public:
  Demuxer() = default;
  ~Demuxer();

  Demuxer(const Demuxer&) = delete;
  Demuxer& operator=(const Demuxer&) = delete;

  bool Open(const std::string& uri);
  void Close();

  // Reads the next packet belonging to the video stream. Returns false on
  // EOF or read error; `packet` is left unreferenced in that case.
  bool ReadVideoPacket(AVPacket* packet);

  const AVCodecParameters* VideoCodecParams() const;
  AVRational VideoTimeBase() const;
  int VideoStreamIndex() const { return video_stream_index_; }

 private:
  AVFormatContext* format_ctx_ = nullptr;
  int video_stream_index_ = -1;
};

}  // namespace vvp::media
