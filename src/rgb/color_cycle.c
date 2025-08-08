/**
 * Color cycle effect
 * @author SpeedyPotato
 **/

void ws2812b_color_cycle(uint32_t counter, bool hid_mode) {
  for (int i = 0; i < WS2812B_LED_SIZE; ++i) {
    uint32_t color = color_wheel((counter + i * (int)(768 / WS2812B_LED_SIZE)) % 768);
    // Extract RGB from color and set in leds array
    leds[i].r = (color >> 8) & 0xFF;   // R is in bits 15-8
    leds[i].g = (color >> 16) & 0xFF;  // G is in bits 23-16
    leds[i].b = color & 0xFF;          // B is in bits 7-0
  }
}