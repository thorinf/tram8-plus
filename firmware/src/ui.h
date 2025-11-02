#ifndef UI_H
#define UI_H

#include <stdint.h>

#include "hardware_config.h"

#define DEBOUNCE_MS 20     // 20ms debounce time
#define LONG_PRESS_MS 2000 // 2 second long press
#define DEBOUNCE_TICKS (DEBOUNCE_MS / TIMER_TICK_MS)
#define LONG_PRESS_TICKS (LONG_PRESS_MS / TIMER_TICK_MS)

#define LED_BLINK1_TICKS (200 / TIMER_TICK_MS)
#define LED_BLINK2_TICKS (100 / TIMER_TICK_MS)
#define LED_BLINK3_TICKS (50 / TIMER_TICK_MS)

typedef enum {
  BUTTON_IDLE = 0,
  BUTTON_DEBOUNCING,
  BUTTON_DOWN,
  BUTTON_PRESSED,
  BUTTON_HELD,
  BUTTON_HELD_WAIT_RELEASE
} button_state_t;

typedef enum { LED_OFF = 0, LED_ON, LED_BLINK1, LED_BLINK2, LED_BLINK3 } led_state_t;

typedef struct {
  button_state_t state;
  uint16_t ticks;
  uint8_t (*get_reading)(void);
} button_t;

typedef struct {
  led_state_t state;
  uint16_t ticks;
  uint8_t blink_mask;
  void (*on)(void);
  void (*off)(void);
} led_t;

void button_update(button_t *button, uint8_t ticks);
void led_update(led_t *led, uint8_t ticks);
#endif
