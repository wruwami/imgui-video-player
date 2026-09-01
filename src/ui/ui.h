#pragma once

namespace vvp {

// Draws the main UI: central dockspace + panels (channel grid placeholder,
// monitor overview). Rendered inside the application's ImGui frame.
class Ui {
 public:
  void Draw();
};

}  // namespace vvp
