#pragma once

#include <string>

#include "media/ffmpeg_utils.h"
#include "media/video_source.h"

namespace vvp {

// Owns the AVFormatContext for one source and exposes video stream info.
// Not thread-safe: used from the channel's worker thread only.
class Demuxer {
 public:
  bool Open(const std::string& url, const SourceOptions& options);
  void Close();

  // Reads the next packet belonging to the video stream.
  // Returns false on EOF or error (check eof() to distinguish).
  bool ReadNext(AVPacket* packet);
  bool Seek(double seconds);

  // Video stream accessors (valid after Open).
  int video_stream_index() const { return video_stream_; }
  AVRational video_time_base() const { return time_base_; }
  const AVCodecParameters* video_codec_parameters() const;
  double duration_seconds() const { return duration_; }
  bool eof() const { return eof_; }

 private:
  void ApplyProtocolOptions(AVDictionary** dict) const;

  FormatContextPtr format_;
  std::string url_;
  int video_stream_ = -1;
  AVRational time_base_{};
  double duration_ = 0.0;
  bool eof_ = false;
  SourceOptions options_;
};

}  // namespace vvp
