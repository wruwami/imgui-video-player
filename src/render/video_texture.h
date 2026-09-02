#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "media/frame.h"

namespace vvp {

// Renders the latest decoded frame into a GPU texture that ImGui can display.
//
// Two conversion paths, chosen per frame:
//  - Hardware frames (AV_PIX_FMT_D3D11): the decoder's NV12 texture array is
//    sampled directly (slice SRVs) and converted YUV->RGB by a pixel shader
//    into a per-channel render target. Zero GPU->CPU copies.
//  - Software frames: sws_scale to RGBA + texture upload (fallback path).
class VideoTexture {
 public:
  VideoTexture() = default;
  ~VideoTexture() { Shutdown(); }

  VideoTexture(const VideoTexture&) = delete;
  VideoTexture& operator=(const VideoTexture&) = delete;

  bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
  void Shutdown();

  // Call once per rendered frame on the render thread. Safe with nullptr.
  void Update(const VideoFramePtr& frame);

  bool valid() const { return output_srv_ != nullptr; }
  int width() const { return width_; }
  int height() const { return height_; }
  bool last_frame_hardware() const { return last_frame_hardware_; }

  // ImTextureID for ImGui::Image (ID3D11ShaderResourceView*).
  ID3D11ShaderResourceView* handle() const { return output_srv_.Get(); }

 private:
  bool EnsureShaders();
  bool EnsureRenderTarget(int width, int height);
  bool EnsureSwTexture(int width, int height);
  bool EnsureCopyTexture(int width, int height);
  bool ConvertHardwareFrame(const AVFrame* frame);
  bool ConvertSoftwareFrame(const AVFrame* frame);

  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;

  // Fullscreen-triangle pass for YUV->RGB.
  Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
  Microsoft::WRL::ComPtr<ID3D11Buffer> conversion_cbuffer_;

  // Hardware path output.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_rtv_;

  // Software path output.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> sw_texture_;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sw_srv_;

  // Hardware path: when decoder surfaces are not shader-bindable (FFmpeg
  // default bind flags), a GPU-side NV12 copy is sampled instead.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> copy_texture_;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> copy_srv_y_;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> copy_srv_uv_;
  int copy_width_ = 0;
  int copy_height_ = 0;

  // Currently displayed SRV (render target or sw texture).
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> output_srv_;

  SwsContextPtr sws_;
  std::vector<uint8_t> sw_buffer_;

  int width_ = 0;
  int height_ = 0;
  bool last_frame_hardware_ = false;
};

}  // namespace vvp
