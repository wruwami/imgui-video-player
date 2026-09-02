#include "ui/video_view.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "media/channel.h"
#include "render/video_texture.h"

namespace vvp {
namespace {

const char* StateLabel(ChannelState state) {
  switch (state) {
    case ChannelState::kIdle:
      return "idle";
    case ChannelState::kOpening:
      return "opening...";
    case ChannelState::kPlaying:
      return "playing";
    case ChannelState::kPaused:
      return "paused";
    case ChannelState::kEnded:
      return "ended";
    case ChannelState::kError:
      return "error";
  }
  return "?";
}

}  // namespace

void DrawVideoView(VideoTexture* texture, float region_w, float region_h) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 region_max(origin.x + region_w, origin.y + region_h);

  // Black background over the whole region.
  draw_list->AddRectFilled(origin, region_max, IM_COL32(0, 0, 0, 255));

  if (texture == nullptr || !texture->valid() || region_w <= 0 || region_h <= 0) {
    return;
  }

  const float tex_w = static_cast<float>(texture->width());
  const float tex_h = static_cast<float>(texture->height());
  if (tex_w <= 0.0f || tex_h <= 0.0f) {
    return;
  }

  // Fit (letterbox).
  const float scale = std::min(region_w / tex_w, region_h / tex_h);
  const float draw_w = tex_w * scale;
  const float draw_h = tex_h * scale;
  const float off_x = origin.x + (region_w - draw_w) * 0.5f;
  const float off_y = origin.y + (region_h - draw_h) * 0.5f;

  ImGui::SetCursorScreenPos(ImVec2(off_x, off_y));
  ImGui::Image(texture->handle(), ImVec2(draw_w, draw_h));
  ImGui::SetCursorScreenPos(region_max);  // Restore the item flow to the region end.
}

void DrawVideoHud(Channel* channel) {
  if (channel == nullptr) {
    return;
  }
  const ChannelStats stats = channel->stats();
  const ChannelState state = channel->state();

  char line[256];
  if (stats.width > 0) {
    std::snprintf(line, sizeof(line), "%dx%d  %.1f fps  %s (%s)  %s", stats.width, stats.height, stats.decoded_fps,
                  stats.decoder.codec_name.c_str(), stats.decoder.hardware ? stats.decoder.hw_backend.c_str() : "sw",
                  StateLabel(state));
  } else {
    std::snprintf(line, sizeof(line), "%s", StateLabel(state));
  }

  ImVec2 pos = ImGui::GetCursorScreenPos();
  pos.x += 8.0f;
  pos.y += 6.0f;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 text_size = ImGui::CalcTextSize(line);
  draw_list->AddRectFilled(ImVec2(pos.x - 4.0f, pos.y - 2.0f),
                           ImVec2(pos.x + text_size.x + 4.0f, pos.y + text_size.y + 2.0f), IM_COL32(0, 0, 0, 160));
  draw_list->AddText(pos, IM_COL32(220, 220, 220, 255), line);
  if (state == ChannelState::kError) {
    draw_list->AddText(ImVec2(pos.x, pos.y + text_size.y + 4.0f), IM_COL32(255, 120, 120, 255), stats.error.c_str());
  }
}

void DrawPlayerControls(Channel* channel) {
  if (channel == nullptr) {
    return;
  }
  const ChannelState state = channel->state();
  const double duration = channel->duration();
  const bool is_file = duration > 0.0;

  if (state == ChannelState::kIdle || state == ChannelState::kOpening) {
    return;
  }

  // Play/pause.
  const char* btn = state == ChannelState::kPaused || state == ChannelState::kEnded ? "Play" : "Pause";
  if (ImGui::Button(btn)) {
    channel->TogglePlay();
    if (state == ChannelState::kEnded) {
      channel->Seek(0.0);
      channel->Play();
    }
  }

  // Seek bar (files only).
  if (is_file && ImGui::GetCurrentContext() != nullptr) {
    ImGui::SameLine();
    const double pos = channel->position();
    int pos_sec = static_cast<int>(pos);
    ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 170.0f));
    if (ImGui::SliderInt("##seek", &pos_sec, 0, static_cast<int>(duration), "")) {
      channel->Seek(static_cast<double>(pos_sec));
    }

    char time_label[64];
    std::snprintf(time_label, sizeof(time_label), "%d:%02d / %d:%02d", pos_sec / 60, pos_sec % 60,
                  static_cast<int>(duration) / 60, static_cast<int>(duration) % 60);
    ImGui::SameLine();
    ImGui::TextUnformatted(time_label);
  }

  // Speed (files only).
  if (is_file) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    static const float kSpeeds[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
    const float speed = static_cast<float>(channel->speed());
    char speed_label[16];
    std::snprintf(speed_label, sizeof(speed_label), "x%.2f", speed);
    if (ImGui::BeginCombo("##speed", speed_label)) {
      for (float s : kSpeeds) {
        char label[16];
        std::snprintf(label, sizeof(label), "x%.2f", s);
        if (ImGui::Selectable(label, speed == s)) {
          channel->SetSpeed(s);
        }
      }
      ImGui::EndCombo();
    }
  }
}

}  // namespace vvp
