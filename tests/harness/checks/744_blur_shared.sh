#!/usr/bin/env bash
# Top-layer surfaces with `blur_shared` blur one backdrop captured above the windows: they show what is beneath them,
# windows included, and a bar and a panel attached below it meet without a step. A bottom-layer square stands in for a
# window: it is below the top layer like the window stack.
set -euo pipefail

readonly LAYER="${UMBRIEL_LAYER_CLIENT:-./build-debug/tests/layer-client}"
readonly SCREENSHOT="$UMBRIEL_RUNTIME_DIR/blur-shared.png"
readonly BASE_CONFIG="$UMBRIEL_RUNTIME_DIR/blur-shared-base.toml"
cp "$UMBRIEL_CONFIG" "$BASE_CONFIG"

# Each phase rewrites the whole config: a TOML table may appear only once.
write_config() {
  local shared=$1
  cp "$BASE_CONFIG" "$UMBRIEL_CONFIG"
  cat >> "$UMBRIEL_CONFIG" <<EOF

[appearance.blur]
enabled = true
optimized = true
passes = 3
radius = 8
noise = 0.0
brightness = 1.0
contrast = 1.0
saturation = 1.0

[[layer_rule]]
match.namespace = "^blur-seam-"
blur = true
blur_shared = $shared
EOF
  "$UMBRIEL" msg config-reload > /dev/null
}

start_layer() {
  local name=$1
  shift
  local log="$UMBRIEL_RUNTIME_DIR/blur-shared-$name.log"
  "$LAYER" HEADLESS-1 "$@" > "$log" 2>&1 &
  for _ in $(seq 100); do
    grep -q '^ready$' "$log" && return 0
    sleep 0.02
  done
  echo "$name layer never presented: $(cat "$log")"
  exit 1
}

write_config true
start_layer background 0
start_layer window 0 bottom-layer
# Translucent dark surfaces (premultiplied 50% alpha), named so the layer rule blurs them.
start_layer bar 40 fill=80101010 namespace=blur-seam-bar
start_layer panel 0 panel=400x300 fill=80101010 namespace=blur-seam-panel

sample() { "$UMBRIEL_PIXEL_PROBE" "$SCREENSHOT" mean "$3x$4+$1+$2"; }
green() { read -r _ g _ <<< "$(sample "$@")"; echo "$g"; }
blue() { read -r _ _ b <<< "$(sample "$@")"; echo "$b"; }
abs() { local v=$1; echo $((v < 0 ? -v : v)); }
capture() {
  "$UMBRIEL" settle
  grim "$SCREENSHOT"
}

# The panel covers x 0-400, y 40-340; the square beneath it covers x 0-200, y 40-240.
capture
over_window=$(green 90 120 20 20)
over_background=$(green 300 280 20 20)
if ((over_window - over_background < 30)); then
  echo "the panel does not show the window beneath it: over window g=$over_window, over background g=$over_background"
  exit 1
fi

# Over plain background, a bar and panel blurring one backdrop meet without a step. Blurring each surface's own
# backdrop instead darkens the panel's first rows with the bar above it.
bar=$(blue 300 34 8 3)
panel_top=$(blue 300 40 8 3)
panel_middle=$(blue 300 150 8 8)
if (($(abs $((panel_top - panel_middle))) > 2 || $(abs $((panel_top - bar))) > 2)); then
  echo "the bar and panel do not meet seamlessly: bar b=$bar, panel top b=$panel_top, panel middle b=$panel_middle"
  exit 1
fi

# Turned off at runtime, the surfaces fall back to optimized blur, which captures only the background.
write_config false
capture
background_over_window=$(green 90 120 20 20)
if (($(abs $((background_over_window - over_background))) > 3)); then
  echo "blur_shared = false still shows the window: g=$background_over_window, background g=$over_background"
  exit 1
fi

echo "blur_shared blurs windows behind top-layer surfaces and joins a bar and panel seamlessly"
