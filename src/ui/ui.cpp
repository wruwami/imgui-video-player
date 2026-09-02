#include "ui/ui.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "media/channel.h"
#include "render/video_texture.h"
#include "ui/video_view.h"

namespace vvp {
namespace {

constexpr size_t kMaxRecentFiles = 8;

void DrawVideosPanel(Channel* channel, VideoTexture* texture, std::vector<std::string>* dropped_paths,
                     const std::function<SourceOptions()>& make_options) {
  if (!ImGui::Begin("Videos")) {
    ImGui::End();
    return;
  }

  // URL / path input + open/close.
  static char path_buf[1024] = "";
  static std::vector<std::string> recent;

  const bool busy = channel->state() != ChannelState::kIdle;
  ImGui::BeginDisabled(busy);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 150.0f);
  bool open_requested = ImGui::InputText("##url", path_buf, sizeof(path_buf), ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  open_requested = ImGui::Button("Open") || open_requested;
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    channel->Close();
  }
  ImGui::EndDisabled();

  if (open_requested && path_buf[0] != '\0') {
    channel->Open(path_buf, make_options());
    recent.erase(std::remove(recent.begin(), recent.end(), path_buf), recent.end());
    recent.insert(recent.begin(), path_buf);
    if (recent.size() > kMaxRecentFiles) {
      recent.resize(kMaxRecentFiles);
    }
  }

  // Consume drag-and-dropped paths when the player is free.
  if (dropped_paths != nullptr && !dropped_paths->empty() &&
      (channel->state() == ChannelState::kIdle || channel->state() == ChannelState::kError)) {
    const std::string path = dropped_paths->front();
    dropped_paths->clear();
    std::snprintf(path_buf, sizeof(path_buf), "%s", path.c_str());
    channel->Open(path, make_options());
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    recent.insert(recent.begin(), path);
    if (recent.size() > kMaxRecentFiles) {
      recent.resize(kMaxRecentFiles);
    }
  }

  if (!recent.empty()) {
    if (ImGui::BeginCombo("##recent", "Recent")) {
      for (const std::string& r : recent) {
        if (ImGui::Selectable(r.c_str())) {
          std::snprintf(path_buf, sizeof(path_buf), "%s", r.c_str());
          channel->Open(r, make_options());
        }
      }
      ImGui::EndCombo();
    }
  }

  // Video area fills the window above a bottom control row.
  const float controls_h = ImGui::GetFrameHeight() + 8.0f;
  const ImVec2 region_pos = ImGui::GetCursorScreenPos();
  const float region_w = ImGui::GetContentRegionAvail().x;
  const float region_h = std::max(64.0f, ImGui::GetContentRegionAvail().y - controls_h);

  DrawVideoView(texture, region_w, region_h);  // Fills the region, black bars included.

  // HUD overlay at the region's top-left.
  ImGui::SetCursorScreenPos(region_pos);
  DrawVideoHud(channel);

  // Controls row below the video area.
  ImGui::SetCursorScreenPos(ImVec2(region_pos.x, region_pos.y + region_h + 4.0f));
  DrawPlayerControls(channel);

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

void Ui::Draw(Channel* channel, VideoTexture* texture, std::vector<std::string>* dropped_paths,
              const std::function<SourceOptions()>& source_options_factory) {
  // Central dockspace filling the main viewport; panels dock freely into it.
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

  DrawVideosPanel(channel, texture, dropped_paths, source_options_factory);
  DrawMonitorsPanel();
}

}  // namespace vvp
