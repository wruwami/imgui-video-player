#pragma once

#include <d3d11.h>

namespace vvp {

class Channel;
class VideoTexture;

// Letterboxed video display: fills the available region, preserving the
// texture's aspect ratio and centering the result (black bars elsewhere).
void DrawVideoView(VideoTexture* texture, float region_w, float region_h);

// Small text overlay (top-left) with channel decode status.
void DrawVideoHud(Channel* channel);

// Playback controls row (play/pause, seek, speed).
void DrawPlayerControls(Channel* channel);

}  // namespace vvp
