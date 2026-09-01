#include "app/app.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_glfw.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "ui/ui.h"

namespace vvp {

Application::~Application() { Shutdown(); }

bool Application::Init() {
  if (!window_.Init({})) {
    return false;
  }
  if (!renderer_.Init(window_.Handle())) {
    return false;
  }

  GLFWwindow* win = window_.Handle();
  glfwSetWindowUserPointer(win, this);
  glfwSetFramebufferSizeCallback(win, [](GLFWwindow* w, int width, int height) {
    auto* self = static_cast<Application*>(glfwGetWindowUserPointer(w));
    self->renderer_.OnResize(width, height);
  });

  if (!InitImGui()) {
    return false;
  }
  ui_ = Ui();
  return true;
}

bool Application::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Panel docking (flexible layout).
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // OS windows on other monitors.

  // When viewports are enabled, tweak style so platform windows look consistent.
  ImGuiStyle& style = ImGui::GetStyle();
  if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  ImGui_ImplGlfw_InitForOther(window_.Handle(), true);
  ImGui_ImplDX11_Init(renderer_.Device(), renderer_.Context());
  imgui_initialized_ = true;
  spdlog::info("ImGui initialized (docking + multi-viewport enabled)");
  return true;
}

void Application::Run() {
  while (!window_.ShouldClose()) {
    Window::PollEvents();
    if (glfwGetWindowAttrib(window_.Handle(), GLFW_ICONIFIED) != 0) {
      continue;  // Skip rendering while minimized; present() is guarded as well.
    }
    DrawFrame();
  }
}

void Application::DrawFrame() {
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ui_.Draw();

  ImGui::Render();
  renderer_.BeginFrame();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  // Update and render platform windows (viewports on other monitors).
  ImGuiIO& io = ImGui::GetIO();
  if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }

  renderer_.Present();
}

void Application::Shutdown() {
  if (imgui_initialized_) {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imgui_initialized_ = false;
  }
  renderer_.Shutdown();
  window_.Shutdown();
}

}  // namespace vvp
