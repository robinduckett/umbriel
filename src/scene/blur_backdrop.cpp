#include "scene/blur_backdrop.h"

// clang-format off
#include <cmath> // IWYU pragma: keep
#include "wlr.h"
// clang-format on

#include <vector>

namespace umbriel {

  namespace {

    // Areas (layout coordinates) of enabled blur nodes that sample the shared capture.
    void collectSharedBlurBoxes(wlr_scene_node* node, std::vector<wlr_box>& boxes) {
      if (!node->enabled) {
        return;
      }
      if (node->type == WLR_SCENE_NODE_TREE) {
        wlr_scene_tree* tree = wlr_scene_tree_from_node(node);
        wlr_scene_node* child;
        wl_list_for_each(child, &tree->children, link) { collectSharedBlurBoxes(child, boxes); }
        return;
      }
      if (node->type != WLR_SCENE_NODE_BLUR) {
        return;
      }
      wlr_scene_blur* blur = wlr_scene_blur_from_node(node);
      if (!blur->should_only_blur_bottom_layer || blur->width <= 0 || blur->height <= 0) {
        return;
      }
      int lx = 0;
      int ly = 0;
      if (!wlr_scene_node_coords(node, &lx, &ly)) {
        return;
      }
      if (wlr_box_empty(&blur->sample_hint)) {
        boxes.push_back({lx, ly, blur->width, blur->height});
      } else {
        boxes.push_back(
            {lx + blur->sample_hint.x, ly + blur->sample_hint.y, blur->sample_hint.width, blur->sample_hint.height}
        );
      }
    }

    wlr_box toBufferBox(const wlr_box& box, const wlr_scene_output* sceneOutput, float scale) {
      const int x0 = static_cast<int>(std::floor((box.x - sceneOutput->x) * scale));
      const int y0 = static_cast<int>(std::floor((box.y - sceneOutput->y) * scale));
      const int x1 = static_cast<int>(std::ceil((box.x + box.width - sceneOutput->x) * scale));
      const int y1 = static_cast<int>(std::ceil((box.y + box.height - sceneOutput->y) * scale));
      return {x0, y0, x1 - x0, y1 - y0};
    }

  } // namespace

  void BlurBackdrop::update(
      wlr_scene_optimized_blur* node, wlr_scene_output* sceneOutput, std::span<wlr_scene_tree* const> trees
  ) {
    std::vector<wlr_box> surfaces;
    for (wlr_scene_tree* tree : trees) {
      if (tree != nullptr) {
        collectSharedBlurBoxes(&tree->node, surfaces);
      }
    }
    const float scale = sceneOutput->output->scale;
    for (wlr_box& box : surfaces) {
      box = toBufferBox(box, sceneOutput, scale);
    }

    PixmanRegion damage;
    wlr_scene_output_get_pending_damage(sceneOutput, damage.get());
    BlurBackdropInput input{
        .surfaces = surfaces,
        .damage = damage.get(),
        .previousArea = m_previousArea.get(),
        .previousPadded = m_previousPadded,
        .captureAll = m_captureAll,
        .reach = wlr_scene_blur_reach(sceneOutput->scene),
    };
    wlr_output_transformed_resolution(sceneOutput->output, &input.width, &input.height);

    const BlurBackdropPlan plan = planBlurBackdrop(input);
    if (pixman_region32_not_empty(plan.write.get())) {
      wlr_scene_optimized_blur_capture(
          node, sceneOutput, plan.capture.get(), plan.write.get(), plan.clamp ? &*plan.clamp : nullptr
      );
    }
    pixman_region32_copy(m_previousArea.get(), plan.area.get());
    m_previousPadded = plan.padded;
    m_captureAll = false;
  }

} // namespace umbriel
