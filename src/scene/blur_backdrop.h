#pragma once

#include "scene/blur_backdrop_regions.h"

struct wlr_scene_optimized_blur;
struct wlr_scene_output;
struct wlr_scene_tree;

namespace umbriel {

  // Keeps an output's shared blur backdrop current for the top-layer surfaces that share it (`blur_shared`). Like a
  // CABackdropLayer group, only the areas of those surfaces are captured, and each frame only the part whose blur can
  // have changed is re-rendered and re-blurred. The capture node belongs to the owner's scene tree; this class never
  // frees it.
  class BlurBackdrop {
  public:
    // Captures what changed beneath the visible sharing surfaces in `tree`. Runs before the frame's damage test, since
    // a capture adds damage.
    void update(wlr_scene_optimized_blur* node, wlr_scene_output* sceneOutput, wlr_scene_tree* tree);
    // The next update captures the whole area, e.g. after the capture node was recreated or resized.
    void invalidate() { m_captureAll = true; }

  private:
    PixmanRegion m_previousArea;
    bool m_previousPadded = false;
    bool m_captureAll = true;
  };

} // namespace umbriel
