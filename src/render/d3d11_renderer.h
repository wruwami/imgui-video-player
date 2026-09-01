#pragma once

#include <d3d11.h>
#include <wrl/client.h>

struct GLFWwindow;
struct HWND__;

namespace vvp {

// Minimal D3D11 device + swapchain for the main window (M0).
// The RHI abstraction (render layer) will grow around this in M1, including
// the NV12 -> RGBA video conversion pass for D3D11VA zero-copy playback.
class D3D11Renderer {
 public:
  D3D11Renderer() = default;
  ~D3D11Renderer();

  D3D11Renderer(const D3D11Renderer&) = delete;
  D3D11Renderer& operator=(const D3D11Renderer&) = delete;

  bool Init(GLFWwindow* window);
  void Shutdown();

  void OnResize(int width, int height);

  void BeginFrame();  // Bind + clear the main render target.
  void Present();     // Present the main swapchain (vsync).

  ID3D11Device* Device() const { return device_.Get(); }
  ID3D11DeviceContext* Context() const { return context_.Get(); }

 private:
  bool CreateDeviceAndSwapchain(HWND__* hwnd, int width, int height);
  void CreateRenderTarget();

  GLFWwindow* window_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> main_rtv_;
};

}  // namespace vvp
