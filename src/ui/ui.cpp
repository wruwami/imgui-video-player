#include "ui/ui.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace vvp {
namespace {

void DrawVideosPanel() {
  if (!ImGui::Begin("Videos")) {
    ImGui::End();
    return;
  }
  ImGui::TextDisabled("No video sources yet.");
  ImGui::Separator();
  ImGui::BulletText("M1: file playback (D3D11VA zero-copy + NV12 shader)");
  ImGui::BulletText("M2: multi-channel grid layout");
  ImGui::BulletText("M3: RTSP sources");
  ImGui::End();
}

void DrawMonitorRow(GLFWmonitor* monitor) {
  const char* name = glfwGetMonitorName(monitor);
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  int pos_x = 0, pos_y = 0;
  glfwGetMonitorPos(monitor, &pos_x, &pos_y);
  int work_x = 0, work_y = 0, work_w = 0, work_h = 0;
  glfwGetMonitorWorkarea(monitor, &work_x, &work_y, &work_w, &work_h);
  float scale_x = 1.0f, scale_y = 1.0f;
  glfwGetMonitorContentScale(monitor, &scale_x, &scale_y);
  int phys_w = 0, phys_h = 0;
  glfwGetMonitorPhysicalSize(monitor, &phys_w, &phys_h);

  if (ImGui::TreeNode(monitor, "%s", name != nullptr ? name : "(unnamed)")) {
    ImGui::Text("Resolution: %d x %d @ %d Hz", mode->width, mode->height, mode->refreshRate);
    ImGui::Text("Position: (%d, %d)", pos_x, pos_y);
    ImGui::Text("Work area: %d x %d at (%d, %d)", work_w, work_h, work_x, work_y);
    ImGui::Text("Physical size: %d x %d mm", phys_w, phys_h);
    ImGui::Text("Content scale: %.2f x %.2f", scale_x, scale_y);
    ImGui::TreePop();
  }
}

void DrawMonitorsPanel() {
  if (!ImGui::Begin("Monitors")) {
    ImGui::End();
    return;
  }
  int count = 0;
  GLFWmonitor** monitors = glfwGetMonitors(&count);
  ImGui::Text("Connected monitors: %d", count);
  ImGui::Separator();
  for (int i = 0; i < count; ++i) {
    ImGui::PushID(i);
    DrawMonitorRow(monitors[i]);
    ImGui::PopID();
  }
  ImGui::End();
}

}  // namespace

void Ui::Draw() {
  // Central dockspace filling the main viewport; panels dock freely into it.
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

  DrawVideosPanel();
  DrawMonitorsPanel();
}

}  // namespace vvp
