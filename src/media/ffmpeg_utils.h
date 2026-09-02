#pragma once

// Single inclusion point for FFmpeg C headers (they must not be included
// under a namespace), with RAII helpers on top.

#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace vvp {

struct AVFormatContextDeleter {
  void operator()(AVFormatContext* p) const { avformat_close_input(&p); }
};
struct AVCodecContextDeleter {
  void operator()(AVCodecContext* p) const { avcodec_free_context(&p); }
};
struct AVFrameDeleter {
  void operator()(AVFrame* p) const { av_frame_free(&p); }
};
struct AVPacketDeleter {
  void operator()(AVPacket* p) const { av_packet_free(&p); }
};
struct SwsContextDeleter {
  void operator()(SwsContext* p) const { sws_freeContext(p); }
};
struct AVBufferRefDeleter {
  void operator()(AVBufferRef* p) const { av_buffer_unref(&p); }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

// av_make_error_string wrapper for logging.
std::string AvErrorString(int errnum);

// Monotonic clock in seconds (av_gettime wrapper).
double NowSeconds();

}  // namespace vvp
