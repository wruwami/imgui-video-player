#pragma once

// YUV -> RGB conversion constants for the NV12 presentation shader.
// Pure computation, no graphics-API or FFmpeg types: color_range/colorspace
// are the raw AVFrame enum values, passed as int so this header stays
// testable and render/media agnostic.

namespace vvp {

// Matches the HLSL cbuffer packing in video_texture.cpp: each float3 occupies
// 16 bytes; float3x3 is stored column-major in three float4 slots.
struct ConversionConstants {
  float offset[3];
  float pad0_;
  float scale[3];
  float pad1_;
  float matrix[12];
};

// Fills constants for the frame's colorspace/range. Falls back to a sensible
// default when the stream does not declare color metadata:
//   range unknown -> limited (MPEG), matrix unknown -> BT.709 for >= 720p
//   else BT.601.
void FillConversionConstants(int color_range, int colorspace, int width, int height, ConversionConstants& out);

}  // namespace vvp
