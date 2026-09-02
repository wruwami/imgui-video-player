#include "media/video_source.h"

#include <algorithm>
#include <cctype>

namespace vvp {
namespace {

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

SourceKind DetectSourceKind(const std::string& url) {
  const std::string lower = ToLower(url);
  if (StartsWith(lower, "rtsp://") || StartsWith(lower, "rtsps://")) {
    return SourceKind::kRtsp;
  }
  if (StartsWith(lower, "http://") || StartsWith(lower, "https://")) {
    // MPD manifests are DASH; other HTTP URLs are handled by avformat as-is.
    if (lower.find(".mpd") != std::string::npos) {
      return SourceKind::kDash;
    }
    return SourceKind::kUnknown;
  }
  return SourceKind::kFile;
}

}  // namespace vvp
