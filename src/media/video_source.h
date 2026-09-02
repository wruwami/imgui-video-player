#pragma once

#include <string>

namespace vvp {

enum class HwAccelPolicy {
  kAuto,     // Try hardware decode, fall back to software automatically.
  kOff,      // Software decode only.
  kRequire,  // Hardware only; fail if unavailable.
};

enum class SourceKind {
  kFile,
  kRtsp,
  kDash,
  kUnknown,
};

struct SourceOptions {
  HwAccelPolicy hw_policy = HwAccelPolicy::kAuto;

  // Native renderer device to share with the hardware decoder (zero-copy).
  // Windows: ID3D11Device* / ID3D11DeviceContext* (kept as void* so media/
  // stays free of graphics API headers).
  void* native_d3d11_device = nullptr;
  void* native_d3d11_context = nullptr;

  // Protocol options (applied when the source kind uses them).
  std::string rtsp_transport = "tcp";  // TCP by default: stable for CCTV.
  int open_timeout_ms = 5000;
  int read_timeout_ms = 5000;
  bool minimize_buffering = true;  // Low-latency: drop excess buffering.
};

// Infers the source kind from the URL scheme.
SourceKind DetectSourceKind(const std::string& url);

}  // namespace vvp
