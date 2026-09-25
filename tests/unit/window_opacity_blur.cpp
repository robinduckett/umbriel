#include "check.h"
#include "config/config.h"
#include "config/store.h"
#include "scene/surface_blur.h"
#include "view/decoration.h"

// clang-format off
#include "wlr.h"
// clang-format on

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>

namespace {

  wlr_scene_blur* onlyBlurChild(wlr_scene_tree& tree) {
    CHECK(!wl_list_empty(&tree.children));
    if (wl_list_empty(&tree.children)) {
      return nullptr;
    }
    wlr_scene_node* child;
    child = wl_container_of(tree.children.next, child, link);
    CHECK_EQ(child->type, WLR_SCENE_NODE_BLUR);
    if (child->type != WLR_SCENE_NODE_BLUR) {
      return nullptr;
    }
    return wlr_scene_blur_from_node(child);
  }

} // namespace

// Window-rule opacity belongs to the client surface composition. The blur
// behind it stays fully sampled, matching a client that submits the same alpha.
UMBRIEL_TEST(windowRuleOpacityDoesNotAttenuateBlur) {
  wlr_scene* scene = wlr_scene_create();
  CHECK(scene != nullptr);
  if (scene == nullptr) {
    return;
  }

  umbriel::ResolvedWindowRule rule;
  rule.blur = true;
  umbriel::ViewDecoration decoration;
  decoration.applyRule(rule);

  const wlr_box box{0, 0, 100, 100};
  decoration.updateBlur(&scene->tree, nullptr, box, box, 0, nullptr, 0.8F, 1.0F);

  wlr_scene_blur* blur = onlyBlurChild(scene->tree);
  CHECK(blur != nullptr);
  if (blur != nullptr) {
    CHECK_EQ(blur->alpha, 1.0F);
  }

  wlr_scene_node_destroy(&scene->tree.node);
}

// Transition opacity still fades the effect itself, otherwise an invisible
// opening or closing window would leave a standalone blur rectangle.
UMBRIEL_TEST(transitionOpacityStillAttenuatesBlur) {
  wlr_scene* scene = wlr_scene_create();
  CHECK(scene != nullptr);
  if (scene == nullptr) {
    return;
  }

  umbriel::ResolvedWindowRule rule;
  rule.blur = true;
  umbriel::ViewDecoration decoration;
  decoration.applyRule(rule);

  const wlr_box box{0, 0, 100, 100};
  decoration.updateBlur(&scene->tree, nullptr, box, box, 0, nullptr, 0.4F, 0.5F);

  wlr_scene_blur* blur = onlyBlurChild(scene->tree);
  CHECK(blur != nullptr);
  if (blur != nullptr) {
    CHECK_EQ(blur->alpha, 0.5F);
  }

  wlr_scene_node_destroy(&scene->tree.node);
}

// A shared blur captured above the windows only stays current under shell surfaces, and it contains the windows
// themselves, so a window's optimized blur must fall back to the live backdrop while layer surfaces keep using it.
UMBRIEL_TEST(capturingWindowsKeepsTheSharedBlurForShellSurfaces) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / ("umbriel-window-opacity-blur-" + std::to_string(getpid()) + ".toml");
  const auto load = [&](const std::string& contents) {
    std::ofstream(path) << contents;
    umbriel::configStore().setRootPath(path, true);
    CHECK(umbriel::configStore().reload().success);
  };
  load("[appearance.blur]\noptimized = true\ncapture_source = \"windows\"\n");

  wlr_scene* scene = wlr_scene_create();
  CHECK(scene != nullptr);
  if (scene != nullptr) {
    const wlr_box box{0, 0, 100, 100};

    umbriel::ResolvedWindowRule rule;
    rule.blur = true;
    umbriel::ViewDecoration decoration;
    decoration.applyRule(rule);
    wlr_scene_tree* windowTree = wlr_scene_tree_create(&scene->tree);
    decoration.updateBlur(windowTree, nullptr, box, box, 0, nullptr, 0.8F, 1.0F);
    if (wlr_scene_blur* blur = onlyBlurChild(*windowTree)) {
      CHECK(!blur->should_only_blur_bottom_layer);
    }

    umbriel::SurfaceBlur shell;
    wlr_scene_tree* shellTree = wlr_scene_tree_create(&scene->tree);
    shell.update(
        shellTree, nullptr, box, box, 0, nullptr,
        umbriel::SurfaceBlurOptions{
            .ignoreAlpha = 0.0F, .enabled = true, .optimized = std::nullopt, .shellSurface = true
        },
        0.8F
    );
    if (wlr_scene_blur* blur = onlyBlurChild(*shellTree)) {
      CHECK(blur->should_only_blur_bottom_layer);
    }

    wlr_scene_node_destroy(&scene->tree.node);
  }

  load("");
  std::filesystem::remove(path);
}

int main() { return RUN_TESTS(); }
