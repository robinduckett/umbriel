# Layer Rules

Layer rules apply visual effects to layer-shell surfaces such as bars,
launchers, and notifications. Run `umbriel layers` to list active namespaces.

```toml
[[layer_rule]]
match.namespace = "^noctalia-bar-"
blur = true
blur_ignore_alpha = 0.5
blur_popups = true
```

## Matching

| Selector | Type | Description |
|----------|------|-------------|
| `match.namespace` | regex | Match the layer surface namespace. |

Regular expressions match any part of a namespace. Use `^` and `$` for an
exact match.

## Effects

| Key | Type | Description |
|-----|------|-------------|
| `blur` | bool | Enable/disable blur for the layer surface. |
| `blur_popups` | bool | Enable/disable blur for descendant XDG popups. |
| `blur_ignore_alpha` | float | Skip blur below an alpha threshold. |
| `blur_optimized` | bool | Override the global optimized-blur choice. |
| `blur_shared` | bool | Blur a backdrop shared with other top-layer surfaces, captured above the windows. |

Layer-shell blur is off by default. Every matching rule contributes its
settings, and later values take precedence.

With `blur_shared = true`, top-layer surfaces blur one backdrop that Umbriel
captures above the windows. They show the windows behind them and never blur
each other, so adjacent surfaces, such as a bar and a panel attached to it,
meet without a seam. `blur_shared` takes precedence over `blur_optimized`. It
has no effect on surfaces in other layers or on popups, which keep their usual
blur.

```toml
[[layer_rule]]
match.namespace = "^noctalia-(bar-[^\"]+|panel|attached-panel)$"
blur = true
blur_shared = true
```

Umbriel captures only the areas behind visible surfaces that share the
backdrop, and on each frame redraws only the parts whose blur can change. A
single surface, usually a bar, blurs only what is directly behind it, so windows
below it cause no capture work. The backdrop takes one output-sized buffer while
a layer rule sets `blur_shared`.

## Keyboard focus

A layer surface declares its own keyboard interactivity through the layer-shell
protocol; no rule overrides it.

| Interactivity | Behavior |
|---------------|----------|
| `none` | Never receives keyboard focus. Clicking the surface leaves the focused window alone. |
| `on_demand` | Takes focus when mapped; clicking a window or using a focus action moves focus away. |
| `exclusive` | Keeps keyboard focus; windows receive no keys and focus actions cannot leave it. |

Launchers and panels with search fields commonly use `on_demand`.
