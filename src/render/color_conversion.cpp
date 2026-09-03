#include "render/color_conversion.h"

#include <cstring>

namespace vvp {

void FillConversionConstants(int color_range, int colorspace, int width, int height, ConversionConstants& c) {
  // AVFrame color range: AVCOL_RANGE_JPEG = 2 (full), others = limited.
  const bool is_full_range = color_range == 2;
  if (is_full_range) {
    c.offset[0] = 0.0f;
    c.offset[1] = 128.0f / 255.0f;
    c.offset[2] = 128.0f / 255.0f;
    c.scale[0] = 1.0f;
    c.scale[1] = 1.0f;
    c.scale[2] = 1.0f;
  } else {
    c.offset[0] = 16.0f / 255.0f;
    c.offset[1] = 128.0f / 255.0f;
    c.offset[2] = 128.0f / 255.0f;
    c.scale[0] = 255.0f / 219.0f;
    c.scale[1] = 1.0f;
    c.scale[2] = 1.0f;
  }

  // Pick the YUV matrix. AVColorSpace values used here:
  //   BT709 = 1, BT470BG = 5, SMPTE170M = 6 (BT.601), UNSPECIFIED = 2/NONE = 0.
  bool bt709 = height >= 720;
  switch (colorspace) {
    case 1:  // AVCOL_SPC_BT709
      bt709 = true;
      break;
    case 5:  // AVCOL_SPC_BT470BG
    case 6:  // AVCOL_SPC_SMPTE170M
      bt709 = false;
      break;
    default:
      break;
  }

  // Rows of Y'UV -> RGB with Y' in [0,1], UV in [-0.5,0.5] after scaling.
  float m[3][3];
  if (bt709) {
    if (is_full_range) {
      const float v[3][3] = {{1.0f, 0.0f, 1.5748f}, {1.0f, -0.1873f, -0.4681f}, {1.0f, 1.8556f, 0.0f}};
      std::memcpy(m, v, sizeof(m));
    } else {
      // HLSL already applies the limited-range Y gain (255/219) via `scale`
      // before the matrix multiply, so the Y column here stays 1.0 to avoid
      // double-applying it.
      const float v[3][3] = {{1.0f, 0.0f, 1.7927f}, {1.0f, -0.2132f, -0.5329f}, {1.0f, 2.1124f, 0.0f}};
      std::memcpy(m, v, sizeof(m));
    }
  } else {
    if (is_full_range) {
      const float v[3][3] = {{1.0f, 0.0f, 1.4020f}, {1.0f, -0.3441f, -0.7141f}, {1.0f, 1.7720f, 0.0f}};
      std::memcpy(m, v, sizeof(m));
    } else {
      const float v[3][3] = {{1.0f, 0.0f, 1.5960f}, {1.0f, -0.3918f, -0.8130f}, {1.0f, 2.0172f, 0.0f}};
      std::memcpy(m, v, sizeof(m));
    }
  }
  // HLSL float3x3 is column-major in memory; transpose rows into columns.
  for (int col = 0; col < 3; ++col) {
    for (int row = 0; row < 3; ++row) {
      c.matrix[col * 4 + row] = m[row][col];
    }
  }
}

}  // namespace vvp
