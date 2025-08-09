# Pico Game Controller (IIDX-oriented fork)

Firmware for Raspberry Pi Pico (RP2040) that targets rhythm-game controllers. This fork is tuned for IIDX-style builds: 11 buttons, 11 per-button LEDs, 1 WS2812B RGB strip (40 LEDs), and 1 encoder (turntable).

![Pocket SDVX Pico](demo.gif)

Key features

- USB HID composite: joystick (default), NKRO keyboard + mouse.
- 1000 Hz polling, debounce algorithms, reversible encoder.
- HID-controlled button LEDs with reactive fallback when host stops sending.
- WS2812B RGB runs on core 1 with PIO; effects use a palette system.
- Two HID RGB “zones” for host-driven color accents.
- Host configuration channel (HID Feature report) to switch RGB effect on the fly; simple Tkinter GUI included.
  - Also supports global brightness control (0–255).

Boot-time controls (hold while powering/resetting)

- Hold SW_GPIO[0] (GPIO 2): start in NKRO keyboard + mouse mode.
- Hold SW_GPIO[1] (GPIO 4): force Turbocharger LED effect.
- Hold SW_GPIO[8] (GPIO 19): disable RGB (don’t launch core 1).

LED effects

- Default: Color Cycle. You can change the active effect from the PC via the GUI tool.
- Built-in effects include: Turbocharger, Trail, Color Cycle, Dual Orbit, Velocity Comet, Button Ripples, Spokes, Counter Stripes, Palette Tint Gradient, Multipoint Snap, Center Pulse, Sector Equalizer, Radar Sweep.
- HID lights: if no OUT report arrives for 1s, the firmware falls back to button‑reactive LEDs.

Configuration

- All sizes, pins, and constants are in `src/controller_config.h`.
  - Buttons (SW_GPIO), per-button LEDs (LED_GPIO), WS2812B pin, and counts.
  - Encoder PPR and direction (flip `ENC_REV[0]` if your knob is reversed).
  - Debounce timing and reactive timeout.
- Debounce algorithms live in `src/debounce/` (eager and deferred).
- RGB effects live in `src/rgb/`; new effects just write into the global `leds[]` and are rendered by core 1.

Build and flash (VS Code tasks)

- Compile Project: builds to `build/` using Ninja.
- Run Project: flashes the current UF2 to a Pico in BOOTSEL via picotool.
- Clean Build and Flash (Picotool): removes old artifacts, builds, and flashes.
- Artifacts: UF2 at `build/src/Pico_Game_Controller.uf2` and copied to `build_uf2/`.
- PowerShell helper: `flash.ps1` can build, reboot to BOOTSEL, detect `RPI-RP2`, and copy the UF2.

Notes

- The default RGB effect is Color Cycle. To choose a fixed effect in firmware, set `ws2812b_mode` (or use `set_effect_by_id()`) in `init()` (see `src/pico_game_controller.c`).
- Two HID RGB color zones are available to effects via `hid_rgb[]`.

PC configuration tool (Tkinter)

- A small GUI to switch the RGB effect at runtime is provided at `tools/effect_selector.py`.
- Requirements (Windows): Python 3 and `hidapi`.

Try it

```powershell
pip install -r tools/requirements.txt
python tools/effect_selector.py
```

HID config channel details (for integrators)

- Report IDs (`src/usb_descriptors.h`): 1=Joystick, 2=Lights, 3=NKRO, 4=Mouse, 5=Config.
- Config uses a vendor page Feature report (8 bytes payload): `[cmd, arg0..arg6]`.
  - GET_FEATURE (Report ID 5) returns 8 bytes: `[status=0x00, current_effect, 0..]`.
  - GET_FEATURE layout: `[status=0x00, effect_id, brightness, 0..]`.
  - SET_EFFECT: host sends Feature report (Report ID 5) with `[0x01, effect_id]`.
  - SET_BRIGHTNESS: host sends Feature report (Report ID 5) with `[0x02, brightness]` (0–255).

Credits and license

- Original project by SpeedyPotato: https://github.com/speedypotato/Pico-Game-Controller/
- Additional contributions and ideas credited throughout the codebase (Turbocharger by 4yn; debounce variant by SushiRemover; encoder tips by KyubiFox; plus many others referenced in source comments).
- This repository retains the original license; see `LICENSE` for details.
