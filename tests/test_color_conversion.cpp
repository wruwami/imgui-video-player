#include <gtest/gtest.h>

#include "render/color_conversion.h"

namespace {

using vvp::ConversionConstants;

// AVFrame enum values (kept as raw ints here so the test documents the exact
// domain FillConversionConstants receives from video_texture.cpp).
constexpr int kRangeUnspecified = 0;
constexpr int kRangeMpeg = 1;  // Limited range.
constexpr int kRangeJpeg = 2;  // Full range.
constexpr int kSpaceUnspecified = 2;
constexpr int kSpaceBt709 = 1;
constexpr int kSpaceBt470Bg = 5;
constexpr int kSpaceSmpte170m = 6;

TEST(ConversionConstants, LimitedRange_UsesMpegOffsetsAndGain) {
  ConversionConstants c = {};
  vvp::FillConversionConstants(kRangeMpeg, kSpaceBt709, 1920, 1080, c);

  EXPECT_FLOAT_EQ(c.offset[0], 16.0f / 255.0f);
  EXPECT_FLOAT_EQ(c.offset[1], 128.0f / 255.0f);
  EXPECT_FLOAT_EQ(c.offset[2], 128.0f / 255.0f);
  EXPECT_FLOAT_EQ(c.scale[0], 255.0f / 219.0f);
  EXPECT_FLOAT_EQ(c.scale[1], 1.0f);
  EXPECT_FLOAT_EQ(c.scale[2], 1.0f);
}

TEST(ConversionConstants, FullRange_UsesUnityOffsetsAndGain) {
  ConversionConstants c = {};
  vvp::FillConversionConstants(kRangeJpeg, kSpaceBt709, 1920, 1080, c);

  EXPECT_FLOAT_EQ(c.offset[0], 0.0f);
  EXPECT_FLOAT_EQ(c.offset[1], 128.0f / 255.0f);
  EXPECT_FLOAT_EQ(c.scale[0], 1.0f);
  EXPECT_FLOAT_EQ(c.scale[1], 1.0f);
  EXPECT_FLOAT_EQ(c.scale[2], 1.0f);
}

TEST(ConversionConstants, Bt709Limited_MatrixIsColumnMajorPadded) {
  ConversionConstants c = {};
  vvp::FillConversionConstants(kRangeMpeg, kSpaceBt709, 1920, 1080, c);

  // Math matrix (row-major): rows are R/G/B output equations. The Y column is
  // 1.0 because HLSL already applies the limited-range gain (255/219) via
  // `scale` before the matrix multiply.
  //   [ 1.0    0      1.7927 ]
  //   [ 1.0 -0.2132  -0.5329 ]
  //   [ 1.0  2.1124   0      ]
  // Stored column-major in float4 slots: matrix[col * 4 + row].
  EXPECT_FLOAT_EQ(c.matrix[0 * 4 + 0], 1.0f);
  EXPECT_FLOAT_EQ(c.matrix[0 * 4 + 1], 1.0f);
  EXPECT_FLOAT_EQ(c.matrix[0 * 4 + 2], 1.0f);
  EXPECT_FLOAT_EQ(c.matrix[1 * 4 + 1], -0.2132f);
  EXPECT_FLOAT_EQ(c.matrix[2 * 4 + 2], 0.0f);
  EXPECT_FLOAT_EQ(c.matrix[2 * 4 + 0], 1.7927f);
  // Padding slots are unused but must not hold NaN/garbage in practice.
  EXPECT_FLOAT_EQ(c.matrix[0 * 4 + 3], 0.0f);
}

TEST(ConversionConstants, UnspecifiedMetadata_HeuristicsByResolutionAndRange) {
  // >= 720p with no metadata: BT.709 + limited.
  ConversionConstants hd = {};
  vvp::FillConversionConstants(kRangeUnspecified, kSpaceUnspecified, 1280, 720, hd);
  EXPECT_FLOAT_EQ(hd.matrix[2 * 4 + 0], 1.7927f);  // 709 limited V coefficient.

  // < 720p with no metadata: BT.601 + limited.
  ConversionConstants sd = {};
  vvp::FillConversionConstants(kRangeUnspecified, kSpaceUnspecified, 640, 480, sd);
  EXPECT_FLOAT_EQ(sd.matrix[2 * 4 + 0], 1.5960f);  // 601 limited V coefficient.
}

TEST(ConversionConstants, ExplicitBt601Metadata_OverridesResolutionHeuristic) {
  ConversionConstants c = {};
  vvp::FillConversionConstants(kRangeMpeg, kSpaceBt470Bg, 1920, 1080, c);
  EXPECT_FLOAT_EQ(c.matrix[2 * 4 + 0], 1.5960f);  // 601 despite HD resolution.
}

}  // namespace
