#include "render/d3d11_renderer.h"

#include <spdlog/spdlog.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace vvp {
namespace {

constexpr DXGI_FORMAT kBackbufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
constexpr float kClearColor[] = {0.08f, 0.09f, 0.10f, 1.00f};

}  // namespace

D3D11Renderer::~D3D11Renderer() { Shutdown(); }

bool D3D11Renderer::Init(GLFWwindow* window) {
  window_ = window;
  HWND hwnd = glfwGetWin32Window(window);
  RECT rect = {};
  GetClientRect(hwnd, &rect);
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  return CreateDeviceAndSwapchain(hwnd, width, height);
}

bool D3D11Renderer::CreateDeviceAndSwapchain(HWND__* hwnd, int width, int height) {
  DXGI_SWAP_CHAIN_DESC desc = {};
  desc.BufferDesc.Width = width > 0 ? static_cast<UINT>(width) : 1;
  desc.BufferDesc.Height = height > 0 ? static_cast<UINT>(height) : 1;
  desc.BufferDesc.Format = kBackbufferFormat;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.OutputWindow = hwnd;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Flip model: required for low-latency video later.
  desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  constexpr D3D_FEATURE_LEVEL kFeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, kFeatureLevels,
                                             ARRAYSIZE(kFeatureLevels), D3D11_SDK_VERSION, &desc, &swapchain_, &device_,
                                             nullptr, &context_);
  if (FAILED(hr)) {
    // D3D_FEATURE_LEVEL_11_1 is not available on some older drivers; retry with 11_0 only.
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &kFeatureLevels[1], 1,
                                       D3D11_SDK_VERSION, &desc, &swapchain_, &device_, nullptr, &context_);
  }
  if (FAILED(hr)) {
    spdlog::critical("D3D11CreateDeviceAndSwapChain failed: 0x{:08X}", static_cast<unsigned>(hr));
    return false;
  }

  CreateRenderTarget();
  spdlog::info("D3D11 device + swapchain created ({}x{})", desc.BufferDesc.Width, desc.BufferDesc.Height);
  return true;
}

void D3D11Renderer::CreateRenderTarget() {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
  HRESULT hr = swapchain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
  if (FAILED(hr)) {
    spdlog::error("SwapChain::GetBuffer failed: 0x{:08X}", static_cast<unsigned>(hr));
    return;
  }
  main_rtv_ = nullptr;
  hr = device_->CreateRenderTargetView(backbuffer.Get(), nullptr, &main_rtv_);
  if (FAILED(hr)) {
    spdlog::error("CreateRenderTargetView failed: 0x{:08X}", static_cast<unsigned>(hr));
  }
}

void D3D11Renderer::OnResize(int width, int height) {
  if (swapchain_ == nullptr || width <= 0 || height <= 0) {
    return;
  }
  main_rtv_ = nullptr;
  HRESULT hr =
      swapchain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
  if (FAILED(hr)) {
    spdlog::error("ResizeBuffers({},{}) failed: 0x{:08X}", width, height, static_cast<unsigned>(hr));
    return;
  }
  CreateRenderTarget();
}

void D3D11Renderer::BeginFrame() {
  if (main_rtv_ == nullptr) {
    return;
  }
  ID3D11RenderTargetView* rtv = main_rtv_.Get();
  context_->OMSetRenderTargets(1, &rtv, nullptr);
  context_->ClearRenderTargetView(rtv, kClearColor);
}

void D3D11Renderer::Present() {
  if (swapchain_ == nullptr) {
    return;
  }
  if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0) {
    return;
  }
  swapchain_->Present(1, 0);  // vsync
}

void D3D11Renderer::Shutdown() {
  if (context_ != nullptr) {
    context_->ClearState();
  }
  main_rtv_.Reset();
  swapchain_.Reset();
  context_.Reset();
  device_.Reset();
}

}  // namespace vvp
