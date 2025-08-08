/**
 * Trail effect - creates a trailing light effect based on encoder movement
 * @author Renard
 **/


static uint32_t trail_prev_enc_val = 0;
static float trail_position = 0.0f;
static uint8_t trail_brightness[WS2812B_LED_SIZE] = {0};

void ws2812b_trail(uint32_t counter, bool hid_mode) {
  // Calculate encoder movement
  uint32_t effect_val = hid_mode ? enc_val[0] : counter; // in iidx we only use one encoder
  
  if (hid_mode) {
    // Calculate encoder delta
    int enc_delta = (enc_val[0] - trail_prev_enc_val) * (ENC_REV[0] ? 1 : -1);
    trail_prev_enc_val = enc_val[0];
    
    // Update trail position based on encoder movement
    trail_position += (float)enc_delta / ENC_PULSE * WS2812B_LED_SIZE;
    
    // Keep position within bounds
    while (trail_position < 0) trail_position += WS2812B_LED_SIZE;
    while (trail_position >= WS2812B_LED_SIZE) trail_position -= WS2812B_LED_SIZE;
  } else {
    // Fallback mode: slow automatic movement
    trail_position = (float)(counter % (WS2812B_LED_SIZE * 100)) / 100.0f;
  }
  
  // Decay all trail brightness values
  for (int i = 0; i < WS2812B_LED_SIZE; i++) {
    if (trail_brightness[i] > 0) {
      trail_brightness[i] = trail_brightness[i] > 10 ? trail_brightness[i] - 10 : 0;
    }
  }
  
  // Set brightness at current position
  int pos = (int)trail_position;
  trail_brightness[pos] = 255;
  
  // Apply trail effect to LEDs
  for (int i = 0; i < WS2812B_LED_SIZE; i++) {
    uint8_t brightness = trail_brightness[i];
    
    // Change palette based on counter to demonstrate different palettes
    // This cycles through palettes every ~10 seconds at 200Hz
    int demo_palette = (counter / 2000) % 9; // 9 palettes available (0-8)
    set_color_palette(demo_palette);
    
    // Create a color that shifts through the spectrum
    uint32_t color = color_wheel((i * 768 / WS2812B_LED_SIZE + counter / 4) % 768);
    
    // Apply brightness scaling
    leds[i].r = (((color >> 8) & 0xFF) * brightness) / 255;
    leds[i].g = (((color >> 16) & 0xFF) * brightness) / 255;
    leds[i].b = ((color & 0xFF) * brightness) / 255;
  }
}
// 
