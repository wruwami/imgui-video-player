#include <gtest/gtest.h>

#include "media/video_source.h"

namespace {

TEST(SourceKind, RtspSchemes_AreRtsp) {
  EXPECT_EQ(vvp::DetectSourceKind("rtsp://192.168.0.10:554/stream"), vvp::SourceKind::kRtsp);
  EXPECT_EQ(vvp::DetectSourceKind("RTSP://cam.local/live"), vvp::SourceKind::kRtsp);  // Case-insensitive.
  EXPECT_EQ(vvp::DetectSourceKind("rtsps://secure.cam/stream"), vvp::SourceKind::kRtsp);
}

TEST(SourceKind, MpdManifest_IsDash) {
  EXPECT_EQ(vvp::DetectSourceKind("http://cdn.example.com/live.mpd"), vvp::SourceKind::kDash);
  EXPECT_EQ(vvp::DetectSourceKind("HTTPS://cdn.example.com/live.MPD"), vvp::SourceKind::kDash);
}

TEST(SourceKind, NonMpdHttp_IsUnknown) {
  EXPECT_EQ(vvp::DetectSourceKind("http://cdn.example.com/video.mp4"), vvp::SourceKind::kUnknown);
  EXPECT_EQ(vvp::DetectSourceKind("https://example.com/hls.m3u8"), vvp::SourceKind::kUnknown);
}

TEST(SourceKind, EverythingElse_IsFile) {
  EXPECT_EQ(vvp::DetectSourceKind("C:/videos/sample.mp4"), vvp::SourceKind::kFile);
  EXPECT_EQ(vvp::DetectSourceKind("/mnt/nas/cam01.mkv"), vvp::SourceKind::kFile);
  EXPECT_EQ(vvp::DetectSourceKind("sample.mp4"), vvp::SourceKind::kFile);
  EXPECT_EQ(vvp::DetectSourceKind(""), vvp::SourceKind::kFile);
}

}  // namespace
