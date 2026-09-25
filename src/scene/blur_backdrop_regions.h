#pragma once

extern "C" {
#include <wlr/util/box.h>
}

#include <optional>
#include <pixman.h>
#include <span>

namespace umbriel {

  // Owns a pixman region for the lifetime of the object.
  class PixmanRegion {
  public:
    PixmanRegion() { pixman_region32_init(&m_region); }
    ~PixmanRegion() { pixman_region32_fini(&m_region); }
    PixmanRegion(const PixmanRegion&) = delete;
    PixmanRegion& operator=(const PixmanRegion&) = delete;
    // Moving takes over the region's storage and leaves the source empty.
    PixmanRegion(PixmanRegion&& other) noexcept : m_region(other.m_region) { pixman_region32_init(&other.m_region); }
    PixmanRegion& operator=(PixmanRegion&& other) noexcept {
      if (this != &other) {
        pixman_region32_fini(&m_region);
        m_region = other.m_region;
        pixman_region32_init(&other.m_region);
      }
      return *this;
    }

    [[nodiscard]] pixman_region32_t* get() { return &m_region; }
    [[nodiscard]] const pixman_region32_t* get() const { return &m_region; }

  private:
    pixman_region32_t m_region;
  };

  // Inputs for one frame of the shared blur backdrop, all in output buffer coordinates.
  struct BlurBackdropInput {
    std::span<const wlr_box> surfaces;               // Areas that show the shared blur, one per blurred surface.
    const pixman_region32_t* damage = nullptr;       // Damage pending for this frame.
    const pixman_region32_t* previousArea = nullptr; // `area` of the previous plan.
    bool previousPadded = false;                     // `padded` of the previous plan.
    bool captureAll = false; // Re-capture the whole area, e.g. after the capture node was recreated.
    int reach = 0;           // How far the blur samples beyond a pixel.
    int width = 0;
    int height = 0;
  };

  // What to re-capture this frame. `capture` is re-rendered beneath the capture node and blurred; only `write`,
  // whose blur samples all stay inside `capture`, is written back to the shared blur buffer.
  struct BlurBackdropPlan {
    PixmanRegion area; // Everything the shared blur must keep current.
    PixmanRegion write;
    PixmanRegion capture;
    std::optional<wlr_box> clamp; // Keep blur samples inside this box.
    bool padded = false;          // `area` includes the blur reach around each surface.
  };

  // A lone surface (usually a bar) blurs only what is directly behind it: no margin and samples clamped to itself, so
  // content beneath its edges never triggers a capture. With two or more, every surface gets the blur reach so
  // neighbours blur the same continuous backdrop and meet seamlessly. Newly covered area, and everything after a switch
  // between those two cases, is captured whole.
  [[nodiscard]] BlurBackdropPlan planBlurBackdrop(const BlurBackdropInput& input);

} // namespace umbriel
