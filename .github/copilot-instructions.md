# Pico Game Controller — AI Coding Instructions

Purpose: Make quick, correct edits to this Pico (RP2040) firmware by following project-specific patterns and workflows.

Architecture (what runs where)

- Core 0: USB HID device loop + input scan + mode/LED logic. See `src/pico_game_controller.c` (main, `joy_mode()`, `key_mode()`, `update_lights()`).
- Core 1: WS2812B rendering loop (`core1_entry()` runs every ~5ms). Only launched if RGB isn’t disabled at boot.
- PIO/DMA: `encoders.pio` feeds `enc_val[]` via DMA; `ws2812.pio` drives LEDs at 800kHz. Headers are auto-generated into `build/src/…` and referenced as `encoders.pio.h`, `ws2812.pio.h`.

Boot-time behavior (read GPIO via internal pull‑ups)

- Hold `SW_GPIO[0]` to start in NKRO keyboard/mouse mode; otherwise default to joystick gamepad.
- Hold `SW_GPIO[1]` to select Turbocharger LEDs; otherwise Trail effect.
- Hold `SW_GPIO[8]` to disable RGB (prevents launching core 1).
  Pins live in `src/controller_config.h` (e.g., `SW_GPIO[] = {2,4,6,8,10,12,14,16,19,21}`); update there only, and keep sizes consistent with `*_SIZE` defines.

HID and data flow

- Report IDs in `src/usb_descriptors.h`: 1=Joystick, 2=Lights, 3=NKRO, 4=Mouse. Descriptors size depends on `SW_GPIO_SIZE`, `LED_GPIO_SIZE`, `WS2812B_LED_ZONES`.
- Lights: OUT reports populate `lights_report.raw[...]`; if no HID lights for `REACTIVE_TIMEOUT_MAX` (1s), fall back to button‑reactive LEDs in `update_lights()`.
- Encoder to joystick: modulo wrap with `ENC_PULSE` (PPR×4), scaled to 0–255.

Project conventions (important when adding features)

- Debounce modes live in `src/debounce/`. Add a `uint16_t my_algo()` and include it in `debounce_include.h`; select via `debounce_mode = &my_algo;` in `init()`.
- RGB effects live in `src/rgb/`. Add `void my_effect(uint32_t counter, bool hid_mode)`; include in `rgb_include.h`; select via `ws2812b_mode = &my_effect;`. Write colors into the global `leds[]`; frame is emitted by `show()`.
- Switches use pull‑ups; pressed means `!gpio_get(SW_GPIO[i])`.

Build, flash, debug (Windows/VS Code Pico extension)

- Tasks: “Compile Project” (ninja build to `build/`), “Run Project” (picotool load), “Flash” (OpenOCD probe), plus rescue/reset tasks.
- Artifacts: UF2 at `build/src/Pico_Game_Controller.uf2` and auto‑copied to `build_uf2/Pico_Game_Controller.uf2`.
- PowerShell helper: `.lash.ps1 [-SkipBuild] [-Timeout <sec>]` builds (ninja), reboots to BOOTSEL (picotool), detects the `RPI-RP2` drive, and copies the UF2.

Files to know

- `src/pico_game_controller.c`: main loop, boot‑time mode selection, encoders DMA IRQ, lights fallback, core‑1 launcher.
- `src/controller_config.h`: single source of truth for sizes, pins, and constants (SW/LED/ENC/RGB).
- `src/usb_descriptors.h`: HID report layouts parameterized by config sizes.
- `src/rgb/*`: `ws2812b_util.c` (palette/color helpers), `color_cycle.c`, `turbocharger.c`, `trail.c` effects.
- `src/debounce/*`: `eager.c` (press‑immediate, hold for N µs), `deferred.c` (stable‑for‑N µs).

Gotchas and edge cases

- Keep arrays aligned to `*_SIZE` defines; descriptors and loops assume exact lengths.
- Single‑writer globals cross cores; follow existing patterns to avoid races (e.g., only core 1 renders, core 0 updates mode/state).
- If encoder direction is wrong, flip `ENC_REV[]` in `controller_config.h`.

Ask before changing

- USB descriptor structure/IDs, boot‑time button semantics, or task configuration—these impact compatibility (EAC/Konami spoofing and VS Code Pico tooling).
