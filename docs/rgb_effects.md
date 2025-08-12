# Pico Game Controller — RGB Effects Guide

A concise description of each available RGB effect, how it looks, and which inputs it reacts to.

Note: Many effects use two HID-provided colors from the host (hid_rgb[0] and hid_rgb[1]) plus a base palette (Ocean, Fire, Plasma, etc.). When you see “hid_mode dims,” it means output is slightly reduced in active HID mode to avoid visual saturation.

## Effect ID index (Config Tool)

Use these numeric IDs in the HID Config Tool to select an effect:

- 0 — Color Cycle (EFFECT_COLOR_CYCLE)
- 1 — Turbocharger (EFFECT_TURBOCHARGER)
- 2 — Trail (EFFECT_TRAIL)
- 3 — Dual Orbit (EFFECT_DUAL_ORBIT)
- 4 — Velocity Comet (EFFECT_VELOCITY_COMET)
- 5 — Button Ripples (EFFECT_BUTTON_RIPPLES)
- 6 — Spokes (EFFECT_SPOKES)
- 7 — Counter Stripes (EFFECT_COUNTER_STRIPES)
- 8 — Palette Tint Gradient (EFFECT_PALETTE_TINT_GRADIENT)
- 9 — Multipoint Snap (EFFECT_MULTIPOINT_SNAP)
- 10 — Center Pulse (EFFECT_CENTER_PULSE)
- 11 — Sector Equalizer (EFFECT_SECTOR_EQUALIZER)
- 12 — Radar Sweep (EFFECT_RADAR_SWEEP)

Note: “Demo All” is a showcase mode inside the firmware and isn’t directly selectable by ID.

## Color Cycle (EFFECT_COLOR_CYCLE)

- Visual: continuous rainbow around the ring.
- Inputs: none (not encoder or button dependent).
- Colors: active palette (default Plasma) + 0–767 wheel.
- Notes: great ambient/base effect.

## Turbocharger (EFFECT_TURBOCHARGER)

- Visual: two chasing “laser” zones orbit around and react to the two encoders (left/right).
- Inputs: encoders; motion starts/stops and decays smoothly; constant speed while moving; fades after inactivity.
- Colors: mix of punchy tones (e.g., cyan/green for left, magenta/orange for right).
- Notes: dynamic feedback for knobs. Can be forced at boot by holding its designated button (see README).

## Trail (EFFECT_TRAIL)

- Visual: 5 bright points with fading trails that follow motion.
- Inputs: encoder 0; after ~3 s of no movement it enters a slow “autoplay.”
- Colors: Neon palette with animated wheel; brightness from trail (255 at head, ~180 for neighbors).
- Notes: smooth and fluid; faster decay when active.

## Dual Orbit (EFFECT_DUAL_ORBIT)

- Visual: two heads 180° apart orbit the ring; subtle palette background glow.
- Inputs: encoder 0 sets direction and phase; drifts over time.
- Colors: heads tinted by hid_rgb[0] and hid_rgb[1]; background by Neon palette; hid_mode dims.
- Notes: clear contrast between left/right halves.

## Velocity Comet (EFFECT_VELOCITY_COMET)

- Visual: a comet with tail; sparks appear on deceleration.
- Inputs: encoder 0; smoothed velocity controls advance; sparks when velocity drops.
- Colors: Plasma palette over the trail, scaled by brightness.
- Notes: very expressive for rhythm/tempo changes.

## Button Ripples (EFFECT_BUTTON_RIPPLES)

- Visual: circular ripples that expand from the pressed button’s angle.
- Inputs: buttons; each new press spawns a finite-life ripple.
- Colors: dim Ocean palette base; ripple color from its zone’s HID color (left/right); hid_mode may dim.
- Notes: great feedback for button activity.

## Spokes (EFFECT_SPOKES)

- Visual: rotating spokes with a gentle strobe.
- Inputs: encoder 0 modulates speed; when any button is down, doubles the number of spokes.
- Colors: alternates hid_rgb[0] and hid_rgb[1]; hid_mode dims.
- Notes: mechanical/wheel-like feel.

## Counter Stripes (EFFECT_COUNTER_STRIPES)

- Visual: counter-rotating stripes, one direction per half of the ring.
- Inputs: none (time-based animation).
- Colors: left half uses Fire palette, right half Ocean; both halves tinted by their respective HID color.
- Notes: strong zone separation visually.

## Palette Tint Gradient (EFFECT_PALETTE_TINT_GRADIENT)

- Visual: palette gradient along the ring with a subtle HID tint that increases with the number of active buttons.
- Inputs: count of pressed buttons scales tint.
- Colors: Sunset palette + hid_rgb[0] tint; hid_mode dims.
- Notes: clean and subtle; shows activity without being intrusive.

## Multipoint Snap (EFFECT_MULTIPOINT_SNAP)

- Visual: 4 points chase target positions and “snap” to button angles on press.
- Inputs: encoder 0 shifts the base; button presses reposition points and shift hue.
- Colors: Viridis palette background; lobes colored alternating hid_rgb[0]/[1]; hid_mode dims.
- Notes: combines knob and button reactivity.

## Center Pulse (EFFECT_CENTER_PULSE)

- Visual: pulses that originate near two opposite centers and expand outward.
- Inputs: buttons nearest a center trigger/retrigger that center’s pulse.
- Colors: Arctic palette base; pulses in hid_rgb[0]/[1]; hid_mode dims.
- Notes: emphasizes the controller’s two halves.

## Sector Equalizer (EFFECT_SECTOR_EQUALIZER)

- Visual: ring divided into per-button sectors; each sector “rises” when its button is active.
- Inputs: buttons, one sector per button.
- Colors: dim Earth palette base; active sector adds its zone’s HID color.
- Notes: like a per-button VU meter.

## Radar Sweep (EFFECT_RADAR_SWEEP)

- Visual: radar-like sweep with decaying trail.
- Inputs: encoder 0 controls sweep speed.
- Colors: blend of a dim Rainbow palette with a tint from hid_rgb[0]; hid_mode dims.
- Notes: elegant continuous motion indicator.

---

### Extra: Demo All (not a direct selectable ID)

- Visual: cycles all effects with smooth crossfades (~8 s per effect + ~1.5 s crossfade).
- Purpose: showcase mode; not a replacement for individual effects.

## Quick reference

- Encoders: influence position/speed in Turbocharger, Trail, Dual Orbit, Velocity Comet, Spokes (speed), Radar Sweep, Multipoint Snap.
- Buttons: spawn waves/pulses in Button Ripples and Center Pulse; activate sectors in Sector Equalizer; change spokes in Spokes; alter hue/snap in Multipoint Snap; increase tint in Palette Tint Gradient.
- Palettes: Ocean/Fire/Plasma/Viridis/Sunset/Arctic/Earth/Neon/Rainbow as per effect.
