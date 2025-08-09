/*
 * Pico Game Controller
 * @author SpeedyPotato
 */
#define PICO_GAME_CONTROLLER_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "controller_config.h"
#include "encoders.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "tusb.h"
#include "usb_descriptors.h"
// Flash persistence
#include "hardware/flash.h"
#include "hardware/sync.h"

// RGB type definition (must be before RGB includes)
#ifndef RGB_T_DEFINED
#define RGB_T_DEFINED
typedef struct
{
  uint8_t r, g, b;
} RGB_t;
#endif

// clang-format off

// ---- Persistent settings (flash) ----
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif
#define FLASH_SECTOR_SZ 4096
#define FLASH_PAGE_SZ 256
#define SETTINGS_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SZ) // last sector

typedef struct __attribute__((packed))
{
  uint32_t magic;      // 'CFG1'
  uint8_t version;     // 1
  uint8_t effect_id;   // 0..N
  uint8_t brightness;  // 0..255
  uint8_t reserved;    // padding
  uint32_t reserved2;  // future
} settings_t;

static const uint32_t SETTINGS_MAGIC = 0x31474643u; // 'CFG1' LE
static void load_settings(void);
static void save_settings(void);

#include "debounce/debounce_include.h"
#include "rgb/rgb_include.h"
// clang-format on

PIO pio, pio_1;
uint32_t enc_val[ENC_GPIO_SIZE];
uint32_t prev_enc_val[ENC_GPIO_SIZE];
int cur_enc_val[ENC_GPIO_SIZE];

bool prev_sw_val[SW_GPIO_SIZE];
uint64_t sw_timestamp[SW_GPIO_SIZE];

bool kbm_report;

uint64_t reactive_timeout_timestamp;

void (*ws2812b_mode)(uint32_t counter, bool hid_mode);
void (*loop_mode)();
uint16_t (*debounce_mode)();
bool joy_mode_check = true;
// Deferred actions from USB callbacks
static volatile bool g_request_bootsel = false;

// RGB effect selection
enum
{
  EFFECT_COLOR_CYCLE = 0,
  EFFECT_TURBOCHARGER = 1,
  EFFECT_TRAIL = 2,
  EFFECT_DUAL_ORBIT = 3,
  EFFECT_VELOCITY_COMET = 4,
  EFFECT_BUTTON_RIPPLES = 5,
  EFFECT_SPOKES = 6,
  EFFECT_COUNTER_STRIPES = 7,
  EFFECT_PALETTE_TINT_GRADIENT = 8,
  EFFECT_MULTIPOINT_SNAP = 9,
  EFFECT_CENTER_PULSE = 10,
  EFFECT_SECTOR_EQUALIZER = 11,
  EFFECT_RADAR_SWEEP = 12,
};

static uint8_t current_effect_id = EFFECT_COLOR_CYCLE;
static uint8_t g_brightness = 255; // 0..255 scaling for WS2812B output

static void set_effect_by_id(uint8_t id)
{
  // Update global pointer; core 1 will use it next frame
  switch (id)
  {
  default:
  case EFFECT_COLOR_CYCLE:
    ws2812b_mode = &ws2812b_color_cycle;
    current_effect_id = EFFECT_COLOR_CYCLE;
    break;
  case EFFECT_TURBOCHARGER:
    ws2812b_mode = &turbocharger_color_cycle;
    current_effect_id = EFFECT_TURBOCHARGER;
    break;
  case EFFECT_TRAIL:
    ws2812b_mode = &ws2812b_trail;
    current_effect_id = EFFECT_TRAIL;
    break;
  case EFFECT_DUAL_ORBIT:
    ws2812b_mode = &ws_dual_orbit;
    current_effect_id = EFFECT_DUAL_ORBIT;
    break;
  case EFFECT_VELOCITY_COMET:
    ws2812b_mode = &ws_velocity_comet;
    current_effect_id = EFFECT_VELOCITY_COMET;
    break;
  case EFFECT_BUTTON_RIPPLES:
    ws2812b_mode = &ws_button_ripples;
    current_effect_id = EFFECT_BUTTON_RIPPLES;
    break;
  case EFFECT_SPOKES:
    ws2812b_mode = &ws_spokes;
    current_effect_id = EFFECT_SPOKES;
    break;
  case EFFECT_COUNTER_STRIPES:
    ws2812b_mode = &ws_counter_stripes;
    current_effect_id = EFFECT_COUNTER_STRIPES;
    break;
  case EFFECT_PALETTE_TINT_GRADIENT:
    ws2812b_mode = &ws_palette_tint_gradient;
    current_effect_id = EFFECT_PALETTE_TINT_GRADIENT;
    break;
  case EFFECT_SECTOR_EQUALIZER:
    ws2812b_mode = &ws_sector_equalizer;
    current_effect_id = EFFECT_SECTOR_EQUALIZER;
    break;
  case EFFECT_RADAR_SWEEP:
    ws2812b_mode = &ws_radar_sweep;
    current_effect_id = EFFECT_RADAR_SWEEP;
    break;
  }
}

// ---- Persistent settings (flash) implementation (after effect state is defined) ----
static void load_settings(void)
{
  const uint8_t *flash_ptr = (const uint8_t *)(XIP_BASE + SETTINGS_FLASH_OFFSET);
  const settings_t *s = (const settings_t *)flash_ptr;
  if (s->magic == SETTINGS_MAGIC && s->version == 1)
  {
    if (s->effect_id <= EFFECT_RADAR_SWEEP)
    {
      current_effect_id = s->effect_id;
    }
    g_brightness = s->brightness;
  }
}

static void save_settings(void)
{
  settings_t s = {
      .magic = SETTINGS_MAGIC,
      .version = 1,
      .effect_id = current_effect_id,
      .brightness = g_brightness,
      .reserved = 0,
      .reserved2 = 0,
  };

  // Prepare a page buffer (0xFF filled)
  uint8_t page[FLASH_PAGE_SZ];
  for (int i = 0; i < FLASH_PAGE_SZ; ++i)
    page[i] = 0xFF;
  memcpy(page, &s, sizeof(s));

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(SETTINGS_FLASH_OFFSET, FLASH_SECTOR_SZ);
  flash_range_program(SETTINGS_FLASH_OFFSET, page, FLASH_PAGE_SZ);
  restore_interrupts(ints);
}

// FastLED-style LED array
RGB_t leds[WS2812B_LED_SIZE];

// Expose button states and HID RGB colors to RGB effects (single-writer: core 0)
volatile uint16_t g_buttons = 0;
RGB_t hid_rgb[WS2812B_LED_ZONES];

union
{
  struct
  {
    uint8_t buttons[LED_GPIO_SIZE];
    RGB_t rgb[WS2812B_LED_ZONES];
  } lights;
  uint8_t raw[LED_GPIO_SIZE + WS2812B_LED_ZONES * 3];
} lights_report;

/**
 * FastLED-style show function - renders the entire LED array
 **/
void show()
{
  for (int i = 0; i < WS2812B_LED_SIZE; i++)
  {
    // Apply global brightness scaling at output time
    uint8_t r = (uint16_t)leds[i].r * g_brightness / 255;
    uint8_t g = (uint16_t)leds[i].g * g_brightness / 255;
    uint8_t b = (uint16_t)leds[i].b * g_brightness / 255;
    put_pixel(urgb_u32(r, g, b));
  }
}

/**
 * WS2812B Lighting
 * @param counter Current number of WS2812B cycles
 **/
void ws2812b_update(uint32_t counter)
{
  bool hid_mode = time_us_64() - reactive_timeout_timestamp >= REACTIVE_TIMEOUT_MAX;
  ws2812b_mode(counter, hid_mode);
  // Render the entire LED array at once
  show();
}

/**
 * HID/Reactive Lights
 **/
void update_lights()
{
  for (int i = 0; i < LED_GPIO_SIZE; i++)
  {
    if (time_us_64() - reactive_timeout_timestamp >= REACTIVE_TIMEOUT_MAX)
    {
      if (!gpio_get(SW_GPIO[i]))
      {
        gpio_put(LED_GPIO[i], 1);
      }
      else
      {
        gpio_put(LED_GPIO[i], 0);
      }
    }
    else
    {
      // Use HID-provided light state
      gpio_put(LED_GPIO[i], lights_report.lights.buttons[i] ? 1 : 0);
    }
  }
}

struct report
{
  uint16_t buttons;
  uint8_t joy0;
  uint8_t joy1;
} report;

/**
 * Gamepad Mode
 **/
void joy_mode()
{
  if (tud_hid_ready())
  {
    // find the delta between previous and current enc_val
    for (int i = 0; i < ENC_GPIO_SIZE; i++)
    {
      cur_enc_val[i] +=
          ((ENC_REV[i] ? 1 : -1) * (enc_val[i] - prev_enc_val[i]));
      while (cur_enc_val[i] < 0)
        cur_enc_val[i] = ENC_PULSE + cur_enc_val[i];
      cur_enc_val[i] %= ENC_PULSE;

      prev_enc_val[i] = enc_val[i];
    }

    report.joy0 = ((double)cur_enc_val[0] / ENC_PULSE) * (UINT8_MAX + 1);
    report.joy1 = 127;

    tud_hid_n_report(0x00, REPORT_ID_JOYSTICK, &report, sizeof(report));
  }
}

/**
 * Keyboard Mode
 **/
void key_mode()
{
  if (tud_hid_ready())
  { // Wait for ready, updating mouse too fast hampers
    // movement
    if (kbm_report)
    {
      /*------------- Keyboard -------------*/
      uint8_t nkro_report[32] = {0};
      for (int i = 0; i < SW_GPIO_SIZE; i++)
      {
        if ((report.buttons >> i) % 2 == 1)
        {
          uint8_t bit = SW_KEYCODE[i] % 8;
          uint8_t byte = (SW_KEYCODE[i] / 8) + 1;
          if (SW_KEYCODE[i] >= 240 && SW_KEYCODE[i] <= 247)
          {
            nkro_report[0] |= (1 << bit);
          }
          else if (byte > 0 && byte <= 31)
          {
            nkro_report[byte] |= (1 << bit);
          }
        }
      }
      tud_hid_n_report(0x00, REPORT_ID_KEYBOARD, &nkro_report,
                       sizeof(nkro_report));
    }
    else
    {
      /*------------- Mouse -------------*/
      // find the delta between previous and current enc_val
      int delta[ENC_GPIO_SIZE] = {0};
      for (int i = 0; i < ENC_GPIO_SIZE; i++)
      {
        delta[i] = (enc_val[i] - prev_enc_val[i]) * (ENC_REV[i] ? 1 : -1);
        prev_enc_val[i] = enc_val[i];
      }
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta[0] * MOUSE_SENS, 0, 0,
                           0);
    }
    // Alternate reports
    kbm_report = !kbm_report;
  }
}

/**
 * Update Input States
 * Note: Switches are pull up, negate value
 **/
void update_inputs()
{
  for (int i = 0; i < SW_GPIO_SIZE; i++)
  {
    // If switch gets pressed, record timestamp
    if (prev_sw_val[i] == false && !gpio_get(SW_GPIO[i]) == true)
    {
      sw_timestamp[i] = time_us_64();
    }
    prev_sw_val[i] = !gpio_get(SW_GPIO[i]);
  }
}

/**
 * DMA Encoder Logic For 2 Encoders
 **/
void dma_handler()
{
  uint i = 1;
  int interrupt_channel = 0;
  while ((i & dma_hw->ints0) == 0)
  {
    i = i << 1;
    ++interrupt_channel;
  }
  dma_hw->ints0 = 1u << interrupt_channel;
  if (interrupt_channel < 4)
  {
    dma_channel_set_read_addr(interrupt_channel, &pio->rxf[interrupt_channel],
                              true);
  }
}

/**
 * Second Core Runnable
 **/
void core1_entry()
{
  uint32_t counter = 0;
  while (1)
  {
    ws2812b_update(++counter);
    sleep_ms(5);
  }
}

/**
 * Initialize Board Pins
 **/
void init()
{
  // LED Pin on when connected
  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  gpio_put(25, 1);

  // Set up the state machine for encoders
  pio = pio0;
  uint offset = pio_add_program(pio, &encoders_program);

  // Setup Encoders
  for (int i = 0; i < ENC_GPIO_SIZE; i++)
  {
    enc_val[i], prev_enc_val[i], cur_enc_val[i] = 0;
    encoders_program_init(pio, i, offset, ENC_GPIO[i], ENC_DEBOUNCE);

    dma_channel_config c = dma_channel_get_default_config(i);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, i, false));

    dma_channel_configure(i, &c,
                          &enc_val[i],  // Destination pointer
                          &pio->rxf[i], // Source pointer
                          0x10,         // Number of transfers
                          true          // Start immediately
    );
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(i, true);
  }

  reactive_timeout_timestamp = time_us_64();

  // Set up WS2812B
  pio_1 = pio1;
  uint offset2 = pio_add_program(pio_1, &ws2812_program);
  ws2812_program_init(pio_1, ENC_GPIO_SIZE, offset2, WS2812B_GPIO, 800000,
                      false);

  // Setup Button GPIO
  for (int i = 0; i < SW_GPIO_SIZE; i++)
  {
    prev_sw_val[i] = false;
    sw_timestamp[i] = 0;
    gpio_init(SW_GPIO[i]);
    gpio_set_function(SW_GPIO[i], GPIO_FUNC_SIO);
    gpio_set_dir(SW_GPIO[i], GPIO_IN);
    gpio_pull_up(SW_GPIO[i]);
  }

  // Setup LED GPIO
  for (int i = 0; i < LED_GPIO_SIZE; i++)
  {
    gpio_init(LED_GPIO[i]);
    gpio_set_dir(LED_GPIO[i], GPIO_OUT);
  }

  // Set listener bools
  kbm_report = false;

  // Joy/KB Mode Switching
  if (!gpio_get(SW_GPIO[0]))
  {
    loop_mode = &key_mode;
    joy_mode_check = false;
  }
  else
  {
    loop_mode = &joy_mode;
    joy_mode_check = true;
  }

  // Load persisted settings (effect + brightness), allow boot override for Turbocharger
  load_settings();
  set_effect_by_id(current_effect_id);

  // Debouncing Mode
  debounce_mode = &debounce_eager;

  // Disable RGB
  if (gpio_get(SW_GPIO[8]))
  {
    multicore_launch_core1(core1_entry);
  }
}

/**
 * Main Loop Function
 **/
int main(void)
{
  board_init();
  init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    update_inputs();
    report.buttons = debounce_mode();
    g_buttons = report.buttons; // publish to effects
    loop_mode();
    update_lights();

    // Handle deferred reboot to BOOTSEL (triggered by HID Feature command)
    if (g_request_bootsel)
    {
      // Small delay to allow control transfer to finish
      sleep_ms(10);
      // Jump to UF2 bootloader (BOOTSEL)
      reset_usb_boot(0, 0);
      // Should not return; just in case
      while (1)
        tight_loop_contents();
    }
  }

  return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
  // TODO not Implemented
  (void)itf;
  (void)reqlen;

  if (report_id == REPORT_ID_CONFIG && report_type == HID_REPORT_TYPE_FEATURE)
  {
    // Return 8-byte config payload: [cmd=0x00 status, effect_id, ...]
    buffer[0] = 0x00; // status OK
    buffer[1] = current_effect_id;
    buffer[2] = g_brightness;
    for (int i = 3; i < 8; ++i)
      buffer[i] = 0;
    return 8;
  }

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
  (void)itf;
  if (report_id == 2 && report_type == HID_REPORT_TYPE_OUTPUT &&
      bufsize >= sizeof(lights_report)) // light data
  {
    size_t i = 0;
    for (i; i < sizeof(lights_report); i++)
    {
      lights_report.raw[i] = buffer[i];
    }
    // Cache HID RGB colors for effects
    for (int z = 0; z < WS2812B_LED_ZONES; ++z)
    {
      hid_rgb[z] = lights_report.lights.rgb[z];
    }
    reactive_timeout_timestamp = time_us_64();
  }
  else if (report_id == REPORT_ID_CONFIG && report_type == HID_REPORT_TYPE_FEATURE && bufsize >= 1)
  {
    // Feature report command handling
    // Layout: [cmd, arg0, arg1, ...] up to 8 bytes
    uint8_t cmd = buffer[0];
    switch (cmd)
    {
    case 0x01: // SET_EFFECT
      if (bufsize >= 2)
      {
        uint8_t id = buffer[1];
        set_effect_by_id(id);
        save_settings();
      }
      break;
    case 0x02: // SET_BRIGHTNESS
      if (bufsize >= 2)
      {
        g_brightness = buffer[1]; // 0..255
        save_settings();
      }
      break;
    case 0x03: // REBOOT_TO_BOOTSEL
      // Defer actual reboot to main loop to avoid disrupting control transfer
      g_request_bootsel = true;
      break;
    default:
      break;
    }
  }
}
