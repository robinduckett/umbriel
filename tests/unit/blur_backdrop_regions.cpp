#include "scene/blur_backdrop_regions.h"

#include "check.h"

#include <format>
#include <string>
#include <vector>

using umbriel::BlurBackdropInput;
using umbriel::BlurBackdropPlan;
using umbriel::PixmanRegion;
using umbriel::planBlurBackdrop;

namespace {

  constexpr int kWidth = 1000;
  constexpr int kHeight = 600;
  constexpr int kReach = 10;
  constexpr wlr_box kBar{0, 0, 1000, 40};
  constexpr wlr_box kPanel{300, 40, 400, 300};

  std::string describeBox(int x, int y, int width, int height) {
    return std::format("{},{} {}x{}", x, y, width, height);
  }

  std::string extents(const PixmanRegion& region) {
    if (!pixman_region32_not_empty(region.get())) {
      return "empty";
    }
    const pixman_box32_t* e = pixman_region32_extents(region.get());
    return describeBox(e->x1, e->y1, e->x2 - e->x1, e->y2 - e->y1);
  }

  bool contains(const PixmanRegion& region, int x, int y) {
    return pixman_region32_contains_point(region.get(), x, y, nullptr) != 0;
  }

  long long pixels(const pixman_region32_t* region) {
    int count = 0;
    const pixman_box32_t* rects = pixman_region32_rectangles(region, &count);
    long long total = 0;
    for (int i = 0; i < count; ++i) {
      total += static_cast<long long>(rects[i].x2 - rects[i].x1) * (rects[i].y2 - rects[i].y1);
    }
    return total;
  }

  bool sameRegion(const pixman_region32_t* a, const pixman_region32_t* b) {
    PixmanRegion difference;
    pixman_region32_subtract(difference.get(), a, b);
    if (pixman_region32_not_empty(difference.get())) {
      return false;
    }
    pixman_region32_subtract(difference.get(), b, a);
    return !pixman_region32_not_empty(difference.get());
  }

  BlurBackdropPlan plan(
      const std::vector<wlr_box>& surfaces, const std::vector<wlr_box>& damageBoxes, const BlurBackdropPlan* previous,
      bool captureAll = false
  ) {
    PixmanRegion damage;
    for (const wlr_box& box : damageBoxes) {
      pixman_region32_union_rect(damage.get(), damage.get(), box.x, box.y, box.width, box.height);
    }
    const BlurBackdropInput input{
        .surfaces = surfaces,
        .damage = damage.get(),
        .previousArea = previous != nullptr ? previous->area.get() : nullptr,
        .previousPadded = previous != nullptr && previous->padded,
        .captureAll = captureAll,
        .reach = kReach,
        .width = kWidth,
        .height = kHeight,
    };
    return planBlurBackdrop(input);
  }

} // namespace

UMBRIEL_TEST(loneSurfaceHasNoMarginAndClampsToItself) {
  const auto first = plan({kBar}, {}, nullptr);
  CHECK(!first.padded);
  CHECK_EQ(extents(first.area), describeBox(0, 0, 1000, 40));
  CHECK(first.clamp.has_value());
  CHECK_EQ(describeBox(first.clamp->x, first.clamp->y, first.clamp->width, first.clamp->height), extents(first.area));
}

UMBRIEL_TEST(loneSurfaceIgnoresDamageBeneathIt) {
  const auto first = plan({kBar}, {}, nullptr);
  // A window redrawing right below the bar is outside its area, so nothing is captured.
  const auto next = plan({kBar}, {{0, 40, 1000, 100}}, &first);
  CHECK_EQ(extents(next.write), std::string("empty"));
  CHECK_EQ(extents(next.capture), std::string("empty"));
}

UMBRIEL_TEST(damageInsideASurfaceRecapturesOnlyItsNeighbourhood) {
  const auto first = plan({kBar}, {}, nullptr);
  const auto next = plan({kBar}, {{100, 10, 20, 5}}, &first);
  // Blurred pixels within the reach of the damage, limited to the bar.
  CHECK_EQ(extents(next.write), describeBox(90, 0, 40, 25));
  // Everything those pixels sample, still limited to the (unpadded) bar.
  CHECK_EQ(extents(next.capture), describeBox(80, 0, 60, 35));
}

UMBRIEL_TEST(neighboursGetTheBlurReachAsMargin) {
  const auto first = plan({kBar, kPanel}, {}, nullptr);
  CHECK(first.padded);
  CHECK(!first.clamp.has_value());
  // The panel's margin below it is part of the area; the bar's margin above the screen is clipped away.
  CHECK(contains(first.area, 500, 340 + kReach - 1));
  CHECK(!contains(first.area, 500, 340 + kReach));
  // Damage in that margin changes blurred pixels near the panel's bottom edge only.
  const auto next = plan({kBar, kPanel}, {{300, 345, 50, 5}}, &first);
  CHECK_EQ(extents(next.write), describeBox(300, 335, 60, 5));
  CHECK_EQ(extents(next.capture), describeBox(290, 325, 80, 25));
}

UMBRIEL_TEST(gainingANeighbourCapturesEverything) {
  const auto bar = plan({kBar}, {}, nullptr);
  // The bar was captured unpadded and clamped; with a neighbour it must be re-captured with its margin.
  const auto both = plan({kBar, kPanel}, {}, &bar);
  PixmanRegion cores;
  pixman_region32_union_rect(cores.get(), cores.get(), kBar.x, kBar.y, kBar.width, kBar.height);
  pixman_region32_union_rect(cores.get(), cores.get(), kPanel.x, kPanel.y, kPanel.width, kPanel.height);
  CHECK(sameRegion(both.write.get(), cores.get()));
  CHECK(sameRegion(both.capture.get(), both.area.get()));
}

UMBRIEL_TEST(newSurfaceIsCapturedWholeAndOthersAreLeftAlone) {
  const auto before = plan({kBar, kPanel}, {}, nullptr);
  const wlr_box launcher{100, 450, 150, 100};
  const auto after = plan({kBar, kPanel, launcher}, {}, &before);
  CHECK_EQ(pixels(after.write.get()), static_cast<long long>(launcher.width) * launcher.height);
  CHECK(contains(after.write, launcher.x, launcher.y));
  CHECK(contains(after.write, launcher.x + launcher.width - 1, launcher.y + launcher.height - 1));
  CHECK(!contains(after.write, 500, 20));
}

UMBRIEL_TEST(nothingChangedCapturesNothing) {
  const auto first = plan({kBar, kPanel}, {}, nullptr);
  const auto next = plan({kBar, kPanel}, {}, &first);
  CHECK_EQ(extents(next.write), std::string("empty"));
  CHECK_EQ(extents(next.capture), std::string("empty"));
}

UMBRIEL_TEST(captureAllRecapturesEverySurface) {
  const auto first = plan({kBar, kPanel}, {}, nullptr);
  const auto again = plan({kBar, kPanel}, {}, &first, true);
  CHECK_EQ(pixels(again.write.get()), 1000LL * 40 + 400LL * 300);
}

UMBRIEL_TEST(surfacesAreClippedToTheOutput) {
  const auto clipped = plan({{-50, -10, 200, 40}}, {}, nullptr);
  CHECK_EQ(extents(clipped.area), describeBox(0, 0, 150, 30));
  CHECK(clipped.clamp.has_value());
  CHECK_EQ(clipped.clamp->width, 150);
  CHECK_EQ(clipped.clamp->height, 30);
}

int main() { return RUN_TESTS(); }
