# ZMK config for beekeeb Toucan Keyboard

[The beekeeb Toucan Keyboard](https://beekeeb.com/toucan-keyboard/) is a wireless split 42-key column-stagger keyboard with a display and a trackpad, with an aggressive stagger on the pinky columns.

## Changes from stock Toucan firmware

### Persistent speed step module (custom ZMK module)

Stock Toucan uses dedicated layers (`SLOW` / `FAST`) to switch trackpad speed, toggled via `&tog` keys. This requires 6 extra layers (SLOW, SLOW\_SCR, FAST, FAST\_SCR, extra\_2, extra\_3) and 4 conditional layer rules just for speed switching. Speed resets on power off.

This fork replaces that with a **custom ZMK input processor module** (`zmk,input-processor-speed-step`) that provides:

- **5-level step adjustment** for both mouse and scroll speed
- **Persistent storage** via Zephyr settings subsystem (NVS flash) -- survives power cycles
- **Dedicated behavior** (`zmk,behavior-speed-step`) for keymap binding

#### Speed levels

| Level | Mouse (`&mss`) | Scroll (`&sss`) |
|-------|----------------|-----------------|
| 0     | 1.25x          | 0.10x           |
| 1     | 1.75x          | **0.20x** (default) |
| 2     | **2.50x** (default) | 0.35x       |
| 3     | 3.75x          | 0.50x           |
| 4     | 5.00x          | 0.80x           |

Default levels match the stock firmware's normal speed (mouse 250/100, scroll 1/5).

#### Module structure

```
zmk-keyboard-toucan/
├── zephyr/module.yml      # Extended to include cmake, kconfig, dts_root
├── Kconfig                # CONFIG_ZMK_INPUT_PROCESSOR_SPEED_STEP
├── CMakeLists.txt         # Builds src/ when config enabled
├── dts/bindings/
│   ├── input_processors/zmk,input-processor-speed-step.yaml
│   └── behaviors/zmk,behavior-speed-step.yaml
├── include/
│   ├── dt-bindings/zmk/speed_step.h   # SPEED_STEP_UP / SPEED_STEP_DN
│   └── speed_step/speed_step.h        # C API (speed_step_change_level)
└── src/
    ├── input_processor_speed_step.c   # Scaler + settings persistence
    └── behavior_speed_step.c          # Keymap behavior
```

No external module dependency. No ZMK fork change required. Built entirely within the keyboard repo as a Zephyr module.

#### How it works

1. `input_processor_speed_step.c` implements a ZMK input processor that scales XY / scroll values by `multipliers[current_level] / divisor`, with remainder tracking for sub-pixel accuracy
2. `behavior_speed_step.c` provides `&mss` / `&sss` behaviors bound in the keymap. On key press, it calls `speed_step_change_level()` on the linked processor
3. On level change, `settings_save_one("spd/ms", ...)` writes to NVS flash immediately
4. On boot, `SETTINGS_STATIC_HANDLER_DEFINE` restores the saved level before any input events

### Keymap

5 layers total (down from 11 in the layer-based speed approach):

| # | Name  | Purpose |
|---|-------|---------|
| 0 | BASE  | QWERTY |
| 1 | NAV   | Numbers, arrows, Bluetooth (left thumb hold) |
| 2 | SYM   | Symbols (right thumb hold) |
| 3 | ADJ   | F-keys, volume, language, **speed step** (NAV+SYM) |
| 4 | MOUSE | Auto-activated on trackpad touch (2s timeout) |

#### ADJ layer speed controls (right bottom row)

| N | M | , | . |
|---|---|---|---|
| Mouse slower | Mouse faster | Scroll slower | Scroll faster |

#### MOUSE layer (auto-activated on trackpad touch)

| Left hand | S | D | F | Left thumb |
|-----------|---|---|---|------------|
|           | Right click | Middle click | Left click | Left click |

### Trackpad configuration

- `zip_temp_layer`: auto-activates MOUSE layer (4) on trackpad touch, 2 second timeout
- `excluded-positions`: mouse button keys (S, D, F, left thumb) don't deactivate the layer
- Scroller mode activates on NAV/SYM layers, with X-axis inversion

### Config (`toucan_left.conf`)

- `CONFIG_SETTINGS=y` -- enables NVS-based persistent settings for speed levels
- `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000` -- 60 min deep sleep (default 15 min)
- `CONFIG_ZMK_PM_SOFT_OFF=y` -- proper sleep notifications for Cirque trackpad driver

# License

The code in this repo is available under the MIT license.

The included shield nice_view_gem is modified from https://github.com/M165437/nice-view-gem licensed under the MIT License.

ZMK code snippets are taken from the ZMK documentation under the MIT license.

The embedded font QuinqueFive is designed by GGBotNet, licensed under under the SIL Open Font License, Version 1.1.
