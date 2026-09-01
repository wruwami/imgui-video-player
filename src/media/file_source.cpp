#include "media/file_source.h"

#include <spdlog/spdlog.h>

namespace vvp::media {

FileSource::~FileSource() { Stop(); }

bool FileSource::Open(const std::string& uri) {
  if (!demuxer_.Open(uri)) {
    state_.store(SourceState::kError);
    return false;
  }

  const AVCodecParameters* params = demuxer_.VideoCodecParams();
  if (!decoder_.Open(params)) {
    state_.store(SourceState::kError);
    return false;
  }

  info_.width = params->width;
  info_.height = params->height;
  const AVRational tb = demuxer_.VideoTimeBase();
  info_.fps = (tb.num != 0) ? static_cast<double>(tb.den) / tb.num : 0.0;
  info_.codec_name = decoder_.CodecName();
  info_.hardware_accelerated = false;  // SW only until #5.

  state_.store(SourceState::kIdle);
  spdlog::info("opened file source '{}': {}x{} codec={}", uri, info_.width, info_.height, info_.codec_name);
  return true;
}

void FileSource::Start() {
  if (thread_.joinable()) {
    return;  // Already running.
  }
  stop_requested_.store(false);
  state_.store(SourceState::kRunning);
  thread_ = std::thread(&FileSource::Run, this);
}

void FileSource::Stop() {
  stop_requested_.store(true);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void FileSource::Run() {
  AVPacket* packet = av_packet_alloc();
  while (!stop_requested_.load()) {
    if (!demuxer_.ReadVideoPacket(packet)) {
      break;  // EOF or read error.
    }
    decoder_.SendPacket(packet);
    av_packet_unref(packet);
    DrainDecoder();
  }

  // Flush frames buffered inside the decoder.
  decoder_.SendPacket(nullptr);
  DrainDecoder();

  av_packet_free(&packet);
  state_.store(SourceState::kStopped);
}

void FileSource::DrainDecoder() {
  VideoFrame frame;
  while (decoder_.ReceiveFrame(&frame)) {
    frames_.Push(std::move(frame));
  }
}

}  // namespace vvp::media
