# Pico Game Controller - AI Coding Instructions

## Architecture Overview

This is a Raspberry Pi Pico-based IIDX/SDVX game controller firmware using TinyUSB, dual-core processing, and PIO state machines. The controller supports 10 buttons, 10 LEDs, 1 encoder, and WS2812B RGB strips with multiple operating modes.

### Core Components

- **Main Controller** (`src/pico_game_controller.c`): Single-threaded USB HID device on core 0
- **RGB Lighting** (`src/rgb/`): Runs on core 1 with 5ms update cycle
- **PIO State Machines**: Hardware-accelerated encoder reading and WS2812B output
- **Modular Systems**: Pluggable debouncing algorithms and RGB effects

### Dual-Core Architecture

- **Core 0**: Main USB loop, input scanning, mode switching, LED control
- **Core 1**: WS2812B RGB lighting effects only (launched via `multicore_launch_core1()`)
- **Communication**: Shared global variables (`lights_report`, `enc_val`, timing variables)

### Operating Modes

The controller switches modes via button combinations held during boot:

- **Default**: Gamepad mode with HID joystick reports
- **Button 0 held**: NKRO Keyboard + Mouse mode (alternating reports)
- **Button 1 held**: Turbocharger RGB mode instead of color cycle
- **Button 8 held**: Disables RGB (core 1 not launched)

## Key Configuration Patterns

### Hardware Configuration (`controller_config.h`)

All pin assignments, timing constants, and array sizes are centralized:

```c
#define SW_GPIO_SIZE 10              // Drives array declarations
const uint8_t SW_GPIO[] = {2, 4, 6, 8, 10, 12, 14, 16, 19, 21};
const uint8_t LED_GPIO[] = {3, 5, 7, 9, 11, 13, 15, 17, 20, 18};
```

Staggered pin layout enables easier PCB routing. All timing constants use microseconds.

### Modular Extensions

#### Adding Debounce Algorithms (`src/debounce/`)

1. Create new `.c` file with function returning `uint16_t` button states
2. Add `#include` to `debounce_include.h`
3. Set `debounce_mode` function pointer in `init()`

#### Adding RGB Effects (`src/rgb/`)

1. Create function accepting `uint32_t counter` parameter
2. Use `put_pixel(urgb_u32(r, g, b))` for output
3. Add `#include` to `rgb_include.h`
4. Set `ws2812b_mode` function pointer

## Build System & Development Workflow

### CMake Structure

- Root `CMakeLists.txt`: Pico SDK integration and VS Code extension compatibility
- `src/CMakeLists.txt`: PIO header generation and TinyUSB linking
- Automatic UF2 copy to `build_uf2/` for easy flashing

### Essential Commands (VS Code Tasks)

- **Compile**: Use "Compile Project" task (calls ninja in build/)
- **Flash**: Use "Run Project" task (picotool load) or drag UF2 to mounted Pico
- **Debug**: "Flash" task for OpenOCD debugging

### PIO Programs

- `encoders.pio`: Quadrature encoder state machine with optional debouncing
- `ws2812.pio`: WS2812B bit-banging state machine
- Generated headers included automatically by CMake

## Critical Implementation Details

### USB HID Reports

- **Joystick**: 16-bit button mask + 2x8-bit joystick values
- **Lights**: Button LEDs (10 bytes) + RGB zones (6 bytes)
- **NKRO**: 32-byte keyboard report with modifier support
- Report IDs defined in `usb_descriptors.h`, descriptors in `usb_descriptors.c`

### Timing-Critical Code

- **Reactive lighting timeout**: 1 second fallback from HID to button-reactive LEDs
- **Debouncing**: 4ms default, timestamp-based per-button
- **Encoder rollover**: Careful modulo arithmetic to prevent uint32 overflow

### Memory Layout

```c
union {
  struct { uint8_t buttons[10]; RGB_t rgb[2]; } lights;
  uint8_t raw[16];
} lights_report;
```

HID light data directly overlays structured access for efficient USB handling.

## Testing & Validation

- Test button debouncing with rapid keypresses
- Verify encoder direction with `ENC_REV[]` array
- Check RGB zones match `WS2812B_LED_ZONES` configuration
- Validate USB descriptor compliance with game software (EAC compatibility tested)

## Hardware Dependencies

- TinyUSB device stack for USB HID
- Pico SDK hardware APIs (PIO, DMA, multicore)
- Pull-up switches (logic inverted in code)
- 800kHz WS2812B timing assumption
