#include "scene/blur_backdrop_regions.h"

// clang-format off
#include <cmath> // IWYU pragma: keep
#include "wlr.h"
// clang-format on

namespace umbriel {

  BlurBackdropPlan planBlurBackdrop(const BlurBackdropInput& input) {
    BlurBackdropPlan plan;

    PixmanRegion cores;
    for (const wlr_box& box : input.surfaces) {
      pixman_region32_union_rect(cores.get(), cores.get(), box.x, box.y, box.width, box.height);
    }
    pixman_region32_intersect_rect(cores.get(), cores.get(), 0, 0, input.width, input.height);

    plan.padded = input.surfaces.size() >= 2;
    wlr_region_expand(plan.area.get(), cores.get(), plan.padded ? input.reach : 0);
    pixman_region32_intersect_rect(plan.area.get(), plan.area.get(), 0, 0, input.width, input.height);
    if (input.surfaces.size() == 1 && pixman_region32_not_empty(cores.get())) {
      const pixman_box32_t* e = pixman_region32_extents(cores.get());
      plan.clamp = wlr_box{e->x1, e->y1, e->x2 - e->x1, e->y2 - e->y1};
    }

    PixmanRegion changed;
    if (input.captureAll || input.previousArea == nullptr || plan.padded != input.previousPadded) {
      pixman_region32_copy(changed.get(), plan.area.get());
    } else {
      pixman_region32_subtract(changed.get(), plan.area.get(), input.previousArea);
    }
    if (input.damage != nullptr) {
      pixman_region32_union(changed.get(), changed.get(), input.damage);
    }
    pixman_region32_intersect(changed.get(), changed.get(), plan.area.get());

    wlr_region_expand(plan.write.get(), changed.get(), input.reach);
    pixman_region32_intersect(plan.write.get(), plan.write.get(), cores.get());
    if (pixman_region32_not_empty(plan.write.get())) {
      wlr_region_expand(plan.capture.get(), plan.write.get(), input.reach);
      pixman_region32_intersect(plan.capture.get(), plan.capture.get(), plan.area.get());
    }
    return plan;
  }

} // namespace umbriel
