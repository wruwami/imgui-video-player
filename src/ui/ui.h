#pragma once

#include <functional>
#include <string>
#include <vector>

#include "media/video_source.h"

namespace vvp {

class Channel;
class VideoTexture;

// Draws the main UI: central dockspace + panels (player, monitors).
// Rendered inside the application's ImGui frame. source_options_factory
// supplies per-open options (e.g. the shared renderer device).
class Ui {
 public:
  void Draw(Channel* channel, VideoTexture* texture, std::vector<std::string>* dropped_paths,
            const std::function<SourceOptions()>& source_options_factory);
};

}  // namespace vvp
