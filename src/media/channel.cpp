#include "media/channel.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <utility>

#include "media/ffmpeg_utils.h"

namespace vvp {
namespace {

constexpr double kIdleSleepSeconds = 0.01;     // Poll interval while paused.
constexpr double kMaxPaceSleepSeconds = 0.25;  // Never sleep too far ahead.
constexpr double kFpsSmoothing = 0.9;          // EMA factor for decode-fps estimate.

}  // namespace

Channel::Channel() = default;

Channel::~Channel() { Close(); }

bool Channel::Open(const std::string& url, const SourceOptions& options) {
  Close();
  url_ = url;
  options_ = options;

  {
    std::lock_guard<std::mutex> lock(info_mutex_);
    decoder_info_ = DecoderInfo{};
    error_.clear();
    width_ = 0;
    height_ = 0;
    duration_ = 0.0;
  }
  position_.store(0.0, std::memory_order_relaxed);
  paused_.store(false, std::memory_order_relaxed);
  speed_.store(1.0, std::memory_order_relaxed);
  pending_seek_.store(-1.0, std::memory_order_relaxed);
  frames_decoded_.store(0, std::memory_order_relaxed);
  decoded_fps_.store(0.0, std::memory_order_relaxed);

  running_.store(true, std::memory_order_relaxed);
  SetState(ChannelState::kOpening);
  worker_ = std::thread(&Channel::WorkerLoop, this);
  spdlog::info("Channel: opening '{}'", url);
  return true;
}

void Channel::Close() {
  if (running_.exchange(false) && worker_.joinable()) {
    queue_.Close();
    worker_.join();
  }
  queue_.Clear();
  SetState(ChannelState::kIdle);
}

void Channel::Play() {
  if (state_.load(std::memory_order_relaxed) == ChannelState::kPaused) {
    paused_.store(false, std::memory_order_relaxed);
    resume_clock_reset_.store(true, std::memory_order_relaxed);  // Rebase the clock.
    SetState(ChannelState::kPlaying);
  }
}

void Channel::Pause() {
  if (state_.load(std::memory_order_relaxed) == ChannelState::kPlaying) {
    paused_.store(true, std::memory_order_relaxed);
    SetState(ChannelState::kPaused);
  }
}

void Channel::TogglePlay() {
  if (paused_.load(std::memory_order_relaxed)) {
    Play();
  } else {
    Pause();
  }
}

void Channel::Seek(double seconds) {
  const double dur = duration();
  if (dur > 0.0) {
    seconds = std::clamp(seconds, 0.0, dur);
  }
  pending_seek_.store(std::max(0.0, seconds), std::memory_order_relaxed);
  if (state_.load(std::memory_order_relaxed) == ChannelState::kEnded) {
    SetState(ChannelState::kPlaying);
  }
}

void Channel::SetSpeed(double speed) { speed_.store(std::clamp(speed, 0.25, 4.0), std::memory_order_relaxed); }

double Channel::duration() const {
  std::lock_guard<std::mutex> lock(info_mutex_);
  return duration_;
}

ChannelStats Channel::stats() const {
  std::lock_guard<std::mutex> lock(info_mutex_);
  ChannelStats s;
  s.decoder = decoder_info_;
  s.error = error_;
  s.width = width_;
  s.height = height_;
  s.duration = duration_;
  s.decoded_fps = decoded_fps_.load(std::memory_order_relaxed);
  s.frames_decoded = frames_decoded_.load(std::memory_order_relaxed);
  return s;
}

void Channel::SetError(const std::string& message) {
  {
    std::lock_guard<std::mutex> lock(info_mutex_);
    error_ = message;
  }
  spdlog::error("Channel '{}': {}", url_, message);
  SetState(ChannelState::kError);
}

void Channel::WorkerLoop() {
  spdlog::info("Channel '{}': worker started", url_);

  Demuxer demuxer;
  Decoder decoder;

  if (!demuxer.Open(url_, options_)) {
    SetError("failed to open source");
    running_.store(false, std::memory_order_relaxed);
    return;
  }
  if (!decoder.Open(demuxer.video_codec_parameters(), demuxer.video_time_base(), options_)) {
    SetError("failed to open decoder");
    running_.store(false, std::memory_order_relaxed);
    return;
  }
  const double time_base = av_q2d(demuxer.video_time_base());
  {
    std::lock_guard<std::mutex> lock(info_mutex_);
    decoder_info_ = decoder.info();
    width_ = demuxer.video_codec_parameters()->width;
    height_ = demuxer.video_codec_parameters()->height;
    duration_ = demuxer.duration_seconds();
  }

  // Realtime pacing: wall_time = clock_base + (pts - pts_base) / speed.
  double clock_base = 0.0;
  double pts_base = 0.0;
  bool clock_set = false;

  double last_frame_wall = NowSeconds();
  double fps = 0.0;

  PacketPtr packet(av_packet_alloc());
  bool input_eof = false;

  SetState(paused_.load(std::memory_order_relaxed) ? ChannelState::kPaused : ChannelState::kPlaying);

  while (running_.load(std::memory_order_relaxed)) {
    if (paused_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::duration<double>(kIdleSleepSeconds));
      continue;
    }
    if (resume_clock_reset_.exchange(false)) {
      clock_set = false;
    }

    // Pending seek: flush both sides of the pipeline, rebase the clock.
    const double seek_target = pending_seek_.exchange(-1.0, std::memory_order_relaxed);
    if (seek_target >= 0.0) {
      if (demuxer.Seek(seek_target)) {
        decoder.Flush();
        queue_.Clear();
        position_.store(seek_target, std::memory_order_relaxed);
        clock_set = false;
        input_eof = false;
        spdlog::info("Channel '{}': seek to {:.2f}s", url_, seek_target);
      }
    }

    // Read one packet (or hit EOF).
    if (!input_eof) {
      if (demuxer.ReadNext(packet.get())) {
        if (!decoder.Send(packet.get())) {
          SetError("decoder send failed");
          break;
        }
        av_packet_unref(packet.get());
      } else if (demuxer.eof()) {
        input_eof = true;
        decoder.Send(nullptr);  // Flush packet: drain remaining frames.
      } else {
        SetError("demuxer read failed");
        break;
      }
    }

    // Receive all frames the decoder has ready.
    bool decode_error = false;
    while (running_.load(std::memory_order_relaxed)) {
      FramePtr raw(av_frame_alloc());
      const Decoder::Status status = decoder.Receive(raw);
      if (status == Decoder::Status::kAgain) {
        break;
      }
      if (status == Decoder::Status::kError) {
        decode_error = true;  // Tolerate single bad packets; keep reading.
        break;
      }

      const double pts = raw->best_effort_timestamp != AV_NOPTS_VALUE ? raw->best_effort_timestamp * time_base
                                                                      : position_.load(std::memory_order_relaxed);
      position_.store(pts, std::memory_order_relaxed);

      // Pace file playback to realtime; live sources arrive paced already.
      if (duration_ > 0.0) {
        const double now = NowSeconds();
        if (!clock_set) {
          clock_base = now;
          pts_base = pts;
          clock_set = true;
        } else {
          const double target = clock_base + (pts - pts_base) / speed_.load(std::memory_order_relaxed);
          const double wait = std::min(target - now, kMaxPaceSleepSeconds);
          if (wait > 0.002) {
            std::this_thread::sleep_for(std::chrono::duration<double>(wait));
          }
        }
      }

      // Decode fps estimate (EMA over wall-clock deltas).
      const double now = NowSeconds();
      const double delta = now - last_frame_wall;
      last_frame_wall = now;
      if (delta > 0.0) {
        fps = fps * kFpsSmoothing + (1.0 / std::max(delta, 1e-6)) * (1.0 - kFpsSmoothing);
        decoded_fps_.store(fps, std::memory_order_relaxed);
      }
      frames_decoded_.fetch_add(1, std::memory_order_relaxed);

      queue_.Push(std::make_shared<const VideoFrame>(FramePtr(raw.release()), pts));
    }
    if (decode_error) {
      continue;
    }

    if (input_eof) {
      SetState(ChannelState::kEnded);
      break;
    }
  }

  queue_.Close();
  spdlog::info("Channel '{}': worker stopped", url_);
}

}  // namespace vvp
