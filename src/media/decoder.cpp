#include "media/decoder.h"

#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <d3d11.h>
#include <libavutil/hwcontext_d3d11va.h>
#endif

namespace vvp {
namespace {

#if defined(_WIN32)
inline constexpr AVPixelFormat kHwPixelFormat = AV_PIX_FMT_D3D11;
inline constexpr const char* kHwBackendName = "d3d11va";
inline constexpr AVHWDeviceType kHwDeviceType = AV_HWDEVICE_TYPE_D3D11VA;
#else
inline constexpr AVPixelFormat kHwPixelFormat = AV_PIX_FMT_NONE;
inline constexpr const char* kHwBackendName = "";
inline constexpr AVHWDeviceType kHwDeviceType = AV_HWDEVICE_TYPE_NONE;
#endif

}  // namespace

Decoder::~Decoder() { Close(); }

bool Decoder::OpenHwDevice() {
  if (kHwDeviceType == AV_HWDEVICE_TYPE_NONE) {
    return false;
  }

  // Prefer sharing the renderer's D3D11 device so decoded textures can be
  // sampled directly (zero-copy). Falls back to a private FFmpeg device.
  if (options_.native_d3d11_device != nullptr) {
    AVBufferRef* shared = av_hwdevice_ctx_alloc(kHwDeviceType);
    if (shared == nullptr) {
      spdlog::warn("Decoder: av_hwdevice_ctx_alloc({}) failed", kHwBackendName);
    } else {
      auto* dev_ctx = reinterpret_cast<AVHWDeviceContext*>(shared->data);
      auto* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(dev_ctx->hwctx);
      // AddRef: FFmpeg releases these refs on device uninit.
      auto* device = static_cast<ID3D11Device*>(options_.native_d3d11_device);
      auto* context = static_cast<ID3D11DeviceContext*>(options_.native_d3d11_context);
      device->AddRef();
      if (context != nullptr) {
        context->AddRef();
      }
      d3d11->device = device;
      d3d11->device_context = context;
      int err = av_hwdevice_ctx_init(shared);
      if (err < 0) {
        spdlog::warn("Decoder: av_hwdevice_ctx_init (shared device) failed: {}", AvErrorString(err));
        device->Release();
        if (context != nullptr) {
          context->Release();
        }
        av_buffer_unref(&shared);
      } else {
        spdlog::info("Decoder: sharing renderer D3D11 device for {}", kHwBackendName);
        hw_device_ = shared;
        return true;
      }
    }
  }

  int err = av_hwdevice_ctx_create(&hw_device_, kHwDeviceType, nullptr, nullptr, 0);
  if (err < 0) {
    spdlog::warn("Decoder: av_hwdevice_ctx_create({}) failed: {}", kHwBackendName, AvErrorString(err));
    hw_device_ = nullptr;
    return false;
  }
  return true;
}

bool Decoder::Open(const AVCodecParameters* params, const AVRational& time_base, const SourceOptions& options) {
  Close();
  options_ = options;

  if (params == nullptr || params->codec_type != AVMEDIA_TYPE_VIDEO) {
    spdlog::error("Decoder: invalid codec parameters");
    return false;
  }

  const AVCodec* codec = avcodec_find_decoder(params->codec_id);
  if (codec == nullptr) {
    spdlog::error("Decoder: no decoder found for codec_id {}", static_cast<int>(params->codec_id));
    return false;
  }

  AVCodecContext* ctx = avcodec_alloc_context3(codec);
  if (ctx == nullptr) {
    spdlog::error("Decoder: avcodec_alloc_context3 failed");
    return false;
  }
  codec_.reset(ctx);

  int err = avcodec_parameters_to_context(codec_.get(), params);
  if (err < 0) {
    spdlog::error("Decoder: avcodec_parameters_to_context failed: {}", AvErrorString(err));
    Close();
    return false;
  }
  codec_->time_base = time_base;
  codec_->opaque = this;
  codec_->get_format = &Decoder::GetFormatTrampoline;

  bool hw_requested = options_.hw_policy != HwAccelPolicy::kOff;
  if (hw_requested && kHwDeviceType != AV_HWDEVICE_TYPE_NONE) {
    if (OpenHwDevice()) {
      // Attach the device; PickPixelFormat() still allows a software fallback
      // when the codec rejects the hardware pixel format (kAuto), and
      // kRequire errors out there when no hw format is offered.
      codec_->hw_device_ctx = av_buffer_ref(hw_device_);
    } else if (options_.hw_policy == HwAccelPolicy::kRequire) {
      spdlog::error("Decoder: hardware acceleration required but unavailable");
      Close();
      return false;
    }
  } else if (options_.hw_policy == HwAccelPolicy::kRequire) {
    spdlog::error("Decoder: hardware acceleration required but not supported on this platform");
    Close();
    return false;
  }

  err = avcodec_open2(codec_.get(), codec, nullptr);
  if (err < 0) {
    // One retry without hardware: a stale driver or blocked session can fail
    // the actual decoder open even though the device was created. Not allowed
    // under kRequire — that must fail loudly instead.
    if (codec_->hw_device_ctx != nullptr && options_.hw_policy != HwAccelPolicy::kRequire) {
      spdlog::warn("Decoder: avcodec_open2 with {} failed ({}), retrying with software", kHwBackendName,
                   AvErrorString(err));
      codec_.reset(avcodec_alloc_context3(codec));
      codec_->opaque = this;
      codec_->get_format = &Decoder::GetFormatTrampoline;
      avcodec_parameters_to_context(codec_.get(), params);
      codec_->time_base = time_base;
      err = avcodec_open2(codec_.get(), codec, nullptr);
    }
    if (err < 0) {
      spdlog::error("Decoder: avcodec_open2 failed: {}", AvErrorString(err));
      Close();
      return false;
    }
  }

  info_.codec_name = codec->name;
  info_.hardware = false;  // Finalized lazily once the first frame arrives
  info_.hw_backend.clear();
  info_.width = codec_->width;
  info_.height = codec_->height;
  info_finalized_ = false;

  // FFmpeg defers get_format/frames allocation to the first decoded frame, so
  // pre-create the hardware frames context to control bind flags: decoded
  // surfaces must be shader-sampleable for zero-copy presentation.
  if (codec_->hw_device_ctx != nullptr) {
    AVBufferRef* frames_ref = av_hwframe_ctx_alloc(codec_->hw_device_ctx);
    if (frames_ref != nullptr) {
      auto* frames_ctx = reinterpret_cast<AVHWFramesContext*>(frames_ref->data);
      frames_ctx->format = kHwPixelFormat;
      frames_ctx->sw_format = AV_PIX_FMT_NV12;  // 8-bit planar (TODO: P010 for 10-bit HEVC).
      frames_ctx->width = codec_->coded_width;
      frames_ctx->height = codec_->coded_height;
      frames_ctx->initial_pool_size = 0;  // Decoder default.

      auto* d3d11_frames = reinterpret_cast<AVD3D11VAFramesContext*>(frames_ctx->hwctx);
      d3d11_frames->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;

      int ferr = av_hwframe_ctx_init(frames_ref);
      if (ferr < 0) {
        spdlog::warn("Decoder: hw frames ctx init failed: {} (using decoder defaults)", AvErrorString(ferr));
        av_buffer_unref(&frames_ref);
      } else {
        codec_->hw_frames_ctx = av_buffer_ref(frames_ref);
        spdlog::info("Decoder: hw frames context ready (shader-sampleable NV12)");
        av_buffer_unref(&frames_ref);
      }
    }
  }

  spdlog::info("Decoder: {} opened ({}x{})", info_.codec_name, info_.width, info_.height);
  return true;
}

AVPixelFormat Decoder::PickPixelFormat(AVCodecContext* ctx, const AVPixelFormat* formats) {
  const bool hw_ok = ctx->hw_device_ctx != nullptr;
  if (hw_ok) {
    for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
      if (*p == kHwPixelFormat) {
        info_pix_fmt_is_hw_ = true;
        spdlog::info("Decoder: hardware pixel format selected ({})", av_get_pix_fmt_name(*p));
        return *p;
      }
    }
  }
  if (options_.hw_policy == HwAccelPolicy::kRequire) {
    spdlog::error("Decoder: hardware pixel format not offered by decoder");
    return AV_PIX_FMT_NONE;
  }
  // Software fallback: first non-hardware format (skip hw surface formats).
  for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*p);
    if (desc != nullptr && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) == 0) {
      spdlog::info("Decoder: falling back to software pixel format {}", av_get_pix_fmt_name(*p));
      info_pix_fmt_is_hw_ = false;
      return *p;
    }
  }
  return formats[0];  // Last resort: accept the decoder's default.
}

void Decoder::Close() {
  codec_.reset();
  if (hw_device_ != nullptr) {
    av_buffer_unref(&hw_device_);
    hw_device_ = nullptr;
  }
  info_ = DecoderInfo{};
  info_pix_fmt_is_hw_ = false;
  info_finalized_ = false;
}

void Decoder::Flush() {
  if (codec_ != nullptr) {
    avcodec_flush_buffers(codec_.get());
  }
}

bool Decoder::Send(const AVPacket* packet) {
  int err = avcodec_send_packet(codec_.get(), packet);
  if (err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF) {
    spdlog::warn("Decoder: avcodec_send_packet failed: {}", AvErrorString(err));
    return false;
  }
  return true;
}

Decoder::Status Decoder::Receive(FramePtr& frame_out) {
  FramePtr frame(av_frame_alloc());
  int err = avcodec_receive_frame(codec_.get(), frame.get());
  if (err == AVERROR(EAGAIN)) {
    return Status::kAgain;
  }
  if (err == AVERROR_EOF) {
    return Status::kAgain;  // Drained.
  }
  if (err < 0) {
    spdlog::warn("Decoder: avcodec_receive_frame failed: {}", AvErrorString(err));
    return Status::kError;
  }

  // The decode path is only known once the first frame exists (get_format is
  // deferred past avcodec_open2).
  if (!info_finalized_) {
    info_finalized_ = true;
    info_.hardware = frame->format == kHwPixelFormat;
    info_.hw_backend = info_.hardware ? kHwBackendName : "";
    spdlog::info("Decoder: {} decoding via {}", info_.codec_name, info_.hardware ? info_.hw_backend : "software");
  }

  frame_out = std::move(frame);
  return Status::kFrame;
}

}  // namespace vvp
