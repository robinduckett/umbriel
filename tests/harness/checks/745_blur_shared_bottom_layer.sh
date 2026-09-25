#!/usr/bin/env bash
# `blur_shared` only applies to top-layer surfaces. The shared backdrop is captured at the bottom of the top layer, so a
# bottom-layer surface sampling it would blur a capture of itself and of everything above it. A bottom-layer surface
# with the rule must blur exactly as it does without it.
set -euo pipefail

readonly LAYER="${UMBRIEL_LAYER_CLIENT:-./build-debug/tests/layer-client}"
readonly SCREENSHOT="$UMBRIEL_RUNTIME_DIR/blur-shared-bottom.png"
readonly BASE_CONFIG="$UMBRIEL_RUNTIME_DIR/blur-shared-bottom-base.toml"
cp "$UMBRIEL_CONFIG" "$BASE_CONFIG"

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
match.namespace = "^blur-shared-"
blur = true
blur_shared = $shared
EOF
  "$UMBRIEL" msg config-reload > /dev/null
}

start_layer() {
  local name=$1
  shift
  local log="$UMBRIEL_RUNTIME_DIR/blur-shared-bottom-$name.log"
  "$LAYER" HEADLESS-1 "$@" > "$log" 2>&1 &
  for _ in $(seq 100); do
    grep -q '^ready$' "$log" && return 0
    sleep 0.02
  done
  echo "$name layer never presented: $(cat "$log")"
  exit 1
}

green() {
  "$UMBRIEL" settle
  grim "$SCREENSHOT"
  read -r _ g _ <<< "$("$UMBRIEL_PIXEL_PROBE" "$SCREENSHOT" mean 20x20+90+90)"
  echo "$g"
}

write_config false
start_layer background 0
# An opaque green square, then a translucent dark square over it on the same layer.
start_layer square 0 bottom-layer
start_layer translucent 0 bottom-layer fill=80101010 namespace=blur-shared-bottom
without_rule=$(green)

write_config true
with_rule=$(green)

if ((with_rule - without_rule > 3 || without_rule - with_rule > 3)); then
  echo "blur_shared changed a bottom-layer surface's blur: g=$with_rule with the rule, g=$without_rule without"
  exit 1
fi

echo "blur_shared leaves bottom-layer surfaces on their own blur"
