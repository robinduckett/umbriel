#pragma once

#include "scene/blur_backdrop_regions.h"

#include <span>

struct wlr_scene_optimized_blur;
struct wlr_scene_output;
struct wlr_scene_tree;

namespace umbriel {

  // Keeps an output's shared blur backdrop current when it is captured above the windows
  // (`appearance.blur.capture_source = "windows"`). Like a CABackdropLayer group, only the areas of blurred shell
  // surfaces are captured, and each frame only the part whose blur can have changed is re-rendered and re-blurred. The
  // capture node belongs to the owner's scene tree; this class never frees it.
  class BlurBackdrop {
  public:
    // Captures what changed beneath the blurred surfaces found in `trees`. Runs before the frame's damage test, since
    // a capture adds damage.
    void update(wlr_scene_optimized_blur* node, wlr_scene_output* sceneOutput, std::span<wlr_scene_tree* const> trees);
    // The next update captures the whole area, e.g. after the capture node was recreated or resized.
    void invalidate() { m_captureAll = true; }

  private:
    PixmanRegion m_previousArea;
    bool m_previousPadded = false;
    bool m_captureAll = true;
  };

} // namespace umbriel
