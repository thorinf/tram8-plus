#include "ui.h"

void button_update(button_t *button, uint8_t ticks) {
  uint8_t current_reading = button->get_reading();

  switch (button->state) {
  case BUTTON_IDLE:
    if (current_reading) { button->state = BUTTON_DEBOUNCING; }
    break;

  case BUTTON_DEBOUNCING:
    if (!current_reading) {
      button->state = BUTTON_IDLE;
      button->ticks = 0;
    } else {
      button->ticks += ticks;
      if (button->ticks >= DEBOUNCE_TICKS) { button->state = BUTTON_DOWN; }
    }
    break;

  case BUTTON_DOWN:
    if (!current_reading) {
      button->state = BUTTON_PRESSED;
    } else {
      button->ticks += ticks;
      if (button->ticks >= LONG_PRESS_TICKS) {
        button->state = BUTTON_HELD;
        button->ticks = 0;
      }
    }
    break;

  case BUTTON_PRESSED:
    button->state = BUTTON_IDLE;
    button->ticks = 0;
    break;

  case BUTTON_HELD:
    button->state = BUTTON_HELD_WAIT_RELEASE;
    break;

  case BUTTON_HELD_WAIT_RELEASE:
    if (!current_reading) {
      button->state = BUTTON_IDLE;
      button->ticks = 0;
    }
    break;
  }
}

void led_update(led_t *led, uint8_t ticks) {
  switch (led->state) {
  case LED_ON:
    led->on();
    break;
  case LED_OFF:
    led->off();
    break;
  case LED_BLINK1:
  case LED_BLINK2:
  case LED_BLINK3: {
    const uint16_t blink_rates[] = {LED_BLINK1_TICKS, LED_BLINK2_TICKS, LED_BLINK3_TICKS};
    uint16_t blink_ticks = blink_rates[led->state - LED_BLINK1];

    led->ticks += ticks;
    if (led->ticks >= blink_ticks) {
      led->ticks = 0;

      // check current bit in mask
      if (led->blink_mask & (1 << 7)) {
        led->on();
      } else {
        led->off();
      }

      // shift mask to next bit
      led->blink_mask = (led->blink_mask << 1) | (led->blink_mask >> 7);
    }
    break;
  }
  }
}
