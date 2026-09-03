#include "render/video_texture.h"

#include <d3dcompiler.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <utility>

#include "media/ffmpeg_utils.h"
#include "render/color_conversion.h"

namespace vvp {
namespace {

// Fullscreen triangle; no vertex buffer needed (SV_VertexID).
constexpr char kVertexShaderSource[] = R"(
struct VSOut {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD0;
};

VSOut main(uint vid : SV_VertexID) {
  VSOut o;
  float2 uv = float2((vid << 1) & 2, vid & 2);
  o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
  o.uv = uv;
  return o;
}
)";

// NV12 -> RGBA. Plane SRVs are bound at t0 (Y, R8) and t1 (UV, R8G8).
constexpr char kPixelShaderSource[] = R"(
Texture2D y_tex : register(t0);
Texture2D uv_tex : register(t1);
SamplerState linear_sampler : register(s0);

cbuffer ConversionCb : register(b0) {
  float3 offset;       // pre-scale offset (16/255, 128/255, 128/255 when limited)
  float3 scale;        // pre-scale gain (255/219, 1, 1 when limited)
  float3x3 yuv_matrix; // Y'UV -> RGB matrix (applied to scaled values)
};

struct PSIn {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target {
  float y = y_tex.Sample(linear_sampler, i.uv).r;
  float2 uv = uv_tex.Sample(linear_sampler, i.uv).rg;
  float3 yuv = (float3(y, uv) - offset) * scale;
  float3 rgb = mul(yuv_matrix, yuv);
  return float4(saturate(rgb), 1);
}
)";

// Matches HLSL cbuffer packing: each float3 occupies 16 bytes; float3x3 is
// stored column-major in three float4 slots.
// Color metadata snapshot used to detect conversion-constant changes.
struct FrameMeta {
  int color_range;
  int colorspace;
  int width;
  int height;
  bool operator==(const FrameMeta& o) const {
    return color_range == o.color_range && colorspace == o.colorspace && width == o.width && height == o.height;
  }
};

Microsoft::WRL::ComPtr<ID3D11Buffer> MakeConstantBuffer(ID3D11Device* device, UINT size) {
  D3D11_BUFFER_DESC desc = {};
  desc.ByteWidth = size;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
  if (FAILED(device->CreateBuffer(&desc, nullptr, &buffer))) {
    return nullptr;
  }
  return buffer;
}

}  // namespace

bool VideoTexture::Init(ID3D11Device* device, ID3D11DeviceContext* context) {
  device_ = device;
  context_ = context;
  if (!EnsureShaders()) {
    Shutdown();
    return false;
  }
  return true;
}

void VideoTexture::Shutdown() {
  vertex_shader_.Reset();
  pixel_shader_.Reset();
  conversion_cbuffer_.Reset();
  render_target_rtv_.Reset();
  render_target_.Reset();
  output_srv_.Reset();
  sw_srv_.Reset();
  sw_texture_.Reset();
  copy_srv_y_.Reset();
  copy_srv_uv_.Reset();
  copy_texture_.Reset();
  sws_.reset();
  sw_buffer_.clear();
  sw_buffer_.shrink_to_fit();
  width_ = 0;
  height_ = 0;
  device_ = nullptr;
  context_ = nullptr;
}

bool VideoTexture::EnsureShaders() {
  Microsoft::WRL::ComPtr<ID3DBlob> blob;
  Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
  HRESULT hr = D3DCompile(kVertexShaderSource, sizeof(kVertexShaderSource), nullptr, nullptr, nullptr, "main", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &error_blob);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: vertex shader compile failed: 0x{:08X}{}{}", static_cast<unsigned>(hr),
                  error_blob != nullptr ? " - " : "",
                  error_blob != nullptr ? static_cast<const char*>(error_blob->GetBufferPointer()) : "");
    return false;
  }
  error_blob.Reset();
  hr = device_->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vertex_shader_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: CreateVertexShader failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }

  hr = D3DCompile(kPixelShaderSource, sizeof(kPixelShaderSource), nullptr, nullptr, nullptr, "main", "ps_5_0",
                  D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &error_blob);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: pixel shader compile failed: 0x{:08X}{}{}", static_cast<unsigned>(hr),
                  error_blob != nullptr ? " - " : "",
                  error_blob != nullptr ? static_cast<const char*>(error_blob->GetBufferPointer()) : "");
    return false;
  }
  hr = device_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &pixel_shader_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: CreatePixelShader failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }

  conversion_cbuffer_ = MakeConstantBuffer(device_, sizeof(ConversionConstants));
  if (conversion_cbuffer_ == nullptr) {
    spdlog::error("VideoTexture: constant buffer creation failed");
    return false;
  }

  D3D11_SAMPLER_DESC samp = {};
  samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  samp.ComparisonFunc = D3D11_COMPARISON_NEVER;
  samp.MaxLOD = D3D11_FLOAT32_MAX;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
  if (FAILED(device_->CreateSamplerState(&samp, &sampler))) {
    spdlog::error("VideoTexture: CreateSamplerState failed");
    return false;
  }
  ID3D11SamplerState* s = sampler.Get();
  context_->PSSetSamplers(0, 1, &s);
  return true;
}

bool VideoTexture::EnsureRenderTarget(int width, int height) {
  if (width == width_ && height == height_ && render_target_ != nullptr) {
    return true;
  }
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

  render_target_rtv_.Reset();
  output_srv_.Reset();
  render_target_.Reset();
  HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &render_target_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: render target {}x{} failed: 0x{:08X}", width, height, static_cast<unsigned>(hr));
    return false;
  }
  hr = device_->CreateRenderTargetView(render_target_.Get(), nullptr, &render_target_rtv_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: CreateRenderTargetView failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }
  width_ = width;
  height_ = height;
  return true;
}

bool VideoTexture::EnsureSwTexture(int width, int height) {
  if (width == width_ && height == height_ && sw_texture_ != nullptr) {
    return true;
  }
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  output_srv_.Reset();
  sw_srv_.Reset();
  sw_texture_.Reset();
  HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &sw_texture_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: sw texture {}x{} failed: 0x{:08X}", width, height, static_cast<unsigned>(hr));
    return false;
  }
  hr = device_->CreateShaderResourceView(sw_texture_.Get(), nullptr, &sw_srv_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: CreateShaderResourceView failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }
  width_ = width;
  height_ = height;
  return true;
}

bool VideoTexture::ConvertHardwareFrame(const AVFrame* frame) {
  auto* texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
  const intptr_t slice = reinterpret_cast<intptr_t>(frame->data[1]);
  if (texture == nullptr) {
    return false;
  }
  if (!EnsureRenderTarget(frame->width, frame->height)) {
    return false;
  }

  // Preferred path: sample the decoder's texture array directly (zero copy).
  ID3D11ShaderResourceView* srvs[2] = {nullptr, nullptr};
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> y_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uv_srv;
  if (copy_texture_ == nullptr) {
    D3D11_TEX2D_ARRAY_SRV array_srv = {};
    array_srv.MostDetailedMip = 0;
    array_srv.MipLevels = 1;
    array_srv.FirstArraySlice = static_cast<UINT>(slice);
    array_srv.ArraySize = 1;

    D3D11_SHADER_RESOURCE_VIEW_DESC y_desc = {};
    y_desc.Format = DXGI_FORMAT_R8_UNORM;
    y_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    y_desc.Texture2DArray = array_srv;

    D3D11_SHADER_RESOURCE_VIEW_DESC uv_desc = {};
    uv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
    uv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    uv_desc.Texture2DArray = array_srv;

    HRESULT srv_err = device_->CreateShaderResourceView(texture, &y_desc, &y_srv);
    if (SUCCEEDED(srv_err)) {
      srv_err = device_->CreateShaderResourceView(texture, &uv_desc, &uv_srv);
    }
    if (SUCCEEDED(srv_err)) {
      srvs[0] = y_srv.Get();
      srvs[1] = uv_srv.Get();
    } else if (srv_err != DXGI_ERROR_MORE_DATA) {
      // Decoder surfaces are not shader-bindable: fall back to a GPU copy.
      static bool copy_mode_logged = false;
      if (!copy_mode_logged) {
        spdlog::info("VideoTexture: decoder surfaces not SRV-bindable (0x{:08X}), using GPU copy path",
                     static_cast<unsigned>(srv_err));
        copy_mode_logged = true;
      }
    }
  }

  if (srvs[0] == nullptr) {
    // GPU copy path: copy the used slice into our own shader-bindable NV12
    // texture (GPU-side only; no CPU round-trip).
    if (!EnsureCopyTexture(frame->width, frame->height)) {
      return false;
    }
    context_->CopySubresourceRegion(copy_texture_.Get(), 0, 0, 0, 0, texture, static_cast<UINT>(slice), nullptr);
    srvs[0] = copy_srv_y_.Get();
    srvs[1] = copy_srv_uv_.Get();
  }

  // Update color conversion constants when metadata changed.
  static FrameMeta last_meta{-1, -1, -1, -1};
  const FrameMeta meta{frame->color_range, frame->colorspace, frame->width, frame->height};
  if (!(meta == last_meta)) {
    ConversionConstants c = {};
    FillConversionConstants(frame->color_range, frame->colorspace, frame->width, frame->height, c);
    context_->UpdateSubresource(conversion_cbuffer_.Get(), 0, nullptr, &c, 0, 0);
    last_meta = meta;
  }

  context_->OMSetRenderTargets(1, render_target_rtv_.GetAddressOf(), nullptr);
  D3D11_VIEWPORT vp = {0.0f, 0.0f, static_cast<float>(frame->width), static_cast<float>(frame->height), 0.0f, 1.0f};
  context_->RSSetViewports(1, &vp);
  context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
  context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
  context_->PSSetConstantBuffers(0, 1, conversion_cbuffer_.GetAddressOf());
  context_->PSSetShaderResources(0, 2, srvs);
  context_->Draw(3, 0);

  // Unbind: leaving the SRVs bound while ImGui uses the RT as SRV would hazard.
  ID3D11ShaderResourceView* null_srvs[2] = {nullptr, nullptr};
  context_->PSSetShaderResources(0, 2, null_srvs);

  output_srv_.Reset();
  HRESULT hr = device_->CreateShaderResourceView(render_target_.Get(), nullptr, &output_srv_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: RT SRV creation failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }
  return true;
}

bool VideoTexture::EnsureCopyTexture(int width, int height) {
  if (width == copy_width_ && height == copy_height_ && copy_texture_ != nullptr) {
    return true;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  copy_srv_y_.Reset();
  copy_srv_uv_.Reset();
  copy_texture_.Reset();
  HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &copy_texture_);
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: NV12 copy texture {}x{} failed: 0x{:08X}", width, height, static_cast<unsigned>(hr));
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC y_desc = {};
  y_desc.Format = DXGI_FORMAT_R8_UNORM;
  y_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  y_desc.Texture2D.MostDetailedMip = 0;
  y_desc.Texture2D.MipLevels = 1;
  hr = device_->CreateShaderResourceView(copy_texture_.Get(), &y_desc, &copy_srv_y_);

  D3D11_SHADER_RESOURCE_VIEW_DESC uv_desc = {};
  uv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
  uv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  uv_desc.Texture2D.MostDetailedMip = 0;
  uv_desc.Texture2D.MipLevels = 1;
  if (SUCCEEDED(hr)) {
    hr = device_->CreateShaderResourceView(copy_texture_.Get(), &uv_desc, &copy_srv_uv_);
  }
  if (FAILED(hr)) {
    spdlog::error("VideoTexture: copy texture SRV failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }
  copy_width_ = width;
  copy_height_ = height;
  return true;
}

bool VideoTexture::ConvertSoftwareFrame(const AVFrame* frame) {
  if (!EnsureSwTexture(frame->width, frame->height)) {
    return false;
  }

  const AVPixelFormat src_fmt = static_cast<AVPixelFormat>(frame->format);
  sws_ = SwsContextPtr(sws_getCachedContext(sws_.release(), frame->width, frame->height, src_fmt, frame->width,
                                            frame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr));
  if (sws_ == nullptr) {
    spdlog::error("VideoTexture: sws_getCachedContext failed");
    return false;
  }

  const int stride = frame->width * 4;
  sw_buffer_.resize(static_cast<size_t>(stride) * frame->height);
  uint8_t* dst[4] = {sw_buffer_.data(), nullptr, nullptr, nullptr};
  int dst_strides[4] = {stride, 0, 0, 0};
  sws_scale(sws_.get(), frame->data, frame->linesize, 0, frame->height, dst, dst_strides);

  context_->UpdateSubresource(sw_texture_.Get(), 0, nullptr, sw_buffer_.data(), stride, 0);
  output_srv_ = sw_srv_;
  return true;
}

void VideoTexture::Update(const VideoFramePtr& frame) {
  if (frame == nullptr || frame->av_frame() == nullptr) {
    return;
  }
  const AVFrame* f = frame->av_frame();
  const bool ok = frame->IsHardware() ? ConvertHardwareFrame(f) : ConvertSoftwareFrame(f);
  if (ok) {
    last_frame_hardware_ = frame->IsHardware();
  }
}

}  // namespace vvp
